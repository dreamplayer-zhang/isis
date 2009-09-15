/*
 *  message.cpp
 *
 *  Created by Enrico Reimer on 04.05.08.
 *  Copyright 2008. All rights reserved.
 */

#include "message.hpp"
#include "common.hpp"
#include <sys/time.h>
#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

using isis::util::_internal::Message;
using ::std::string;

namespace isis{ namespace util{

void _internal::MessageHandlerBase::stopBelow(LogLevel stop){
	m_stop_below=stop;
}

bool _internal::MessageHandlerBase::requestStop(LogLevel _level){
	if(m_stop_below>_level)
		return kill(getpid(),SIGTSTP) == 0;
	else 
		return false;
}

string Message::strTime()const{
	char buffer[11];
	tm r = {0};
	strftime(buffer, sizeof(buffer), "%X", localtime_r(&timeStamp, &r));
	struct timeval tv;
	gettimeofday(&tv, 0);
	char result[100] = {0};
	::std::sprintf(result, "%s.%03ld", buffer, (long)tv.tv_usec / 1000);
	return result;
}

Message::Message(string _object,string _file,int _line,LogLevel _level,MessageHandlerBase *_commitTo)
:object(_object),file(_file),line(_line),level(_level),commitTo(_commitTo){
	time(&timeStamp);
}
Message::Message(const Message &src)
: ::std::ostringstream(src.str()),object(src.object),file(src.file),subjects(src.subjects),timeStamp(src.timeStamp),line(src.line),commitTo(src.commitTo)
{}

  
string Message::merge()const{
	string ret(str());
	size_t found=string::npos;
	std::list<std::string>::const_iterator subj=subjects.begin();
	if((found=ret.find("{o}"))!=string::npos)
		ret.replace(found,3,object);
	found=0;
	while((found=ret.find("{s}",found))!=string::npos)
		ret.replace(found,3,*(subj++));
	return ret;
}

void DefaultMsgPrint::commit(const Message &mesg){
	*o << "[" << mesg.file << ":" << mesg.line << "|" << mesg.object << "]\t" << mesg.merge() << std::endl;
}

void DefaultMsgPrint::setStream(::std::ostream &_o){
	o = &_o;
}

void DefaultMsgPrintNeq::commit(const Message &mesg){
	const string out(mesg.merge());
	if(last!=out){
		*o << "[" << mesg.strTime() << "|" << mesg.file << ":" << mesg.line << "::" << mesg.object << "]\t" <<  out << std::endl;
		last=out;
	}
}

bool Message::shouldCommit()const{
	return (commitTo && commitTo->level >= level);
}

::std::ostream *DefaultMsgPrint::o=&::std::cout;
LogLevel _internal::MessageHandlerBase::m_stop_below=error;


}}
namespace std{
Message& endl(Message& __os) {
	if(__os.shouldCommit()){
		__os.commitTo->commit(__os);
		__os.str("");
		__os.clear();
		__os.commitTo->requestStop(__os.level);
	}
	return __os;
}

}
