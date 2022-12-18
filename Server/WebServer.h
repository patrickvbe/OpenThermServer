
class ControlValues;

class WebServer
{
  public:
    void Init(ControlValues& ctrl);
    void Process();

  private:
    ControlValues*  MPCtrl = nullptr;
    void ServeRoot();
    void ServeRead();
    void ServeWrite();
    void ServeNotFound();
};

extern WebServer webserver;