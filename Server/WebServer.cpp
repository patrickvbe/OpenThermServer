#include "WebServer.h"

#include <ESP8266WebServer.h>
#include "PrintString.h"

static ESP8266WebServer server(80);
WebServer webserver;

static const char* pageheader = "<html><head><style>body { zoom: 3; font-family: Arial;} .head { background-color: lightblue; text-align: center; font-size: 40px;} .T { font-size: 100px;}</style></head><body>";
static const char* pagefooter = "</body></html>";

void WebServer::Init(ControlValues& ctrl)
{
  MPCtrl = &ctrl;
  server.on("/", []() {
    webserver.ServeRoot();
  });
  server.onNotFound([]() {
    webserver.ServeNotFound();
  });
  server.begin();
}

void WebServer::ServeRoot()
{
  PrintString result(pageheader);
  result += "<div class=\"head\">Historie</div><br>time: ";
  result.print(MPCtrl->timestampsec);
  result += "<br><table><tr><th>tijd</th><th>id</th><th>send H L</th><th>rec H L</th></tr>";
  ValueNode* pnode;
  byte nodeidx = MPCtrl->head;
  while ( nodeidx != NO_NODE )
  {
    pnode = &MPCtrl->nodes[nodeidx];
    result += "<tr><td>";
    result.print(pnode->timestamp);
    result += "</td><td>";
    result.print(pnode->id);
    result += "</td><td>";
    result.print(pnode->sendHB, HEX);
    result += " ";
    result.print(pnode->sendLB, HEX);
    result += "</td><td>";
    result.print(pnode->recHB, HEX);
    result += " ";
    result.print(pnode->recLB, HEX);
    result += "</td></tr>";
    nodeidx = pnode->next;
  }
  result += "</table>";
  result += pagefooter;
  server.send(200, "text/html", result);
}

void WebServer::ServeNotFound()
{
  String message = "Page Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void WebServer::Process()
{
  server.handleClient();
}
