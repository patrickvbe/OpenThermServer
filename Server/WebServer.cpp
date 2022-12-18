#include "WebServer.h"
#include "ControlValues.h"
#include <ESP8266WebServer.h>
#include "OpenTherm.h"
#include "PrintString.h"

static ESP8266WebServer server(80);
WebServer webserver;

static const char* pageheader = "<html><head><style>"
"body { zoom: 3; font-family: Arial;}"
" .head { background-color: lightblue; text-align: center; font-size: 40px;}"
" .T { font-size: 100px;}"
"table, th, td {border: 1px solid black; border-collapse: collapse;}"
"</style></head><body>";
static const char* pagefooter = "</body></html>";

void WebServer::Init(ControlValues& ctrl)
{
  MPCtrl = &ctrl;
  server.on("/", []() {
    webserver.ServeRoot();
  });
  server.on("/read", []() {
    webserver.ServeRead();
  });
  server.on("/write", []() {
    webserver.ServeWrite();
  });
  server.onNotFound([]() {
    webserver.ServeNotFound();
  });
  server.begin();
}

void AppendOTValue(PrintString& result, OpenTherm::ValueType vtype, uint16_t value)
{
  if ( value == 0xFFFF )
  {
    result += "----";
  }
  else
  {
    switch ( vtype )
    {
      case OpenTherm::ValueType::TInt:
        result.print(value, HEX);
        break;
      case OpenTherm::ValueType::TFloat:
        result.print(OpenTherm::toFloat(value));
        break;
      case OpenTherm::ValueType::TTwoByte:
        result.print(value >> 8);
        result += '-';
        result.print(value &0xFF);
        break;
      case OpenTherm::ValueType::TFlags:
        result += 'B';
        result.print(value >> 8, BIN);
        result += " B";
        result.print(value & 0xFF, BIN);
        break;
    }
  }
}

void WebServer::ServeRoot()
{
  PrintString result(pageheader);
  result += "<div class=\"head\">Historie</div><br>Uptime: ";
  PrintTime(MPCtrl->timestampsec, result);
  result += "<br><table><tr><th>tijd</th><th>id</th><th>send</th><th>rec</th></tr>";
  MPCtrl->ForNodes([&](const ValueNode& node)
  {
    result += "<tr><td>";
    result.print((long)(node.timestamp - MPCtrl->timestampsec));
    result += "s</td><td>";
    result.print(node.id);
    result += " ";
    OpenTherm::ValueType vtype;
    auto str = OpenTherm::messageIDToString((OpenThermMessageID)node.id, vtype);
    if ( str != nullptr ) result += str;
    result += "</td><td>";
    result.print(OpenTherm::messageTypeToString((OpenThermMessageType)node.stype));
    result += " ";
    AppendOTValue(result, vtype, node.send);
    if ( node.rec == 0xFFFF )
    {
      result += "</td><td>----";
    }
    else
    {
      result += "</td><td>";
      result.print(OpenTherm::messageTypeToString((OpenThermMessageType)node.rtype));
      result += " ";
      AppendOTValue(result, vtype, node.rec);
    }
    result += "</td></tr>";
    return true; // keep going
  });
  result += "</table>";
  result += pagefooter;
  server.send(200, "text/html", result);
}

void WebServer::ServeRead()
{
  long id = server.arg("id").toInt();
  long data = strtol(server.arg("data").c_str(), nullptr, 0); // Supports dec/oct/hex
  auto pnode = MPCtrl->FindId(id);
  if ( pnode && (pnode->stype == OpenThermMessageType::WRITE_DATA || pnode->send == data) && MPCtrl->timestampsec - pnode->timestamp < 30 )  // Less than 30s old, use the existing value.
  {
    PrintString result;
    OpenTherm::ValueType vtype;
    auto str = OpenTherm::messageIDToString((OpenThermMessageID)id, vtype);
    result += str;
    result.print("=");
    AppendOTValue(result, vtype, pnode->rec);
    server.send(200, "text/plain", result);
  }
  else
  {
    // Send request and keep scanning for a few seconds for the response.
    if ( MPCtrl->request_status == ControlValues::RequestStatus::Idle )
    {
      MPCtrl->insert_time = MPCtrl->timestampsec;
      MPCtrl->pending_request = OpenTherm::buildRequest(OpenThermMessageType::READ_DATA, (OpenThermMessageID)id, data);
      MPCtrl->request_status = ControlValues::RequestStatus::PendingSend;
    }
    else
    {
      server.send(503, "text/plain", "Error: A web request is still pending.");
    }
  }
}

void WebServer::ServeWrite()
{
  long id = server.arg("id").toInt();
  long data = strtol(server.arg("data").c_str(), nullptr, 0); // Supports dec/oct/hex
  // Send request and keep scanning for a few seconds for the response.
  if ( id == 0 )
  {
    server.send(503, "text/plain", "Error: Invalid ID");
  }
  else if ( MPCtrl->request_status == ControlValues::RequestStatus::Idle )
  {
    MPCtrl->insert_time = MPCtrl->timestampsec;
    MPCtrl->pending_request = OpenTherm::buildRequest(OpenThermMessageType::WRITE_DATA, (OpenThermMessageID)id, data);
    MPCtrl->request_status = ControlValues::RequestStatus::PendingSend;
  }
  else
  {
    server.send(503, "text/plain", "Error: A web request is still pending.");
  }
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
  if ( MPCtrl->request_status == ControlValues::RequestStatus::Received )
  {
    MPCtrl->request_status = ControlValues::RequestStatus::Idle;
    PrintString result;
    OpenTherm::ValueType vtype;
    auto str = OpenTherm::messageIDToString(OpenTherm::getDataID(MPCtrl->response), vtype);
    result += str;
    result.print("=");
    AppendOTValue(result, vtype, OpenTherm::getUInt(MPCtrl->response));
    result += "!";
    server.send(200, "text/plain", result);
  }
  else if ( MPCtrl->request_status != ControlValues::RequestStatus::Idle && MPCtrl->timestampsec - MPCtrl->insert_time > 5  )
  {
    MPCtrl->request_status = ControlValues::RequestStatus::Idle;
    server.send(504, "text/plain", "Error: Failed to process the request.");
  }
}
