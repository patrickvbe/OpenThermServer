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
  byte            stype;
  byte            rtype;
  byte            id;
  uint16_t        send;
  uint16_t        rec;
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

    void ForNodes(std::function<bool(const ValueNode&)>);
    const ValueNode* FindId(byte id);
};

#endif // CONTROLVALUES_H
