#pragma once


#include <string>
#include <syslog.h>
#include <cstdlib>

using namespace std;

// Usage:
// - log initialization
//		this is only needed in the main() function
//		ASN_INITLOG("asn.log.properties");
//		which uses logconfig
//
// - setting up the logger:
// 		AsnLoggerPtr logger = ASN_GETLOGGER("asn.sfa");
//
// - Note:
//		log category naming:
//			- starts with "asn."
//			- followed by the sub-directory name, e.g. "framework"
//			- followed by the internal class name (include sub-directory name if needed)
//		 	- e.g.: "asn.framework.workflow"
//					"rmm.coreapp.sfa"
//
// - Example: each class's static member, static inside the cpp file
//		MyClass.h:
//		- no need to include AsnLog from .h
//
//		MyClass.cpp:
//			#include "AsnLog.h"
//			static AsnLoggerPtr logger = ASN_GETLOGGER("sfa.MyClass");
//			...
//
// - Example: logging string
//		ASN_DEBUG(logger, "Debug Message");
//
// - Example: log formatting using stream
//      ASN_DEBUG_STREAM_BEGIN(logger) << "Message" + string1 ASN_DEBUG_STREAM_END(logger);
//                                     ^ must have << here   ^ no "<<" here!!!
//

// #define _ASN_LOG_USE_LOG4CXX_
#define _ASN_LOG_USE_LOG4CPLUS_

#ifdef WIN32
#undef ASN_LOG_HAS_FUNCTION
#ifdef  ASN_LOG_CPP
#define ASN_LOG_EXPORT __declspec(dllexport)
#else
#define ASN_LOG_EXPORT __declspec(dllimport)
#endif

#else
#define ASN_LOG_HAS_FUNCTION
#define ASN_LOG_EXPORT
#endif

#ifdef  _ASN_LOG_USE_LOG4CPLUS_		// --------------------------------------------------------
#include <log4cplus/logger.h>
#include <log4cplus/ndc.h>

// TypeDef: AsnLoggerPtr
// Purpose: the ASN logger type
typedef log4cplus::Logger AsnLoggerPtr ;

/*
 * Macro: ASN_DEBUG, ASN_INFO, ASN_WARN, ASN_ERROR
 * Purpose: performs logging, depending on the desired log level
 * Parameter:
 *	- AsnLoggerPtr logger: the logger instance, previously initialized using ASN_GETLOGGER()
 *	- std::string: the message to be logged.
 */

#ifdef ASN_LOG_HAS_FUNCTION
#define ASN_LOG(logger,message,level) { \
if (logger.isEnabledFor(level)) {\
	::log4cplus::tostringstream oss; \
	oss << std::dec;\
	if (logger.isEnabledFor(::log4cplus::INFO_LOG_LEVEL)) \
		oss << __FUNCTION__ << "(): ";\
	oss << ::log4cplus::getNDC().get(); \
	oss << message; \
    switch(level) \
    { \
        case ::log4cplus::FATAL_LOG_LEVEL: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_CRIT, "###### [FATAL] %s %s", __FILE__, oss.str().c_str()); \
            break; \
        case ::log4cplus::ERROR_LOG_LEVEL: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_ERR, "###### [ERROR] %s %s", __FILE__, oss.str().c_str()); \
            break; \
        case ::log4cplus::WARN_LOG_LEVEL: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_WARNING, "[WARN] %s %s", __FILE__, oss.str().c_str()); \
            break; \
        case ::log4cplus::INFO_LOG_LEVEL: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_INFO, "[INFO] %s %s", __FILE__, oss.str().c_str()); \
            break; \
        case ::log4cplus::DEBUG_LOG_LEVEL: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_DEBUG, "[DEBUG] %s %s", __FILE__, oss.str().c_str()); \
            break; \
      	case ::log4cplus::TRACE_LOG_LEVEL: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_DEBUG, "[TRACE] %s %s", __FILE__, oss.str().c_str()); \
            break; \
        default: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_INFO, "%s %s", __FILE__, oss.str().c_str()); \
            break; \
    } \
	logger.forcedLog(level, oss.str(), __FILE__, __LINE__); }}

