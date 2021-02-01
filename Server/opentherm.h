#ifndef OPENTHERM_H
#define OPENTHERM_H

#include "Arduino.h"

// Messages types
#define OT_MSGTYPE_READ_DATA          B000
#define OT_MSGTYPE_READ_ACK           B100
#define OT_MSGTYPE_WRITE_DATA         B001
#define OT_MSGTYPE_WRITE_ACK          B101
#define OT_MSGTYPE_INVALID_DATA       B010
#define OT_MSGTYPE_DATA_INVALID       B110
#define OT_MSGTYPE_UNKNOWN_DATAID     B111

// Message IDs
#define OT_MSGID_STATUS               0
#define OT_MSGID_CH_SETPOINT          1
#define OT_MSGID_MASTER_CONFIG        2
#define OT_MSGID_SLAVE_CONFIG         3
#define OT_MSGID_COMMAND_CODE         4
#define OT_MSGID_FAULT_FLAGS          5
#define OT_MSGID_REMOTE               6
#define OT_MSGID_COOLING_CONTROL      7
#define OT_MSGID_CONTROL_SETPOINT_CH2 8
#define OT_MSGID_CH_SETPOINT_OVERRIDE 9
#define OT_MSGID_ROOM_SETPOINT        16
#define OT_MSGID_MODULATION_LEVEL     17
#define OT_MSGID_CH_WATER_PRESSURE    18
#define OT_MSGID_DHW_FLOW_RATE        19
#define OT_MSGID_DAY_TIME             20
#define OT_MSGID_DATE                 21
#define OT_MSGID_YEAR                 22
#define OT_MSGID_ROOM_SETPOINT_CH2    23
#define OT_MSGID_ROOM_TEMP            24
#define OT_MSGID_FEED_TEMP            25
#define OT_MSGID_DHW_TEMP             26
#define OT_MSGID_OUTSIDE_TEMP         27
#define OT_MSGID_RETURN_WATER_TEMP    28
#define OT_MSGID_SOLAR_STORE_TEMP     29
#define OT_MSGID_SOLAR_COLLECT_TEMP   30
#define OT_MSGID_FEED_TEMP_CH2        31
#define OT_MSGID_DHW2_TEMP            32
#define OT_MSGID_EXHAUST_TEMP         33
#define OT_MSGID_DHW_BOUNDS           48
#define OT_MSGID_CH_BOUNDS            49
#define OT_MSGID_DHW_SETPOINT         56
#define OT_MSGID_MAX_CH_SETPOINT      57
#define OT_MSGID_OVERRIDE_FUNC        100
#define OT_MSGID_BURNER_STARTS        116
#define OT_MSGID_CH_PUMP_STARTS       117
#define OT_MSGID_DHW_PUMP_STARTS      118
#define OT_MSGID_DHW_BURNER_STARTS    119
#define OT_MSGID_BURNER_HOURS         120
#define OT_MSGID_CH_PUMP_HOURS        121
#define OT_MSGID_DHW_PUMP_HOURS       122
#define OT_MSGID_DHW_BURNER_HOURS     123
#define OT_MSGID_OT_VERSION_MASTER    124
#define OT_MSGID_OT_VERSION_SLAVE     125
#define OT_MSGID_VERSION_MASTER       128
#define OT_MSGID_VERSION_SLAVE        127

/**
 * Structure to hold Opentherm data packet content.
 * Use f88(), u16() or s16() functions to get appropriate value of data packet accoridng to id of message.
 */
struct OpenthermData {
  byte type;
  byte id;
  byte valueHB;
  byte valueLB;

  /**
   * @return float representation of data packet value
   */
  float f88();

  /**
   * @param float number to set as value of this data packet
   */
  void f88(float value);

  /**
   * @return unsigned 16b integer representation of data packet value
   */
  uint16_t u16();

  /**
   * @param unsigned 16b integer number to set as value of this data packet
   */
  void u16(uint16_t value);

  /**
   * @return signed 16b integer representation of data packet value
   */
  int16_t s16();

  /**
   * @param signed 16b integer number to set as value of this data packet
   */
  void s16(int16_t value);
};

/**
 * Opentherm static class that supports parallel listening and sending
 * and multiple OpenTherm interfaces in the same project.
 * With no waiting / delays.
 */
class OPENTHERM {
  public:
    OPENTHERM(byte send_pin, byte rec_pin);

    /** Immediately send out Opentherm data packet to line connected on given pin.
     * Completed data transfer is indicated by isSent() function.
     * Error state is indicated by isError() function.
     * @param pin digital pin number to send data on.
     * @param data Opentherm data packet.
     * @param callback if provided, callback function is called once data packet is sent.
     */
    void send(byte pin, OpenthermData &data, void (*callback)() = NULL);

    /** Use this function to check whether send() function already finished sending data packed to line.
     * @return true if data packet has been sent, false otherwise.
     */
    bool isSent();

    /** Indicates whether last listen() or send() operation ends up with an error.
     * @return true if last listen() or send() operation ends up with an error.
     */
    bool isError();

    /** Helper function to debug content of data packet.
     * It will print whatevet is in given data packet to Serial as formatted string.
     * @param data data packet to print to serial
     */
    void printToSerial(OpenthermData &data);

#ifdef AVR
    static void _timerISR(); // this function needs to be public since its attached as interrupt handler
#endif // END ESP8266
#ifdef ESP8266
    static void ICACHE_RAM_ATTR _timerISR(); // this function needs to be public since its attached as interrupt handler
#endif // END ESP8266

  private:  
    byte send_pin;
    void (*_callback)();
    volatile unsigned long _data;
    volatile byte _clock;
    volatile byte _bitPos;
    volatile byte _mode;
    volatile bool _active;

    void _startWriteTimer(); // writing timer to send manchester code (at 2kHz)
    void _stopTimer();
    bool _checkParity(unsigned long val);
    void _writeBit(byte high, byte pos);
    void _callCallback();
};

#endif
