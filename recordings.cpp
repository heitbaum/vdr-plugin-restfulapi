#include "recordings.h"

void RecordingsResponder::reply(std::ostream& out, cxxtools::http::Request& request, cxxtools::http::Reply& reply)
{
  if ( request.method() == "GET" ) {
     showRecordings(out, request, reply);
  } else if ( request.method() == "DELETE" ) {
     deleteRecording(out, request, reply);
  } else {
     reply.httpReturn(501, "Only GET and DELETE methods are supported.");
  }
}

void RecordingsResponder::deleteRecording(std::ostream& out, cxxtools::http::Request& request, cxxtools::http::Reply& reply)
{
  QueryHandler q("/recordings", request);
  int recording_number = q.getParamAsInt(0);
  if ( recording_number <= 0 || recording_number > Recordings.Count() ) { 
     reply.httpReturn(404, "Wrong recording number!");
  } else {
     recording_number--; // first recording is 0 and not 1 like in param
     cRecording* delRecording = Recordings.Get(recording_number);
     if ( delRecording->Delete() ) {
        Recordings.DelByName(delRecording->FileName());
     }
  }
}

void RecordingsResponder::showRecordings(std::ostream& out, cxxtools::http::Request& request, cxxtools::http::Reply& reply)
{
  QueryHandler q("/recordings", request);
  RecordingList* recordingList;

  if ( q.isFormat(".json") ) {
     reply.addHeader("Content-Type", "application/json; charset=utf-8");
     recordingList = (RecordingList*)new JsonRecordingList(&out);
  } else if ( q.isFormat(".html") ) {
     reply.addHeader("Content-Type", "text/html; charset=utf-8");
     recordingList = (RecordingList*)new HtmlRecordingList(&out);
  } else if ( q.isFormat(".xml") )  {
     reply.addHeader("Content-Type", "text/xml; charset=utf-8");
     recordingList = (RecordingList*)new XmlRecordingList(&out);
  } else {
     reply.httpReturn(404, "Resources are not available for the selected format. (Use: .json or .html)");
     return;
  }

  recordingList->init();

  for (cRecording* recording = Recordings.First(); recording; recording = Recordings.Next(recording)) {
     recordingList->addRecording(recording); 
  }

  recordingList->finish();
  delete recordingList; 
}

int getRecordingDuration(cRecording* m_recording) {
  int RecLength = 0;
  if ( !m_recording->FileName() ) return 0;
  cIndexFile *index = new cIndexFile(m_recording->FileName(), false, m_recording->IsPesRecording());
  if ( index && index->Ok()) {
     RecLength = (int) (index->Last() / SecondsToFrames(60, m_recording->FramesPerSecond()));
  }
  delete index;

  return RecLength;
}

void operator<<= (cxxtools::SerializationInfo& si, const SerRecording& p)
{
  si.addMember("name") <<= p.Name;
  si.addMember("file_name") <<= p.FileName;
  si.addMember("is_new") <<= p.IsNew;
  si.addMember("is_edited") <<= p.IsEdited;
  si.addMember("is_pes_recording") <<= p.IsPesRecording;
  si.addMember("duration") <<= p.Duration;
  si.addMember("event_title") <<= p.EventTitle;
  si.addMember("event_short_text") <<= p.EventShortText;
  si.addMember("event_description") <<= p.EventDescription;
  si.addMember("event_start_time") <<= p.EventStartTime;
  si.addMember("event_duration") <<= p.EventDuration;
}

void operator<<= (cxxtools::SerializationInfo& si, const SerRecordings& p)
{
  si.addMember("rows") <<= p.recording;
}

RecordingList::RecordingList(std::ostream *out)
{
  s = new StreamExtension(out);
}

RecordingList::~RecordingList()
{
  delete s;
}

void HtmlRecordingList::init()
{
  s->writeHtmlHeader();
  s->write("<ul>");
}

void HtmlRecordingList::addRecording(cRecording* recording)
{
  s->write("<li>");
  s->write((char*)recording->Name());
  s->write("</body></html>");
}

void HtmlRecordingList::finish()
{
  s->write("</ul>");
  s->write("</body></html>");
}

