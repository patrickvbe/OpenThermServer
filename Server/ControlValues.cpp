#include "ControlValues.h"

void ControlValues::ForNodes(std::function<bool(const ValueNode&)> func)
{
  bool keepgoing = true;
  byte nodeidx = head;
  while ( keepgoing && nodeidx != NO_NODE )
  {
    const ValueNode* pnode = &nodes[nodeidx];
    keepgoing = func(*pnode);
    nodeidx = pnode->next;
  }
}

const ValueNode* ControlValues::FindId(byte id)
{
  const ValueNode* presult = nullptr;
  ForNodes([&](const ValueNode& node)
  {
    if ( node.id == id )
    {
      presult = &node;
      return false;
    }
    return true;
  });
  return presult;
}

