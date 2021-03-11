
class ControlValues;

class WebServer
{
  public:
    void Init(ControlValues& ctrl);
    void Process();

  private:
    ControlValues*  MPCtrl = nullptr;
    void ServeRoot();
    void ServeNotFound();
};

extern WebServer webserver;