#define ASN_TRACE(logger,message) ASN_LOG(logger,message,::log4cplus::TRACE_LOG_LEVEL)
#define ASN_DEBUG(logger,message) ASN_LOG(logger,message,::log4cplus::DEBUG_LOG_LEVEL)
#define ASN_INFO(logger,message)  ASN_LOG(logger,message,::log4cplus::INFO_LOG_LEVEL)
#define ASN_WARN(logger,message)  ASN_LOG(logger,message,::log4cplus::WARN_LOG_LEVEL)
#define ASN_ERROR(logger,message) ASN_LOG(logger,message,::log4cplus::ERROR_LOG_LEVEL)
#define ASN_FATAL(logger,message) ASN_LOG(logger,message,::log4cplus::FATAL_LOG_LEVEL)
#else
#define ASN_TRACE(logger,message) LOG4CPLUS_TRACE(logger,message)
#define ASN_DEBUG(logger,message) LOG4CPLUS_DEBUG(logger,message)
#define ASN_INFO(logger,message)  LOG4CPLUS_INFO(logger,message)
#define ASN_WARN(logger,message)  LOG4CPLUS_WARN(logger,message)
#define ASN_ERROR(logger,message) LOG4CPLUS_ERROR(logger,message)
#define ASN_FATAL(logger,message) LOG4CPLUS_FATAL(logger,message)
#endif

/*
 * ASN_DEBUG_STREAM_BEGIN, ASN_DEBUG_STREAM_END....
 * Purpose: perform streamed logging on the desired log level
 * Parameters:
 *	- AsnLoggerPtr logger: the logger instance, previously initialized using ASN_GETLOGGER()
 * Usage:
 * 	ASN_DEBUG_STREAM_BEGIN(logger) << "Message" + string1 ASN_DEBUG_STREAM_END(logger);
 *								   ^ must have << here             ^ no "<<" here!!!
 * 	ASN_INFO_STREAM_BEGIN(logger) << "Message" + string1 ASN_DEBUG_STREAM_END(logger);
 */

#ifdef ASN_LOG_HAS_FUNCTION
#define ASN_STREAM_BEGIN(logger,level) {\
if (logger.isEnabledFor(level)) {\
   ::log4cplus::tostringstream oss; \
   oss <<  std::dec;\
	if (logger.isEnabledFor(::log4cplus::INFO_LOG_LEVEL)) \
		oss << __FUNCTION__ << "(): "; \
	oss << ::log4cplus::getNDC().get(); \
	oss
#else
#define ASN_STREAM_BEGIN(logger,level) {\
if (logger.isEnabledFor(level)) {\
	::log4cplus::tostringstream oss; \
	oss <<  std::dec;\
	oss
#endif

#define ASN_STREAM_END(logger,level) ; \
    switch(level) \
    { \
        case ::log4cplus::FATAL_LOG_LEVEL: \
            cout << oss.str() << endl; \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_CRIT, "###### [FATAL] %s %s", __FILE__, oss.str().c_str()); \
            break; \
        case ::log4cplus::ERROR_LOG_LEVEL: \
            cout << oss.str() << endl; \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_ERR, "###### [ERROR] %s %s", __FILE__, oss.str().c_str()); \
            break; \
        case ::log4cplus::WARN_LOG_LEVEL: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_WARNING, "[WARN] %s %s", __FILE__, oss.str().c_str()); \
            break; \
        case ::log4cplus::INFO_LOG_LEVEL: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_INFO, "[INFO] %s %s", __FILE__, oss.str().c_str()); \
            break; \
        case ::log4cplus::DEBUG_LOG_LEVEL: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_DEBUG, "[DEBUG] %s %s", __FILE__, oss.str().c_str()); \
            break; \
        default: \
            if(AsnLogUtil::isSyslogEnabled()) syslog(LOG_INFO, "%s %s", __FILE__, oss.str().c_str()); \
            break; \
    } \
	logger.forcedLog(level, oss.str(), __FILE__, __LINE__); }}

#define ASN_TRACE_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,::log4cplus::TRACE_LOG_LEVEL)
#define ASN_DEBUG_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,::log4cplus::DEBUG_LOG_LEVEL)
#define ASN_INFO_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,::log4cplus::INFO_LOG_LEVEL)
#define ASN_WARN_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,::log4cplus::WARN_LOG_LEVEL)
#define ASN_ERROR_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,::log4cplus::ERROR_LOG_LEVEL)
#define ASN_FATAL_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,::log4cplus::FATAL_LOG_LEVEL)

