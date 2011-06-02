#include "events.h"

void EventsResponder::reply(std::ostream& out, cxxtools::http::Request& request, cxxtools::http::Reply& reply)
{
  std::string params = getRestParams((std::string)"/events", request.url()); 
  EventList* eventList;

  if ( request.method() != "GET") {
     reply.httpReturn(403, "To retrieve information use the GET method!");
     return;
  }

  if ( isFormat(params, ".json") ) {
     reply.addHeader("Content-Type", "application/json; charset=utf-8");
     eventList = (EventList*)new JsonEventList(&out);
  } else if ( isFormat(params, ".html") ) {
     reply.addHeader("Content-Type", "text/html; charset=utf-8");
     eventList = (EventList*)new HtmlEventList(&out);
  } else if ( isFormat(params, ".xml") ) {
     reply.addHeader("Content-Type", "text/xml; charset=utf-8");
     eventList = (EventList*)new XmlEventList(&out);
  } else {
     reply.httpReturn(403, "Resources are not available for the selected format. (Use: .json or .html)");
     return;
  }

  int channel_number = getIntParam(params, 0);
  int timespan = getIntParam(params, 1);
  int from = getIntParam(params, 2);

  bool scan_images = (int)request.qparams().find("images=true") != -1 ? true : false;
  
  cChannel* channel = getChannel(channel_number);
  if ( channel == NULL ) { 
     reply.httpReturn(404, "Channel with number _xx_ not found!"); 
     return; 
  }

  if ( from == -1 ) from = time(NULL); // default time is now
  if ( timespan == -1 ) timespan = 3600; // default timespan is one hour
  int to = from + timespan;

  cSchedulesLock MutexLock;
  const cSchedules *Schedules = cSchedules::Schedules(MutexLock);

  if( !Schedules ) {
     reply.httpReturn(404, "Could not find schedules!");
     return;
  }

  const cSchedule *Schedule = Schedules->GetSchedule(channel->GetChannelID());
  
  if ( !Schedule ) {
     reply.httpReturn(404, "Could not find schedule!");
     return;
  }

  eventList->init();

  for(cEvent* event = Schedule->Events()->First(); event; event = Schedule->Events()->Next(event))
  {
    int ts = event->StartTime();
    int te = ts + event->Duration();
    if ( ts <= to && te >= from ) {
       eventList->addEvent(event, scan_images);
    }else{
      if(ts > to) break;
    }
  }

  eventList->finish();
  delete eventList;
}

void operator<<= (cxxtools::SerializationInfo& si, const SerEvent& e)
{
  si.addMember("id") <<= e.Id;
  si.addMember("title") <<= e.Title;
  si.addMember("short_text") <<= e.ShortText;
  si.addMember("description") <<= e.Description;
  si.addMember("start_time") <<= e.StartTime;
  si.addMember("duration") <<= e.Duration;
  si.addMember("image") <<= e.Image;
}

void operator<<= (cxxtools::SerializationInfo& si, const SerEvents& e)
{
  si.addMember("rows") <<= e.event;
}

void HtmlEventList::init()
{
  writeHtmlHeader(out);
  
  write(out, "<ul>");
}

void HtmlEventList::addEvent(cEvent* event, bool scan_images = false)
{
  write(out, "<li>");
  write(out, (char*)event->Title()); //TODO: add more infos
  write(out, "\n");
}

void HtmlEventList::finish()
{
  write(out, "</ul>");
  write(out, "</body></html>");
}

void JsonEventList::addEvent(cEvent* event, bool scan_images = false)
{
  cxxtools::String eventTitle;
  cxxtools::String eventShortText;
  cxxtools::String eventDescription;
  cxxtools::String empty = UTF8Decode("");
  SerEvent serEvent;

  if( !event->Title() ) { eventTitle = empty; } else { eventTitle = UTF8Decode(event->Title()); }
  if( !event->ShortText() ) { eventShortText = empty; } else { eventShortText = UTF8Decode(event->ShortText()); }
  if( !event->Description() ) { eventDescription = empty; } else { eventDescription = UTF8Decode(event->Description()); }

  serEvent.Id = event->EventID();
  serEvent.Title = eventTitle;
  serEvent.ShortText = eventShortText;
  serEvent.Description = eventDescription;
  serEvent.StartTime = event->StartTime();
  serEvent.Duration = event->Duration();

  if ( scan_images ) {
     std::string regexpath = (std::string)"/var/lib/vdr/plugins/tvm2vdr/epgimages/" + itostr(serEvent.Id) + (std::string)"_*.*";
     esyslog("restfulapi: imagepath:/%s/", regexpath.c_str());
     std::vector< std::string > images;
     int found = scanForFiles(regexpath, images);
     if ( found > 0 ) {
        serEvent.Image = UTF8Decode(images[0]);
     }
  }

  serEvents.push_back(serEvent);
}

void JsonEventList::finish()
{
  cxxtools::JsonSerializer serializer(*out);
  serializer.serialize(serEvents, "events");
  serializer.finish();
}

void XmlEventList::init()
{
  write(out, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
  write(out, "<events xmlns=\"http://www.domain.org/restfulapi/2011/events-xml\">\n");
}

void XmlEventList::addEvent(cEvent* event, bool scan_images = false)
{
  std::string eventTitle;
  std::string eventShortText;
  std::string eventDescription;

  if ( event->Title() == NULL ) { eventTitle = ""; } else { eventTitle = event->Title(); }
  if ( event->ShortText() == NULL ) { eventShortText = ""; } else { eventShortText = event->ShortText(); }
  if ( event->Description() == NULL ) { eventDescription = ""; } else { eventDescription = event->Description(); }

  write(out, " <event>\n");
  write(out, (const char*)cString::sprintf("  <param name=\"id\">%i</param>\n", event->EventID()));
  write(out, (const char*)cString::sprintf("  <param name=\"title\">%s</param>\n", encodeToXml(eventTitle).c_str()));
  write(out, (const char*)cString::sprintf("  <param name=\"short_text\">%s</param>\n", encodeToXml(eventShortText).c_str()));
  write(out, (const char*)cString::sprintf("  <param name=\"description\">%s</param>\n", encodeToXml(eventDescription).c_str()));
  write(out, (const char*)cString::sprintf("  <param name=\"start_time\">%i</param>\n", (int)event->StartTime()));
  write(out, (const char*)cString::sprintf("  <param name=\"duration\">%i</param>\n", event->Duration()));
  write(out, " </event>\n");
}

void XmlEventList::finish()
{
  write(out, "</events>");
}
