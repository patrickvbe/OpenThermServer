#include "WebServer.h"

#include <ESP8266WebServer.h>
#include "PrintString.h"

static ESP8266WebServer server(80);
WebServer webserver;

static const char* pageheader = "<html><head><style>body { zoom: 3; font-family: Arial;} .myDiv { background-color: lightblue; text-align: center; font-size: 40px;} .T { font-size: 100px;}</style></head><body>";
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
  result += "<div class=\"myDiv\">Verwarming</div><p>Binnen temperatuur<br><div class=\"T\">";
  result.ConcatTemp(MPCtrl->insideTemperature);
  result += "</div></p><p>Buiten temperatuur<br><div class=\"T\">";
  result.ConcatTemp(MPCtrl->outsideTemperature);
  result += "</div></p><p>Water: ";
  result.ConcatTemp(MPCtrl->waterTemperature);
  result += "<br>Pomp: ";
  result.concat(MPCtrl->isPumpOn ? "aan" : "uit");
  result += "</p>";
  result += pagefooter;
  server.send(200, "text/html", result);
}

void WebServer::ServeNotFound()
{
  String message = "File Not Found\n\n";
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