void JsonRecordingList::addRecording(cRecording* recording)
{
  cxxtools::String empty = StringExtension::UTF8Decode("");
  cxxtools::String eventTitle = empty;
  cxxtools::String eventShortText = empty;
  cxxtools::String eventDescription = empty;
  int eventStartTime = -1;
  int eventDuration = -1;

  cEvent* event = (cEvent*)recording->Info()->GetEvent();

  if ( event != NULL )
  {
     if ( event->Title() ) { eventTitle = StringExtension::UTF8Decode(event->Title()); }
     if ( event->ShortText() ) { eventShortText = StringExtension::UTF8Decode(event->ShortText()); }
     if ( event->Description() ) { eventDescription = StringExtension::UTF8Decode(event->Description()); }
     if ( event->StartTime() > 0 ) { eventStartTime = event->StartTime(); }
     if ( event->Duration() > 0 ) { eventDuration = event->Duration(); }
  }

  SerRecording serRecording;
  serRecording.Name = StringExtension::UTF8Decode(recording->Name());
  serRecording.FileName = StringExtension::UTF8Decode(recording->FileName());
  serRecording.IsNew = recording->IsNew();
  serRecording.IsEdited = recording->IsEdited();
  serRecording.IsPesRecording = recording->IsPesRecording();
  serRecording.Duration = -1;// don't start HDD / disabled until implemented in vdr --- getRecordingDuration(recording);
  serRecording.EventTitle = eventTitle;
  serRecording.EventShortText = eventShortText;
  serRecording.EventDescription = eventDescription;
  serRecording.EventStartTime = eventStartTime;
  serRecording.EventDuration = eventDuration;

  //esyslog("restfulapi: name: %s, filename:%s, event_title:%s, event_short_text:%s, event_description:%s", recording->Name(), recording->FileName()

  serRecordings.push_back(serRecording);
}

void JsonRecordingList::finish()
{
  cxxtools::JsonSerializer serializer(*s->getBasicStream());
  serializer.serialize(serRecordings, "recordings");
  serializer.finish();
}

void XmlRecordingList::init()
{
  s->writeXmlHeader();
  s->write("<recordings xmlns=\"http://www.domain.org/restfulapi/2011/recordings-xml\">\n");
}

void XmlRecordingList::addRecording(cRecording* recording)
{
  std::string eventTitle = "";
  std::string eventShortText = "";
  std::string eventDescription = "";
  int eventStartTime = -1;
  int eventDuration = -1;

  cEvent* event = (cEvent*)recording->Info()->GetEvent();

  if ( event != NULL )
  {
     if ( event->Title() ) { eventTitle = event->Title(); }
     if ( event->ShortText() ) { eventShortText = event->ShortText(); }
     if ( event->Description() ) { eventDescription = event->Description(); }
     if ( event->StartTime() > 0 ) { eventStartTime = event->StartTime(); }
     if ( event->Duration() > 0 ) { eventDuration = event->Duration(); }
  }

  s->write(" <recording>\n");
  s->write((const char*)cString::sprintf("  <param name=\"name\">%s</param>\n", StringExtension::encodeToXml(recording->Name()).c_str()) );
  s->write((const char*)cString::sprintf("  <param name=\"filename\">%s</param>\n", StringExtension::encodeToXml(recording->Name()).c_str()) );
  s->write((const char*)cString::sprintf("  <param name=\"is_new\">%s</param>\n", recording->IsNew() ? "true" : "false" ));
  s->write((const char*)cString::sprintf("  <param name=\"is_edited\">%s</param>\n", recording->IsEdited() ? "true" : "false" ));
  s->write((const char*)cString::sprintf("  <param name=\"is_pes_recording\">%s</param>\n", recording->IsPesRecording() ? "true" : "false" ));
  s->write((const char*)cString::sprintf("  <param name=\"duration\">%i</param>\n", -1)); //getRecordingDuration(recording);
  s->write((const char*)cString::sprintf("  <param name=\"event_title\">%s</param>\n", StringExtension::encodeToXml(eventTitle).c_str()) );
  s->write((const char*)cString::sprintf("  <param name=\"event_short_text\">%s</param>\n", StringExtension::encodeToXml(eventShortText).c_str()) );
  s->write((const char*)cString::sprintf("  <param name=\"event_description\">%s</param>\n", StringExtension::encodeToXml(eventDescription).c_str()) );
  s->write((const char*)cString::sprintf("  <param name=\"event_start_time\">%i</param>\n", eventStartTime));
  s->write((const char*)cString::sprintf("  <param name=\"event_duration\">%i</param>\n", eventDuration));
  s->write(" </recording>\n");
}

void XmlRecordingList::finish()
{
  s->write("</recordings>");
}
