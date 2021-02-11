#include "opentherm.h"
#include "Arduino.h"

#define STOP_BIT_POS 33

// We define a few ISRs to use. After that, it's over... But you can add more, if you need more.
static OPENTHERM* ISR1this = nullptr;
static void ICACHE_RAM_ATTR ISR1()
{
  ISR1this->_inputISR();
}
static OPENTHERM* ISR2this = nullptr;
static void ICACHE_RAM_ATTR ISR2()
{
  ISR2this->_inputISR();
}

OPENTHERM::OPENTHERM(byte spin, byte rpin, OpenThermMode mode)
: _status(OpenThermStatus::IDLE),
  _active(false),
  _mode(mode),
  send_pin(spin),
  rec_pin(rpin)
{
  if ( ISR1this == nullptr )
  {
    _isr = &ISR1this;
    ISR1this = this;
 		attachInterrupt(digitalPinToInterrupt(rec_pin), ISR1, CHANGE);
  }
  if ( ISR2this == nullptr )
  {
    _isr = &ISR2this;
    ISR2this = this;
 		attachInterrupt(digitalPinToInterrupt(rec_pin), ISR2, CHANGE);
  }
}

OPENTHERM::~OPENTHERM(byte send_pin, byte rec_pin)
{
  if ( _isr ) *_isr = nullptr;  // Free the ISR
}

///////////////////////////////////////////////////////////////////
// Code for sending
///////////////////////////////////////////////////////////////////

void OPENTHERM::send(byte pin, OpenthermData &data, void (*callback)())
{
  _callback = callback;
  _data = data.Get();
  _clock = 1; // clock starts at HIGH
  _bitPos = 33; // count down (33 == start bit, 32-1 data, 0 == stop bit)
  _status = OpenThermStatus::SENDING;

  _active = true;
  _startWriteTimer();
}

void OPENTHERM::stop() {
  _stop();
  _status = OpenThermStatus::IDLE;
}

void OPENTHERM::_stop() {
  if (_active) {
    _stopTimer();
    _active = false;
  }
}

void OPENTHERM::_timerISR() {
  if (_status == OpenThermStatus::SENDING) {
    // write data to pin
    if (_bitPos == 33 || _bitPos == 0)  { // start bit
      _writeBit(1, _clock);
    }
    else { // data bits
      _writeBit(bitRead(_data, _bitPos - 1), _clock);
    }
    if (_clock == 0) {
      if (_bitPos <= 0) { // check termination, all sent
        _status = _mode == OpenThermMode::MASTER ? OpenThermStatus::LISTEN : OpenThermStatus::IDLE;
        sentTimestamp = micros();
        _stop();
        _callCallback();
      }
      _bitPos--;
      _clock = 1;
    }
    else {
      _clock = 0;
    }
  }
}

void OPENTHERM::_writeBit(byte high, byte clock) {
  if (clock == 1) { // left part of manchester encoding
    digitalWrite(send_pin, !high); // low means logical 1 to protocol
  }
  else { // right part of manchester encoding
    digitalWrite(send_pin, high); // high means logical 0 to protocol
  }
}

bool OPENTHERM::isIdle() {
  return _status == OpenThermStatus::IDLE;
}

bool OPENTHERM::isError() {
  return _status == OpenThermStatus::TIMEOUT || _status == OpenThermStatus::RECEIVED_INVALID;
}

void OPENTHERM::_callCallback() {
  if (_callback != NULL) {
    _callback();
    _callback = NULL;
  }
}

#ifdef AVR
ISR(TIMER2_COMPA_vect) { // Timer2 interrupt
  OPENTHERM::_timerISR();
}

// 2 kHz timer
void OPENTHERM::_startWriteTimer() {
  cli();
  TCCR2A = 0; // set entire TCCR2A register to 0
  TCCR2B = 0; // same for TCCR2B
  TCNT2  = 0; //initialize counter value to 0
  // set compare match register for 2080Hz increments (2kHz to do transition in the middle of the bit)
  OCR2A = 252;// = (16*10^6) / (2080*32) - 1 (must be <256)
  TCCR2A |= (1 << WGM21); // turn on CTC mode
  TCCR2B |= (1 << CS21) | (1 << CS20); // Set CS21 & CS20 bit for 32 prescaler
  TIMSK2 |= (1 << OCIE2A); // enable timer compare interrupt
  sei();
}