#define ASN_TRACE_STREAM_END(logger) ASN_STREAM_END(logger,::log4cplus::TRACE_LOG_LEVEL)
#define ASN_DEBUG_STREAM_END(logger) ASN_STREAM_END(logger,::log4cplus::DEBUG_LOG_LEVEL)
#define ASN_INFO_STREAM_END(logger) ASN_STREAM_END(logger,::log4cplus::INFO_LOG_LEVEL)
#define ASN_WARN_STREAM_END(logger) ASN_STREAM_END(logger,::log4cplus::WARN_LOG_LEVEL)
#define ASN_ERROR_STREAM_END(logger) ASN_STREAM_END(logger,::log4cplus::ERROR_LOG_LEVEL)
#define ASN_FATAL_STREAM_END(logger) ASN_STREAM_END(logger,::log4cplus::FATAL_LOG_LEVEL)

#define ASN_IS_DEBUG_ENABLED(logger) logger.isEnabledFor(::log4cplus::DEBUG_LOG_LEVEL)
#define ASN_IS_INFO_ENABLED(logger)	 logger.isEnabledFor(::log4cplus::INFO_LOG_LEVEL)

#endif // #ifdef  _ASN_LOG_USE_LOG4CPLUS_	--------------------------------------------------------

#ifdef  _ASN_LOG_USE_LOG4CXX_	// ------------------------------------------------------------------

#include <log4cxx/logger.h>
#include <log4cxx/helpers/stringhelper.h>
#include <log4cxx/propertyconfigurator.h>

// TypeDef: AsnLoggerPtr
// Purpose: the ASN logger type
typedef log4cxx::LoggerPtr AsnLoggerPtr ;

/*
 * Macro: ASN_DEBUG, ASN_INFO, ASN_WARN, ASN_ERROR
 * Purpose: performs logging, depending on the desired log level
 * Parameter:
 *	- AsnLoggerPtr logger: the logger instance, previously initialized using ASN_GETLOGGER()
 *	- std::string: the message to be logged.
 */
#ifdef ASN_LOG_HAS_FUNCTION
#define ASN_LOG(logger,message,level) { \
if (logger->isEnabledFor(::log4cxx::Level::level)) {\
	::log4cxx::StringBuffer oss; \
	if (logger->isDebugEnabled()) \
		oss << __FUNCTION__ << "(): ";\
	oss << message; \
	if(level == ERROR || level == FATAL)\
	logger->forcedLog(::log4cxx::Level::level, oss.str(), __FILE__, __LINE__); }}

#define ASN_DEBUG(logger,message) ASN_LOG(logger,message,DEBUG)
#define ASN_INFO(logger,message)  ASN_LOG(logger,message,INFO)
#define ASN_WARN(logger,message)  ASN_LOG(logger,message,WARN)
#define ASN_ERROR(logger,message) ASN_LOG(logger,message,ERROR)
#define ASN_FATAL(logger,message) ASN_LOG(logger,message,FATAL)
#else
#define ASN_DEBUG(logger,message) LOG4CXX_DEBUG(logger,message)
#define ASN_INFO(logger,message)  LOG4CXX_INFO(logger,message)
#define ASN_WARN(logger,message)  LOG4CXX_WARN(logger,message)
#define ASN_ERROR(logger,message) LOG4CXX_ERROR(logger,message)
#define ASN_FATAL(logger,message) LOG4CXX_FATAL(logger,message)
#endif

/*
 * ASN_DEBUG_STREAM_BEGIN, ASN_DEBUG_STREAM_END....
 * Purpose: perform streamed logging on the desired log level
 * Parameters:
 *	- AsnLoggerPtr logger: the logger instance, previously initialized using ASN_GETLOGGER()
 * Usage:
 * 	ASN_DEBUG_STREAM_BEGIN(logger) << "Message" + string1 ASN_DEBUG_STREAM_END(logger);
 *								   ^ must have << here             ^ no "<<" here!!!
 * 	ASN_INFO_STREAM_BEGIN(logger) << "Message" + string1 ASN_DEBUG_STREAM_END(logger);
  */

#ifdef ASN_LOG_HAS_FUNCTION
#define ASN_STREAM_BEGIN(logger,level) {\
if (logger->isEnabledFor(::log4cxx::Level::level)) {\
::log4cxx::StringBuffer oss; \
	if (logger->isDebugEnabled()) \
		oss << __FUNCTION__ << "(): "; \
	oss
#else
#define ASN_STREAM_BEGIN(logger,level) {\
if (logger->isEnabledFor(::log4cxx::Level::level)) {\
::log4cxx::StringBuffer oss; \
	oss
#endif

#define ASN_STREAM_END(logger,level) ; \
	logger->forcedLog(::log4cxx::Level::level, oss.str(), __FILE__, __LINE__); }}

