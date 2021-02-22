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

#define LOG false

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
  // See if we already have a msg with that id
  if ( ctrl.head != NO_NODE )
  {
    
  }
}

void LogReply(OpenthermData& msg, ControlValues& ctrl)
{

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
      // Insert local control. Current implementation may miss thermostat communication.
      if ( serial_status == SERIAL_WAITING_TO_SEND )
      {
        LogSend(serial_message, ctrl);
        Serial.print("=> ");
        OPENTHERM::printToSerial(serial_message);
        Serial.println();
        OPENTHERM::send(BOILER_OUT, serial_message);
        mode = MODE_LISTEN_SLAVE_LOCAL;
        serial_status = SERIAL_IDLE;
      }
      else OPENTHERM::listen(THERMOSTAT_IN);
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