void OPENTHERM::_stopTimer() {
  cli();
  TIMSK2 = 0;
  sei();
}
#endif // END AVR

#ifdef ESP8266
// 2 kHz timer
void OPENTHERM::_startWriteTimer() {
  noInterrupts();
  timer1_attachInterrupt(OPENTHERM::_timerISR);
  timer1_enable(TIM_DIV16, TIM_EDGE, TIM_LOOP); // 5MHz (5 ticks/us - 1677721.4 us max)
  timer1_write(2500); // 2kHz
  interrupts();
}

void OPENTHERM::_stopTimer() {
  noInterrupts();
  timer1_disable();
  timer1_detachInterrupt();
  interrupts();
}
#endif // END ESP8266

///////////////////////////////////////////////////////////////////
// Code for reading
///////////////////////////////////////////////////////////////////

void ICACHE_RAM_ATTR OPENTHERM::_inputISR()
{
	unsigned long newTs = micros();
	if ( _status == OpenThermStatus::LISTENING )
		if (digitalRead(rec_pin) == HIGH) {
			_status == OpenThermStatus::START_BIT;
			readTimestamp = newTs;
		}
		else {
			_status = _mode == OpenThermMode::MASTER ? OpenThermStatus::RECEIVED_INVALID : OpenThermStatus::LISTENING;
			readTimestamp = newTs;
		}
	}
	else if (_status == OpenThermStatus::START_BIT) {
		if ((newTs - readTimestamp < 750) && digitalRead(rec_pin) == LOW) {
			_status = OpenThermStatus::RECEIVING;
			readTimestamp = newTs;
			readBitIndex = 0;
      _data = 0;
		}
		else {
			_status = _mode == OpenThermMode::MASTER ? OpenThermStatus::RECEIVED_INVALID : OpenThermStatus::LISTENING;
			readTimestamp = newTs;
		}
	}
	else if (_status == OpenThermStatus::RECEIVING) {
		if ((newTs - readTimestamp) > 750) {
			if (readBitIndex < 32) {
				_data = (_data << 1) | !digitalRead(rec_pin);
				readTimestamp = newTs;
				readBitIndex++;
			}
			else { //stop bit
				_status = OpenThermStatus::RECEIVED;
				readTimestamp = newTs;
			}
		}
	}
}

///////////////////////////////////////////////////////////////////
// Generic helper funtions
///////////////////////////////////////////////////////////////////

void OpenTherm::Process()
{
	if (st == OpenThermStatus::READY || st == OpenThermStatus::) return;

  unsigned long newTs = micros();

  if ( _mode == OpenThermMode::MASTER )
  {
    if ( _status > OpenThermStatus::DONE_STATUSSUS ) return;
    if ( _status == OpenThermStatus::RECEIVED )
    {
      _message.Set(_data);
      _status = _message.isValidResponse() ? OpenThermStatus::RECEIVED_VALID : OpenThermStatus::RECEIVED_INVALID;
    }
    else if ( newTs - sentTimestamp > 8000 )
    {
      _status = OpenThermStatus::TIMEOUT;
    }
  }
  else
  { // slave
    if ( _status == OpenThermStatus::RECEIVED_VALID ) return;
    if ( _status == OpenThermStatus::RECEIVED )
    {
      _message.Set(_data);
      _status = _message.isValidRequest() ? OpenThermStatus::RECEIVED_VALID : OpenThermStatus::LISTENING;
    }
    else if ( st != OpenThermStatus::LISTENING && newTs - readTimestamp > 400000 )
    {
      _status = OpenThermStatus::LISTENING; // Timeout receiving message.
    }
  }

	if (st != OpenThermStatus::NOT_INITIALIZED && (newTs - ts) > 1000000) {
		status = OpenThermStatus::READY;
		_dataStatus = OpenThermResponseStatus::TIMEOUT;
		if (processResponseCallback != NULL) {
			processResponseCallback(_data, _dataStatus);
		}
	}
	else if (st == OpenThermStatus::RESPONSE_RECEIVED_INVALID) {
		status = OpenThermStatus::DELAY;
		_dataStatus = OpenThermResponseStatus::RECEIVED_INVALID;
		if (processResponseCallback != NULL) {
			processResponseCallback(_data, _dataStatus);
		}
	}
	else if (st == OpenThermStatus::RESPONSE_READY) {
    _message.Set(_data);
		if ( _mode == OpenThermMode::SLAVE ) _status = _message.isValidRequest() ? OpenThermStatus::RECEIVED_VALID : OpenThermStatus::LISTENING;
    else _status = _message.isValidResponse()) ? OpenThermStatus::RECEIVED_VALID : OpenThermStatus::RECEIVED_INVALID;
		if (processResponseCallback != NULL) {
			processResponseCallback(_data, _dataStatus);
		}
	}
	else if (st == OpenThermStatus::DELAY) {
		if ((newTs - ts) > 100000) {
			status = OpenThermStatus::READY;
		}
	}
}