#define ASN_DEBUG_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,DEBUG)
#define ASN_INFO_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,INFO)
#define ASN_WARN_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,WARN)
#define ASN_ERROR_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,ERROR)
#define ASN_FATAL_STREAM_BEGIN(logger) ASN_STREAM_BEGIN(logger,FATAL)

#define ASN_DEBUG_STREAM_END(logger) ASN_STREAM_END(logger,DEBUG)
#define ASN_INFO_STREAM_END(logger) ASN_STREAM_END(logger,INFO)
#define ASN_WARN_STREAM_END(logger) ASN_STREAM_END(logger,WARN)
#define ASN_ERROR_STREAM_END(logger) ASN_STREAM_END(logger,ERROR)
#define ASN_FATAL_STREAM_END(logger) ASN_STREAM_END(logger,FATAL)

#define ASN_IS_DEBUG_ENABLED(logger) logger->isDebugEnabled()
#define ASN_IS_INFO_ENABLED(logger) logger->isInfoEnabled()

#endif // _ASN_LOG_USE_LOG4CXX_ -----------------------------------------------------------------

// COMMON --------------------------------------------------------------------------------
/*
 * Macro: ASN_GET_LOGGER
 * Purpose: get the logging pointer (AsnLoggerPtr) for later logger
 * Parameters:
 *	- const char* category: log category name
 */
#define ASN_GETLOGGER(category) AsnLogUtil::getLogger(category)

/*
 * Macro: ASN_INITLOG
 * Purpose: initializat logger with the specified log file.  This should
 *			be called from the main function only.
 *
 * Parameters:
 *	- std::string logconfig, a filename
 *
 * Note:
 * Sample File (which would turn on logging debug by default):

	log4j.rootLogger=DEBUG, logging
	log4j.appender.logging=org.apache.log4j.ConsoleAppender
	log4j.appender.logging.layout=org.apache.log4j.PatternLayout

	# Print the date in ISO 8601 format
	log4j.appender.logging.layout.ConversionPattern=%d{ISO8601} %5p [%t] %c{1}.%M(): %m%n

 */
#define ASN_INITLOG(logconfig) {\
    AsnLogUtil::initLog(logconfig); \
}

#define ASN_INITLOG_WITHDEBUG(logconfig, debug) {\
   AsnLogUtil::setDebug(debug); \
   AsnLogUtil::initLog(logconfig); }


#define ASN_INITLOG_WITHSYSLOG(logconfig, syslogname) {\
   AsnLogUtil::enableSyslog(syslogname, LOG_INFO); \
   AsnLogUtil::initLog(logconfig); \
}

#define ASN_INITLOG_WITHSYSLOG_DEBUGMODE(logconfig, syslogname) {\
   AsnLogUtil::enableSyslog(syslogname, LOG_DEBUG); \
   AsnLogUtil::initLog(logconfig); \
}


#define ASN_SHUTDOWNLOG() {\
    AsnLogUtil::shutdown(); \
}

#define ASN_DEBUGLOGGER(debug) AsnLogUtil::setDebug(debug);


#ifdef _ASN_LOG_USE_LOG4CPLUS_
#define ASN_CONFIG_CREATE_NDCCONTEXT(ndcString) {\
   AsnLogUtil::createNDCContext(ndcString);}
#define ASN_CONFIG_REMOVE_NDCCONTEXT() {\
   AsnLogUtil::removeNDCContext();}
#endif

class ThreadLocalData
{
public:
	ThreadLocalData():buf(NULL), index(0), length(1024)
   {
		buf = (char*)malloc(length);
   }
	~ThreadLocalData()
	{
		free(buf);
		buf = NULL;
	}
	char* buf;
	int index;
	int length;
};

class ASN_LOG_EXPORT AsnLogUtil {
	public:
	   static const std::string DEFAULT_LOG_PROPERTIES_FILE; // "asn.log.properties"
		static AsnLoggerPtr getLogger(const string category);
		static void initLog(const string logconfig, bool deft=false);
		static void setDebug(const bool debug);
		static void enableSyslog(string syslogname, int sysloglevel);
		static bool isSyslogEnabled();  //is syslog enable?
		static void shutdown();
      static void createNDCContext(const string contextString);
      static void removeNDCContext();
      static void addErrLog(const string& msg);
      static char* getErrLog();
      static void clearErrLog();
	private:
      static pthread_key_t* threadLocal;
		static AsnLoggerPtr s_rootLogger;
		static bool s_initDefault;
		static bool s_initProp;
	   static bool s_debug;
	   static bool s_syslogenable;
	   static string s_syslogname;
	   static int s_sysloglevel;
		static void createDefaultLogProp(const char* logconfig);
};



