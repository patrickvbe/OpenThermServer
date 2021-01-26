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

void PrintString::ConcatTime(unsigned long time)
{
  time /= 1000;
  unsigned long hours = time / 3600;
  unsigned long minutes = time % 3600;
  unsigned long seconds = minutes % 60;
  minutes /= 60;
  if ( hours > 0 )
  {
    concat(hours);
    concat(':');
    if ( minutes < 10 ) concat('0');
  }
  concat(minutes);
  concat(':');
  if ( seconds < 10 ) concat('0');
  concat(seconds);
} 
