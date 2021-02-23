//////////////////////////////////////////////////////////////
// Global control values
//////////////////////////////////////////////////////////////
#include <opentherm.h>

#ifndef CONTROLVALUES_H
#define CONTROLVALUES_H

#define NO_NODE 0xFF

struct ValueNode
{
  unsigned long   timestamp;  // seconds
  byte            id;
  byte            sendHB;
  byte            sendLB;
  byte            recHB;
  byte            recLB;
  byte            previous;
  byte            next;
};

#define MAX_NODES 50

class ControlValues
{
  public:
    unsigned long   timestampsec = 0;
    ValueNode       nodes[MAX_NODES];
    byte            nextFreeNode = 0;
    byte            head = NO_NODE;
    char            wifiStatus = '-'; // '-' not connected, '+' connected, '#' got IP.
};

#endif // CONTROLVALUES_H
