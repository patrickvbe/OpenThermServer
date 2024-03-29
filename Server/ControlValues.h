//////////////////////////////////////////////////////////////
// Global control values
//////////////////////////////////////////////////////////////
#include "OpenTherm.h"

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
    enum class RequestStatus { Idle, PendingSend, PendingReceive, Received};
    unsigned long   timestampsec = 0;

    // Inserting request from 'outside' the monitor loop (e.g. web/rest/serial)
    unsigned long   insert_time;
    unsigned long   pending_request = 0;
    unsigned long   response = 0;

    RequestStatus   request_status = RequestStatus::Idle;
    ValueNode       nodes[MAX_NODES];
    byte            nextFreeNode = 0;
    byte            head = NO_NODE;
    char            wifiStatus = '-'; // '-' not connected, '+' connected, '#' got IP.

    void ForNodes(std::function<bool(const ValueNode&)>);
    const ValueNode* FindId(byte id);
};

#endif // CONTROLVALUES_H
