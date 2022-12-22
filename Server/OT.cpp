#include "core_esp8266_features.h"
#include "OT.h"
#include "ControlValues.h"
#include "PrintString.h"
#include "OpenTherm.h"

//////////////////////////////////////////////////////////////
// Pin settings
//////////////////////////////////////////////////////////////
#define THERMOSTAT_IN 5
#define THERMOSTAT_OUT 4
#define BOILER_IN 12
#define BOILER_OUT 13

OpenTherm mOT(BOILER_IN, BOILER_OUT);
OpenTherm sOT(THERMOSTAT_IN, THERMOSTAT_OUT, true);

#define LOG false

#define MODE_LISTEN_MASTER 0
#define MODE_LISTEN_SLAVE 1
#define MODE_LISTEN_SLAVE_LOCAL 2

int mode = 0;

// Reboot when no request / response for the given timeout: 1000ms * 60s * 5min = 300000
#define REBOOT_TIMEOUT 300000
unsigned long lastrequest = millis();
unsigned long lastresponse = lastrequest;
unsigned long lastlistenmaster = lastrequest;

// Allow for serial input to to control the boiler.
#define SERIAL_IDLE 0
#define SERIAL_REGISTER 1
#define SERIAL_VALUE 2
#define SERIAL_WAITING_TO_SEND 3
int serial_status = SERIAL_IDLE;
unsigned int serial_value = 0;
int serial_hb = 0;
OpenThermMessageType  serial_type;
OpenThermMessageID    serial_id;
unsigned int          serial_data;
ControlValues*        pctrl;

void LogRequest(unsigned long msg, ControlValues& ctrl)
{
  auto msg_id = OpenTherm::getDataID(msg);
  ValueNode* pnode;
  byte nodeidx = ctrl.head;
  byte oldestidx = nodeidx;
  // See if we already have a msg with that id, use that.
  while ( nodeidx != NO_NODE )
  {
    pnode = &ctrl.nodes[nodeidx];
    if ( pnode->id == msg_id ) break;
    oldestidx = nodeidx;
    nodeidx = pnode->next;
  }
  if ( nodeidx == NO_NODE ) // ID not found
  {
    // Do we still have nodes left?
    if ( ctrl.nextFreeNode < MAX_NODES )
    {
      nodeidx = ctrl.nextFreeNode++;
      pnode = &ctrl.nodes[nodeidx];
      pnode->next = NO_NODE;
      pnode->previous = NO_NODE;
    }
    else // Nothing left, use the oldest.
    {
      nodeidx = oldestidx;
      pnode = &ctrl.nodes[nodeidx];
    }
  }
  // Detach node from the chain.
  if ( pnode->next     != NO_NODE ) ctrl.nodes[pnode->next].previous = pnode->previous;
  if ( pnode->previous != NO_NODE ) ctrl.nodes[pnode->previous].next = pnode->next;
  // Attach node to the head.
  if ( ctrl.head != NO_NODE ) ctrl.nodes[ctrl.head].previous = nodeidx;
  pnode->previous = NO_NODE;
  pnode->next = ctrl.head;
  ctrl.head = nodeidx;
  // Put values in the node.
  pnode->timestamp = ctrl.timestampsec;
  pnode->stype = OpenTherm::getMessageType(msg);
  pnode->rtype = 0xFF;
  pnode->id = msg_id;
  pnode->send = OpenTherm::getUInt(msg);
  pnode->rec = 0xFFFF;
}

void LogResponse(unsigned long msg, ControlValues& ctrl)
{
  // Should be the head...
  if ( ctrl.head != NO_NODE )
  {
    ValueNode* pnode = &ctrl.nodes[ctrl.head];
    if ( pnode->id == OpenTherm::getDataID(msg) )  // Does it match?
    {
      pnode->rtype = OpenTherm::getMessageType(msg);
      pnode->rec = OpenTherm::getUInt(msg);
    }
  }
}

void LogMessage(unsigned long message, const char* prefix)
{
  if ( LOG || mode == MODE_LISTEN_SLAVE_LOCAL )
  {
    Serial.print(prefix);
    Serial.print(OpenTherm::messageTypeToString(OpenTherm::getMessageType(message)));
    Serial.print(" ");
    Serial.print(OpenTherm::getDataID(message));
    Serial.print(" ");
    Serial.println(OpenTherm::getUInt(message), HEX);
  }
}

void IRAM_ATTR mHandleInterrupt()
{
    mOT.handleInterrupt();
}

void IRAM_ATTR sHandleInterrupt()
{
    sOT.handleInterrupt();
}

