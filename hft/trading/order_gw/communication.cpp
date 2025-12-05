
//#include "common/AsnLog.h"
#include "communication.h"

using namespace std;
static AsnLoggerPtr logger = ASN_GETLOGGER("planning.communication");


shared_ptr<SyncHttpClient> SyncHttpClient::getInstance() //overwrite
{ 
    if (m_instance == nullptr) 
    {
        unique_lock<mutex> lock(m_mutex);
        if (m_instance == nullptr) 
        {
            //auto globalConfigInstance = GlobalConfig::getInstance();            
            m_instance = shared_ptr<SyncHttpClient>(new SyncHttpClient("wspap.okx.com", "8443"));
        }
    }
    return m_instance;
}

shared_ptr<AsyncHttpClient> AsyncHttpClient::getInstance() //overwrite
{ 
    if (m_instance == nullptr) 
    {
        unique_lock<mutex> lock(m_mutex);
        if (m_instance == nullptr) 
        {
            //auto globalConfigInstance = GlobalConfig::getInstance();            
            //m_instance = shared_ptr<AsyncHttpClient>(new AsyncHttpClient(globalConfigInstance->getIOServiceInstance()));
        }
    }
    return m_instance;
}


string SyncHttpClient::make_request(string requestHeader, string requestBody) {
    string request;
    request += requestHeader;
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + to_string(requestBody.length()) + "\r\n";            
    request += "\r\n";        
    request += requestBody;    
    return request;
}


string SyncHttpClient::request(string request) 
{
    try {
        std::string body_message;

        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        tcp::socket socket(io_context);
		
        tcp::resolver::results_type endpoints = resolver.resolve(host, port);
        boost::asio::connect(socket, endpoints);
		
        boost::asio::write(socket, boost::asio::buffer(request));

      	ASN_TRACE(logger, "Request sent to " << host << ":" << port << ", reading response...");
		
        boost::asio::streambuf response;
        boost::system::error_code error;
 
        boost::asio::read(socket, response, error);

        if (error == boost::asio::error::eof) {
            // remote connection is closed
        }
        else if (error) {
			ASN_ERROR(logger, "system error.");
            throw boost::system::system_error(error);
        }

        std::istream response_stream(&response);

        std::string http_version;
        response_stream >> http_version;
        unsigned int status_code;
        response_stream >> status_code;

        if (status_code != 200)
        {
			ASN_ERROR(logger, "Request error: " << status_code);
            return body_message;
        }
        if (http_version.substr(0, 5) != "HTTP/")
        {
			ASN_ERROR(logger, "It's not a HTTP response!");
            return body_message;
        }
        
        while (std::getline(response_stream, body_message)) {
            // read header and body in while loop
        }

        if (body_message.empty()) {
			ASN_ERROR(logger, "No response data!");
            return body_message;
        }

        return body_message;
        
    
    } catch (std::exception& e) {
        //std::cerr << "meet error: " << e.what() << std::endl;
		ASN_ERROR(logger, "meet error: " << e.what());
        return string();
    }
}
