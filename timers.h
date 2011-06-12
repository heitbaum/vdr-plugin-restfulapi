#include <vdr/channels.h>
#include <vdr/timers.h>
#include <vdr/epg.h>
#include <vdr/menu.h>
#include <cxxtools/http/request.h>
#include <cxxtools/http/reply.h>
#include <cxxtools/http/responder.h>
#include <cxxtools/query_params.h>
#include <cxxtools/arg.h>
#include <cxxtools/jsonserializer.h>
#include <cxxtools/regex.h>
#include <cxxtools/serializationinfo.h>
#include <cxxtools/utf8codec.h>
#include <iostream>
#include <sstream>
#include "tools.h"


class TimersResponder : public cxxtools::http::Responder
{
  public:
    explicit TimersResponder(cxxtools::http::Service& service)
      : cxxtools::http::Responder(service)
      { }
     virtual void reply(std::ostream& out, cxxtools::http::Request& request, cxxtools::http::Reply& reply);
     void createTimer(std::ostream& out, cxxtools::http::Request& request, cxxtools::http::Reply& reply);
     void updateTimer(std::ostream& out, cxxtools::http::Request& request, cxxtools::http::Reply& reply);
     void deleteTimer(std::ostream& out, cxxtools::http::Request& request, cxxtools::http::Reply& reply);
     void showTimers(std::ostream& out, cxxtools::http::Request& request, cxxtools::http::Reply& reply);
};

typedef cxxtools::http::CachedService<TimersResponder> TimersService;

struct SerTimer
{
  int Start;
  int Stop;
  int Priority;
  int Lifetime;
  int EventID;
  int WeekDays;
  int Day;
  int Channel;
  bool IsRecording;
  bool IsPending;
  bool IsActive;
  cxxtools::String FileName;
  cxxtools::String ChannelName;
};

struct SerTimers
{
  std::vector < struct SerTimer > timer;
};

void operator<<= (cxxtools::SerializationInfo& si, const SerTimer& t);
void operator>>= (const cxxtools::SerializationInfo& si, SerTimer& t);
void operator<<= (cxxtools::SerializationInfo& si, const SerTimers& t);

class TimerList
{
  protected:
    StreamExtension *s;
  public:
    TimerList(std::ostream* _out);
    ~TimerList();
    virtual void init() { };
    virtual void addTimer(cTimer* timer) { };
    virtual void finish() { };
};

class HtmlTimerList : TimerList
{
  public:
    HtmlTimerList(std::ostream* _out) : TimerList(_out) { };
    ~HtmlTimerList() { };
    virtual void init();
    virtual void addTimer(cTimer* timer);
    virtual void finish();
};

class JsonTimerList : TimerList
{
  private:
    std::vector < struct SerTimer > serTimers;
  public:
    JsonTimerList(std::ostream* _out) : TimerList(_out) { };
    ~JsonTimerList() { };
    virtual void addTimer(cTimer* timer);
    virtual void finish();
};

class XmlTimerList : TimerList
{
  public:
    XmlTimerList(std::ostream* _out) : TimerList(_out) { };
    ~XmlTimerList() { };
    virtual void init();
    virtual void addTimer(cTimer* timer);
    virtual void finish();
};

class TimerValues
{
  private:
    int		ConvertNumber(std::string v);
  public:
    TimerValues() { };
    ~TimerValues() { };
    bool 	IsFlagsValid(int v);
    bool	IsFileValid(std::string v);
    bool	IsLifetimeValid(int v);
    bool	IsPriorityValid(int v);
    bool	IsStopValid(int v);
    bool	IsStartValid(int v);
    bool	IsWeekdaysValid(std::string v);

    int		ConvertFlags(std::string v);
    cEvent*	ConvertEvent(std::string event_id, cChannel* channel);
    cTimer*	ConvertTimer(std::string v);
    std::string ConvertFile(std::string v); // replaces : with | - required by parsing method of VDR
    std::string ConvertAux(std::string v);  // replaces : with | - required by parsing method of VDR
    int		ConvertLifetime(std::string v);
    int		ConvertPriority(std::string v);
    int		ConvertStop(std::string v);
    int		ConvertStart(std::string v);
    int		ConvertDay(std::string v);
    cChannel*	ConvertChannel(std::string v);
};