void processRequest(unsigned long request, OpenThermResponseStatus status)
{
  if ( status == OpenThermResponseStatus::SUCCESS )
  {
    lastrequest = millis();
    LogMessage(request, "-> ");
    if ( mode != MODE_LISTEN_MASTER )
    {
      if ( LOG ) Serial.println("Request received while local request pending.");
    }
    else
    {
      LogRequest(request, *pctrl);
      mOT.sendRequestAync(request);
      mode = MODE_LISTEN_SLAVE;
    }
  }
  else if (LOG) Serial.println("Invalid request");
}

void processResponse(unsigned long response, OpenThermResponseStatus status) {
  if ( status == OpenThermResponseStatus::SUCCESS )
  {
    lastresponse = millis();
    LogMessage(response, "<- ");
    LogResponse(response, *pctrl);
    if ( mode == MODE_LISTEN_SLAVE ) sOT.sendResponse(response);
    else if ( mode == MODE_LISTEN_SLAVE_LOCAL &&  pctrl->insert_status == ControlValues::InsertStatus::PendingReceive )
    {
      pctrl->insert_response = response;
      pctrl->insert_status = ControlValues::InsertStatus::Received;
    }
  }
  else
  {
    if (LOG) Serial.println(OpenTherm::statusToString(status));
  }
  mode = MODE_LISTEN_MASTER;
  lastlistenmaster = millis();
}

void OT::Init(ControlValues& ctrl)
{
  pctrl = &ctrl;
  mOT.begin(mHandleInterrupt, processResponse);
  sOT.begin(sHandleInterrupt, processRequest);
}

/**
 * Loop will act as man in the middle connected between Opentherm boiler and Opentherm thermostat.
 * It will listen for requests from thermostat, forward them to boiler and then wait for response from boiler and forward it to thermostat.
 * Requests and response are logged to Serial on the way through the gateway.
 */
void OT::Process()
{
  // Allow for serial input to control the boiler.
  if ( Serial.available() > 0 )
  {
    byte serdata = Serial.read();
    if ( serial_status == SERIAL_IDLE )
    {
      if ( serdata == 'r' )
      {
        serial_type = OpenThermMessageType::READ;
        serial_status = SERIAL_REGISTER;
      }
      else if ( serdata == 'w' )
      {
        serial_type = OpenThermMessageType::WRITE;
        serial_status = SERIAL_REGISTER;
      }
    }
    else if ( serial_status != SERIAL_WAITING_TO_SEND )
    {
      if( isDigit(serdata) )
      {
        serial_value *= 10;
        serial_value += serdata - '0';
      }
      else
      {
        if ( serial_status == SERIAL_REGISTER )
        {
          serial_id = (OpenThermMessageID)serial_value;
          serial_value = 0;
          serial_status = SERIAL_VALUE;
        }
        if ( serdata == '/' )
        {
          serial_hb = serial_value << 8;
          serial_value = 0;
        }
        if ( serdata == '\n' )
        {
          serial_data = serial_value | serial_hb;
          serial_status = SERIAL_WAITING_TO_SEND;
          serial_value = 0;
          serial_hb = 0;
        }
      }
    }
  }

  // Regular OT gateway logic.
  sOT.process();
  mOT.process();
  if ( pctrl->insert_status == ControlValues::InsertStatus::PendingSend && mOT.isReady() )
  {
    mode = MODE_LISTEN_SLAVE_LOCAL;
    pctrl->insert_status = ControlValues::InsertStatus::PendingReceive;
    LogRequest(pctrl->insert_request, *pctrl);
    LogMessage(pctrl->insert_request, "#> ");
    mOT.sendRequestAync(pctrl->insert_request);
  }
  if ( serial_status == SERIAL_WAITING_TO_SEND && mOT.isReady() )
  {
    mode = MODE_LISTEN_SLAVE_LOCAL;
    unsigned long message = OpenTherm::buildRequest(serial_type, serial_id, serial_data);
    LogMessage(message, "=> ");
    mOT.sendRequestAync(message);
    serial_status = SERIAL_IDLE;
  }

  // Reset the message loop / mode if we did not get back into 'idle' within 1s.
  auto timestamp = millis();
  if ( timestamp - lastlistenmaster > 1000 )
  {
    mode = MODE_LISTEN_MASTER;
  }
  if ( mode == MODE_LISTEN_MASTER ) lastlistenmaster = timestamp;

  // Reboot when no messages have been process for REBOOT_TIMEOUT ms.  
  if ( (timestamp - lastrequest) > REBOOT_TIMEOUT || (timestamp - lastresponse) > REBOOT_TIMEOUT )
  {
    ESP.restart();
  }
  
}
