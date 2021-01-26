
#include <WString.h>
#include <Print.h>

/***************************************************************
 * String formatting class, compatible with the Print interface
 ***************************************************************/
class PrintString : public Print, public String
{
public:
  PrintString(const char* s = nullptr) : Print(), String(s) {}
  PrintString(const String &str) : Print(), String(str) {}
  explicit PrintString(char c) : Print(), String(c) {}

  PrintString& operator=(const PrintString &rhs) { String::operator=(rhs); return *this;}
  PrintString& operator=(char c) { String::operator=(c);  return *this;}
  PrintString& operator=(const char *cstr) { String::operator=(cstr);  return *this;}
  PrintString& operator=(const __FlashStringHelper *str)  { String::operator=(str);  return *this;}

  virtual size_t write(uint8_t c) { concat((char)c); }
  virtual size_t write(const uint8_t *buffer, size_t size)
  {
    reserve(length() + size + 1);
    while(size--) concat((char)*buffer++);
  }
  virtual int availableForWrite() { return 100; } // Whatever??
  void ConcatTemp(int temp, bool usedecimals=true);
  void ConcatTime(unsigned long time);
};