// https://stackoverflow.com/questions/21617970/how-to-check-if-value-has-even-parity-of-bits-or-odd
bool OPENTHERM::_checkParity(unsigned long val) {
  val ^= val >> 16;
  val ^= val >> 8;
  val ^= val >> 4;
  val ^= val >> 2;
  val ^= val >> 1;
  return (~val) & 1;
}

void OPENTHERM::printToSerial(OpenthermData &data) {
  if (data.type == OT_MSGTYPE_READ_DATA) {
    Serial.print("ReadData");
  }
  else if (data.type == OT_MSGTYPE_READ_ACK) {
    Serial.print("ReadAck");
  }
  else if (data.type == OT_MSGTYPE_WRITE_DATA) {
    Serial.print("WriteData");
  }
  else if (data.type == OT_MSGTYPE_WRITE_ACK) {
    Serial.print("WriteAck");
  }
  else if (data.type == OT_MSGTYPE_RECEIVED_INVALID_DATA) {
    Serial.print("InvalidData");
  }
  else if (data.type == OT_MSGTYPE_DATA_RECEIVED_INVALID) {
    Serial.print("DataInvalid");
  }
  else if (data.type == OT_MSGTYPE_UNKNOWN_DATAID) {
    Serial.print("UnknownId");
  }
  else {
    Serial.print(data.type, BIN);
  }
  Serial.print(" ");
  Serial.print(data.id);
  Serial.print(" ");
  Serial.print(data.valueHB, HEX);
  Serial.print(" ");
  Serial.print(data.valueLB, HEX);
}

unsigned long OpenthermData::Get()
{
  unsigned long data = type;
  data = (data << 12) | id;
  data = (data << 8) | valueHB;
  data = (data << 8) | valueLB;
  if (!_checkParity(_data)) {
    _data = _data | 0x80000000;
  }
}

void OpenthermData::Set(unsigned long data)
{
	if (_checkParity(data))
  {
    data.type = OT_MSGTYPE_RECEIVED_INVALID_MSG;
  }
  else
  {
    type = (data >> 28) & 0x7;
    id = (data >> 16) & 0xFF;
    valueHB = (data >> 8) & 0xFF;
    valueLB = data & 0xFF;
  }
}

bool OpenTherm::isValidResponse(unsigned long response)
{
	return type == OT_MSGTYPE_READ_ACK || type == OT_MSGTYPE_WRITE_ACK;
}

bool OpenTherm::isValidRequest(unsigned long request)
{
	return type == OT_MSGTYPE_READ_DATA || type == OT_MSGTYPE_WRITE_DATA;
}


float OpenthermData::f88() {
  float value = (int8_t) valueHB;
  return value + (float)valueLB / 256.0;
}

void OpenthermData::f88(float value) {
  if (value >= 0) {
    valueHB = (byte) value;
    float fraction = (value - valueHB);
    valueLB = fraction * 256.0;
  }
  else {
    valueHB = (byte)(value - 1);
    float fraction = (value - valueHB - 1);
    valueLB = fraction * 256.0;
  }
}

uint16_t OpenthermData::u16() {
  uint16_t value = valueHB;
  return (value << 8) | valueLB;
}

void OpenthermData::u16(uint16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

int16_t OpenthermData::s16() {
  int16_t value = valueHB;
  return (value << 8) | valueLB;
}

void OpenthermData::s16(int16_t value) {
  valueLB = value & 0xFF;
  valueHB = (value >> 8) & 0xFF;
}

bool OPENTHERM::getMessage(OpenthermData &data) {
  if (_mode == MODE_RECEIVED) {
    data.type = (_data >> 28) & 0x7;
    data.id = (_data >> 16) & 0xFF;
    data.valueHB = (_data >> 8) & 0xFF;
    data.valueLB = _data & 0xFF;
    return true;
  }
  return false;
}
