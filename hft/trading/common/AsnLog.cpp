
#include <string>
#include <string.h>
#include <iostream>
#include <fstream>

#include "AsnLog.h"

#ifdef  _ASN_LOG_USE_LOG4CXX_
#include <log4cxx/helpers/properties.h>
#endif

#ifdef _ASN_LOG_USE_LOG4CPLUS_
#include <log4cplus/configurator.h>
#endif

using namespace std;

/****************************************************************************
 * Public constants
 ****************************************************************************/
const string AsnLogUtil::DEFAULT_LOG_PROPERTIES_FILE = "asn.log.properties";


/****************************************************************************
 * Private variables
 ****************************************************************************/
pthread_key_t* AsnLogUtil::threadLocal = NULL;
bool AsnLogUtil::s_initDefault = false;
bool AsnLogUtil::s_initProp = false;
bool AsnLogUtil::s_debug = false;
bool AsnLogUtil::s_syslogenable = false;
string AsnLogUtil::s_syslogname("");
int AsnLogUtil::s_sysloglevel = 0;
AsnLoggerPtr AsnLogUtil::s_rootLogger = log4cplus::Logger::getInstance("asn");


/****************************************************************************
 * Public static methods
 ****************************************************************************/
AsnLoggerPtr AsnLogUtil::getLogger(const string category) {
	if (category.length() == 0) {
	   if (s_debug)
	      cout << "Invalid empty category" << endl;
		return s_rootLogger;
	}

	if (!s_initDefault) {
	   if (s_debug)
	      cout << "Using default log config for category " << category \
            << " using file \"asn.log.properties\"" << endl;
	   initLog("asn.log.properties", true);
	}

#ifdef 	_ASN_LOG_USE_LOG4CXX_
	return log4cxx::Logger::getLogger(category);
#endif

#ifdef _ASN_LOG_USE_LOG4CPLUS_
	return log4cplus::Logger::getInstance(category);
#endif

}

void AsnLogUtil::initLog(const string logconfig, bool deft) {

 //  cout << "logconfig =  " << logconfig << endl;
   if (!s_initDefault || !s_initProp) {

      if (!s_initDefault && deft) {
         if (s_debug)
            cout << "initLog (default) using " << logconfig << endl;
         s_initDefault = true;

#ifdef _ASN_LOG_USE_LOG4CXX_
         createDefaultLogProp(logconfig);
         log4cxx::PropertyConfigurator::configure(logconfig);
#endif

#ifdef _ASN_LOG_USE_LOG4CPLUS_
         ::log4cplus::BasicConfigurator config;
         config.configure();
#endif
         //return;
      }
      else if (!s_initProp && !deft)
      {
         s_initDefault = true;
         s_initProp = true;
         if (s_debug)
            cout << "initLog (supplied) using " << logconfig << endl;

#ifdef _ASN_LOG_USE_LOG4CXX_
            log4cxx::PropertyConfigurator::configureAndWatch(logconfig, 30*1000);
#endif

#ifdef _ASN_LOG_USE_LOG4CPLUS_
#ifdef WIN32
         ::log4cplus::PropertyConfigurator::doConfigure(logconfig);
#else
         static log4cplus::ConfigureAndWatchThread watcherThread(logconfig, 30*1000);
#endif
#endif
         //return;
      }

      //do syslog init here..
      if(s_syslogenable)
      {
          if (s_debug)
              cout << "initLog enable syslog" << endl;

          string name = s_syslogname;
          openlog(name.c_str(), LOG_PID, LOG_USER);
          int logmask =  LOG_UPTO(s_sysloglevel);
          setlogmask(logmask);
      }

      //threadlocal
//      threadLocal = new pthread_key_t();
//      pthread_key_create(threadLocal, NULL);
   }
}

void AsnLogUtil::addErrLog(const string& msg)
{
//	ThreadLocalData* data = (ThreadLocalData*)pthread_getspecific(*threadLocal);
//	if(data)
//	{
//		memcpy(data->buf+ data->index, msg.c_str(), msg.length());
//		data->index += msg.length();
//		data->buf[data->index++] = '\r';
//		data->buf[data->index++] = '\n';
//		data->buf[data->index] = 0;//string ending
//	}
}

void AsnLogUtil::clearErrLog()
{
//	ThreadLocalData* data = (ThreadLocalData*)pthread_getspecific(*threadLocal);
//	if(!data)
//	{
//		data = new ThreadLocalData();
//		data->index = 0;
//		data->buf[0] = 0;//string ending
//		pthread_setspecific(*threadLocal, data);
//	}
//	else
//	{
//		data->index = 0;
//		data->buf[0] = 0;//string ending
//	}
}

char* AsnLogUtil::getErrLog()
{
//	ThreadLocalData* data = (ThreadLocalData*)pthread_getspecific(*threadLocal);
//	if(data)
//	{
//		return data->buf;
//	}
	return NULL;
}

void AsnLogUtil::setDebug(const bool debug) {
   s_debug = debug;
}

void AsnLogUtil::enableSyslog(string syslogname, int sysloglevel) {
    s_syslogenable = true;
    s_syslogname = syslogname;
    s_sysloglevel = sysloglevel;
}

bool AsnLogUtil::isSyslogEnabled() {
    return s_syslogenable;
}

void AsnLogUtil::shutdown() {
#ifdef _ASN_LOG_USE_LOG4CPLUS_
         log4cplus::Logger::shutdown();
#endif

     if(s_syslogenable)
     {
        closelog();
        s_syslogenable = false;
        s_syslogname.clear();
     }

     //reset all variables
     s_initDefault = false;
     s_initProp = false;
     s_debug = false;

     return;
}



void AsnLogUtil::createNDCContext(const string contextString)
{
#ifdef _ASN_LOG_USE_LOG4CPLUS_
   log4cplus::getNDC().push((log4cplus::tstring)contextString);
#endif
}

void AsnLogUtil::removeNDCContext()
{
#ifdef _ASN_LOG_USE_LOG4CPLUS_
   log4cplus::getNDC().pop();
   log4cplus::getNDC().remove();
#endif
}


/****************************************************************************
 * Private static methods
 ****************************************************************************/
void AsnLogUtil::createDefaultLogProp(const char* logconfig) {
   ofstream f(logconfig);
   if (f.is_open())
   {
      cout << "ERROR!!!! failed to startup logger" << endl;
      f.close();
      return;
   }

   static const char* defaultLogContent =
      "# Copy this file to your local directory for now...\n"
      "log4j.rootLogger=DEBUG, logging\n"
      "log4j.appender.logging=org.apache.log4j.ConsoleAppender\n"
      "log4j.appender.logging.layout=org.apache.log4j.PatternLayout\n"
      "\n"
      "# Print the date in ISO 8601 format\n"
      "log4j.appender.logging.layout.ConversionPattern=%d{ISO8601} %5p [%t] %c{1}(): %m%n\n";

   f << defaultLogContent;
   f.close();
   return;
}






