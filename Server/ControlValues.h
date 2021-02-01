//////////////////////////////////////////////////////////////
// Global control values
//////////////////////////////////////////////////////////////
#include <opentherm.h>

#ifndef CONTROLVALUES_H
#define CONTROLVALUES_H

struct ValueNode
{
  unsigned long   timestamp;  // seconds
  byte            id;
  byte            sendHB;
  byte            sendLB;
  byte            recHB;
  byte            recLB;
  uint8           previous;
  uint8           next;
};

#define VALUE_BUFSIZE 50;

class ControlValues
{
  public:
    unsigned long   timestampsec = 0;
    ValueNode       nodes[VALUE_BUFSIZE];
    int             nextFreeNode = 0;
    char            wifiStatus = '-'; // '-' not connected, '+' connected, '#' got IP.
};

#endif // CONTROLVALUES_H
