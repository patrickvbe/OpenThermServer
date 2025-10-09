//////////////////////////////////////////////////////////////
// Global control values
//////////////////////////////////////////////////////////////
#include "OpenTherm.h"

#ifndef CONTROLVALUES_H
#define CONTROLVALUES_H

#define NO_NODE 0xFF
#define INVALID_TEMP -1000

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

    // Inserting request from 'outside' the monitor loop (e.g. web/rest/serial)
    bool            insert_pending = false;
    unsigned long   insert_request = 0;

    // When a request is received from the master while we already communicate to the slave,
    // flag this here and save the request. When we received a response from the slave, we will
    // foreward this message to the slave as normal.
    bool            request_pending = false;
    unsigned long   pending_request;

    // Linked list of historical values send / received.
    ValueNode       nodes[MAX_NODES];
    byte            nextFreeNode = 0;
    byte            head = NO_NODE;

    // MQTT values
    int16_t         tset_reported = INVALID_TEMP;
    int16_t         tset_received = INVALID_TEMP;
    int16_t         troom_reported = INVALID_TEMP;
    int16_t         troom_received = INVALID_TEMP;
    int16_t         modlevel_reported = -1;
    int16_t         modlevel_received = 0;
    int16_t         toffset = 0;

    char            wifiStatus = '-'; // '-' not connected, '+' connected, '#' got IP.

    void ForNodes(std::function<bool(const ValueNode&)>);
    const ValueNode* FindId(byte id);
};

#endif // CONTROLVALUES_H
