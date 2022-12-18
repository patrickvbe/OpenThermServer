#include "PrintString.h"

/***************************************************************
 * Format a temperature in 10ths of degrees to a nice string.
 ***************************************************************/
void PrintString::ConcatTemp(int temp, bool usedecimals)
{
  if ( temp > -1000 )
  {
    concat(temp / 10);
    if ( usedecimals )
    {
      concat(".");
      concat(temp % 10);
    }
  } 
  else
  {
    if ( usedecimals ) concat("--.-");
    else concat("--");
  }
} 

void PrintTime(unsigned long seconds, Print& target)
{
  unsigned long minutes = seconds / 60;
  seconds %= 60;
  unsigned long hours = minutes / 60;
  minutes %= 60;
  unsigned long days = hours / 24;
  hours %= 24;
  if ( days > 0 )
  {
    target.print(days);
    target.print("d ");
  }
  if ( days > 0 || hours > 0 )
  {
    target.print(hours);
    target.print(':');
    if ( minutes < 10 ) target.print('0');
  }
  target.print(minutes);
  target.print(':');
  if ( seconds < 10 ) target.print('0');
  target.print(seconds);
}
