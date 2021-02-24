#include "OT.h"

#include <opentherm.h>
#include "PrintString.h"

//////////////////////////////////////////////////////////////
// Pin settings
//////////////////////////////////////////////////////////////
#define THERMOSTAT_IN 5
#define THERMOSTAT_OUT 4
#define BOILER_IN 12
#define BOILER_OUT 13

#define LOG true

OpenthermData message;

#define MODE_LISTEN_MASTER 0
#define MODE_LISTEN_SLAVE 1
#define MODE_LISTEN_SLAVE_LOCAL 2

int mode = 0;

// Allow for serial input to to control the boiler.
#define SERIAL_IDLE 0
#define SERIAL_REGISTER 1
#define SERIAL_VALUE 2
#define SERIAL_WAITING_TO_SEND 3
int serial_status = SERIAL_IDLE;
int serial_value = 0;
int serial_hb = 0;
OpenthermData serial_message;

void LogSend(OpenthermData& msg, ControlValues& ctrl)
{
  ValueNode* pnode;
  byte nodeidx = ctrl.head;
  byte oldestidx = nodeidx;
  // See if we already have a msg with that id, use that.
  while ( nodeidx != NO_NODE )
  {
    pnode = &ctrl.nodes[nodeidx];
    if ( pnode->id == msg.id ) break;
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
  pnode->id = msg.id;
  pnode->sendHB = msg.valueHB;
  pnode->sendLB = msg.valueLB;
  pnode->recHB = 0xFF;
  pnode->recLB = 0xFF;
}

void LogReply(OpenthermData& msg, ControlValues& ctrl)
{
  // Should be the head...
  if ( ctrl.head != NO_NODE )
  {
    ValueNode* pnode = &ctrl.nodes[ctrl.head];
    if ( pnode->id == msg.id )  // Does it match?
    {
      pnode->recHB = msg.valueHB;
      pnode->recLB = msg.valueLB;
    }
  }
}

void OT::Init()
{
  pinMode(THERMOSTAT_IN, INPUT);
  digitalWrite(THERMOSTAT_IN, HIGH); // pull up
  digitalWrite(THERMOSTAT_OUT, HIGH);
  pinMode(THERMOSTAT_OUT, OUTPUT); // low output = high current, high output = low current
  pinMode(BOILER_IN, INPUT);
  digitalWrite(BOILER_IN, HIGH); // pull up
  digitalWrite(BOILER_OUT, HIGH);
  pinMode(BOILER_OUT, OUTPUT); // low output = high voltage, high output = low voltage
}

/**
 * Loop will act as man in the middle connected between Opentherm boiler and Opentherm thermostat.
 * It will listen for requests from thermostat, forward them to boiler and then wait for response from boiler and forward it to thermostat.
 * Requests and response are logged to Serial on the way through the gateway.
 */
void OT::Process(ControlValues& ctrl)
{
  // Allow for serial input to control the boiler.
  if ( Serial.available() > 0 )
  {
    byte serdata = Serial.read();
    if ( serial_status == SERIAL_IDLE )
    {
      if ( serdata == 'r' )
      {
        serial_message.type = OT_MSGTYPE_READ_DATA;
        serial_status = SERIAL_REGISTER;
      }
      else if ( serdata == 'w' )
      {
        serial_message.type = OT_MSGTYPE_WRITE_DATA;
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
          serial_message.id = serial_value;
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
          serial_message.u16(serial_value | serial_hb);
          serial_status = SERIAL_WAITING_TO_SEND;
          serial_value = 0;
          serial_hb = 0;
        }
      }
    }
  }

  // Regular OT gateway logic.
  if (mode == MODE_LISTEN_MASTER) {
    if (OPENTHERM::isSent() || OPENTHERM::isIdle() || OPENTHERM::isError()) {
      OPENTHERM::listen(THERMOSTAT_IN);
    }
    else if (OPENTHERM::getMessage(message)) {
     LogSend(message, ctrl);
     if ( LOG )
      {
        Serial.print("-> ");
        OPENTHERM::printToSerial(message);
        Serial.println();
      }
      OPENTHERM::send(BOILER_OUT, message); // forward message to boiler
      mode = MODE_LISTEN_SLAVE;
    }
    else if ( serial_status == SERIAL_WAITING_TO_SEND )
    {
      // Insert local control. Current implementation may miss thermostat communication.
      LogSend(serial_message, ctrl);
      Serial.print("=> ");
      OPENTHERM::printToSerial(serial_message);
      Serial.println();
      OPENTHERM::send(BOILER_OUT, serial_message);
      mode = MODE_LISTEN_SLAVE_LOCAL;
      serial_status = SERIAL_IDLE;
    }
  }

  else if (mode == MODE_LISTEN_SLAVE || mode == MODE_LISTEN_SLAVE_LOCAL) {
    if (OPENTHERM::isSent()) {
      OPENTHERM::listen(BOILER_IN, 800); // response need to be send back by boiler within 800ms
    }
    else if (OPENTHERM::getMessage(message)) {
      LogReply(message, ctrl);
      if ( LOG || mode == MODE_LISTEN_SLAVE_LOCAL )
      {
        Serial.print("<- ");
        OPENTHERM::printToSerial(message);
        Serial.println();
        Serial.println();
      }
      if (mode == MODE_LISTEN_SLAVE ) OPENTHERM::send(THERMOSTAT_OUT, message); // send message back to thermostat
      mode = MODE_LISTEN_MASTER;
    }
    else if (OPENTHERM::isError()) {
      mode = MODE_LISTEN_MASTER;
      Serial.println("<- Timeout");
      Serial.println();
    }
  }

}
