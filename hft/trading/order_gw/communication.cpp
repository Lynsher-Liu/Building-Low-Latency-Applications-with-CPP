
#include "AsnLog.h"
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
            auto globalConfigInstance = GlobalConfig::getInstance();            
            m_instance = shared_ptr<SyncHttpClient>(new SyncHttpClient(globalConfigInstance->getCCMSServerIP(), to_string(globalConfigInstance->getCCMSServerPort())));
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
            auto globalConfigInstance = GlobalConfig::getInstance();            
            m_instance = shared_ptr<AsyncHttpClient>(new AsyncHttpClient(globalConfigInstance->getIOServiceInstance()));
        }
    }
    return m_instance;
}


shared_ptr<AsyncHttpServer> AsyncHttpServer::getInstance() //overwrite
{
    if (m_instance == nullptr) 
    {
        unique_lock<mutex> lock(m_mutex);
        if (m_instance == nullptr) 
        {
            auto globalConfigInstance = GlobalConfig::getInstance();            
            m_instance = shared_ptr<AsyncHttpServer>(new AsyncHttpServer(globalConfigInstance->getPathPlanningServiceIP(), 
                globalConfigInstance->getPathPlanningServicePort(), globalConfigInstance->getIOServiceInstance()));
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



void SyncHttpServer::response() {
    try {
        tcp::endpoint endpoint(boost::asio::ip::address_v4::from_string(IP), port);
        tcp::acceptor acceptor(io_context, endpoint);

		ASN_DEBUG(logger, "Listening on " << endpoint.address().to_string() << ":" << endpoint.port() );
		
        while (true) {
            tcp::socket socket(io_context);
            acceptor.accept(socket);

            boost::asio::streambuf request;
            boost::asio::read_until(socket, request, "\r\n\r\n");


            std::string method, path, http_version;
            std::istream request_stream(&request);
            request_stream >> method >> path >> http_version;
			ASN_DEBUG(logger, "method: " << method << " , path: " << path << " ,http_version: " << http_version);

            if (method == "GET") {
                //处理GET请求
                std::string data = "{\"aaaaaa\":\"bbbbbbb\"}";
                std::string response_string = make_response(path, data);
				ASN_DEBUG(logger, "sending response_string: \n" << response_string);
                boost::asio::write(socket, boost::asio::buffer(response_string));
            } else if (method == "POST") {
                //处理POST请求
                boost::asio::read_until(socket, request, "\r\n\r\n");
                std::string data((std::istreambuf_iterator<char>(&request)), std::istreambuf_iterator<char>());

                std::string response_data = "{\"status\":\"ok\",\"data\":";
                response_data += data.substr(data.find("{")); //从请求数据中获取json数据，构造响应数据
                response_data += "}";

                std::string response_string = make_response(path, response_data);
				ASN_DEBUG(logger, "sending response_string: " << response_string);
                boost::asio::write(socket, boost::asio::buffer(response_string));
            }
        }

    } catch (std::exception& e) {
		ASN_ERROR(logger, "Meet error =  " <<e.what());
    }
}

//size_t AsyncHttpRequest::seq_num = 0;


void HttpRouter::register_route(string url_regex, 
		string request_method, 
		void (*callback)(Request*, Response*) )
{
	// set a Route object to be pushed into the routes vector
	HttpRoute route;
	route.url_regex = url_regex;
	route.request_method = request_method;
	route.callback = callback;
	routes.push_back(route);	
}


void HttpRouter::route_request(Request* req, Response* res)
{
	for (auto& r : routes) {
		// match request path with route regex
		regex pat {r.url_regex};
        regex reg {req->method};
		smatch match;

		if (std::regex_match(req->path, match, pat) 				
                && std::regex_search(r.request_method, match, reg)) {
			r.callback(req, res);
			break;
		}
	}
}

void AsyncHttpServer::loop()
{
    constructRouter();

    do_accept();
    //io_service_.run();  // run it in seperated ioServiceThreads in main()
}

void AsyncHttpServer::constructRouter() {
    router->register_route("/pathplanning/single", "POST", handle_request_for_single);
    router->register_route("/pathplanning/mapf", "POST", handle_request_for_mapf);
    router->register_route("/pathplanning/topologyparser", "POST", handle_request_for_topologyparser);
    router->register_route("/pathplanning/post/agv_list", "POST", handle_request_for_agvlist);
    router->register_route("/pathplanning/post/agvtrafficfree", "POST", handle_request_for_agvtrafficfree);
}


void AsyncHttpSession::do_read_header() {
    auto self = shared_from_this();
    /*
        成员函数, 接收一次数据, 收到多少是多少
        为什么要捕获this?为了直接调用成员函数do_write和变量data_
        为什么要捕获self?为了延长对象生命周期
        断开连接,boost::asio内部会将shared_ptr的引用计数降到0

        异步监听,当有信息发过来,就调用lambda
    */
    //buf.prepare(1024); // resize boost::asio::streambuf buf in case the header is larger than default 512

    //read http status line
    boost::asio::async_read_until(
        socket_,
        buf, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t bytes_transferred) {
            if (!ec) {               
                std::istream response_stream(&buf);
                string HTTP_method;
                response_stream >> HTTP_method;
                string request_path;
                response_stream >> request_path;
                string http_version;
                response_stream >> http_version;				
					ASN_DEBUG(logger, "HTTP_method: " << HTTP_method << ", request_path: " 
                    << request_path << ", http_version: "  << http_version);
                if (http_version.substr(0, 5) != "HTTP/")
                {                   
					ASN_ERROR(logger, "Invalid request");
                    return;
                }		
				
                //read http headers
                //async_read_until always read more bytes than what terminator sets like \r\n\r\n
                boost::asio::async_read_until(
                    socket_,
                    buf, "\r\n\r\n",
                    [this, self, HTTP_method, request_path](boost::system::error_code ec, std::size_t bytes_transferred) {
                        if (!ec)
                        {                            
	                          int content_length = 0;	   
							  bool bFindContentLength = false;
						      std::istream read_buf_stream(&buf);
						      std::string header;
						      while (std::getline(read_buf_stream, header, '\n'))
					      	  {								
					      	  	if (header.length() > 15) 
								{
	                                if (header.substr(0, 14) == "Content-Length")
									{
	                                    content_length = stoi(header.substr(16, header.length()-1));
										ASN_TRACE(logger, "Content-Length = " << content_length);
										bFindContentLength = true;
	                                }
	                            } 
								
								if((bFindContentLength == true) &&(header=="\r"))
								{
									break;
								}
					      	  }
							  
                            std::string bodyStr(boost::asio::buffers_begin(buf.data()), boost::asio::buffers_end(buf.data()));
                            buf.consume(buf.size());																				

							int bodyStrLen = bodyStr.length();
                            if (bodyStrLen != content_length) { 
                                // the body is not completely read in async_read_until, read remaining body data
                                do_read_body(request_path, HTTP_method, bodyStr, content_length);
                            }
                            else {
                                // the body is completely read in async_read_until
                                do_write(request_path, HTTP_method, bodyStr); 
                            }                                                      
                        }
                        else
                        {
							ASN_ERROR(logger,  "Error in reading http header, rnrn': " << ec);
                        }
                    }
                );
            }
            else
            {            
				ASN_ERROR(logger,  "Error in reading http status line, rn': " << ec);
            }
        }
    );
}


void AsyncHttpSession::do_read_body(string requestPath, string http_method, string body_already_read, int content_length) {
    auto self(shared_from_this()); 
	
    boost::asio::async_read(socket_, boost::asio::buffer(buffer_), boost::asio::transfer_at_least(1),
        [this, self, requestPath, http_method, body_already_read, content_length] (boost::system::error_code ec, std::size_t bytes_transferred) mutable {
            if (!ec)
            {
                string remaining_body(buffer_.data(), bytes_transferred);
				
                body_already_read.append(remaining_body); 
				
				ASN_DEBUG(logger,  "++++++Receive TCP packge, totally received http body's length = "<<body_already_read.length());
				int readLen = body_already_read.length();
                if (readLen < content_length) { 
                    // already received body is not completed, continue read
                    do_read_body(requestPath, http_method, body_already_read, content_length);
                }  
                else {                    
                    do_write(requestPath, http_method, body_already_read); 
                }                                                                                                                                            
            }
            else
            {
				ASN_ERROR(logger,  "Error in do_read_body: " << ec);
            }
        }
    );
}


void AsyncHttpSession::do_write(string requestPath, string http_method, string requestBody) {
	ASN_DEBUG(logger,  "Enter, requestBody = \n"<<requestBody<< "\n");
    auto self(shared_from_this());
    
    string response = do_process(requestPath, http_method, requestBody); 
    ASN_DEBUG(logger,  "Enter, response = \n"<< response << "\n");

    try
    {
        boost::asio::async_write(socket_, boost::asio::buffer(response),
                             [this, self](boost::system::error_code ec, std::size_t) {
        if (!ec) {
            ASN_INFO(logger,  "Response sent success!");
        }
        else
        {
            ASN_ERROR(logger,  "Response sent failed!");
        }
        }); 
    }
    catch(const std::exception& e)
    {
        ASN_DEBUG(logger,  "exception in async_write: \n"<< e.what() << "\n");
    }
      
}


string AsyncHttpSession::do_process(string requestPath, string http_method, string requestBody) {
    Request req;
	Response res;
	req.method = http_method;
	req.path = requestPath;
    req.body = requestBody;

    router_->route_request(&req, &res);

    if (res.body.empty()) 
        return make_response("ERROR: No route found for this http request", "HTTP/1.1 404 Not Found");
    
    return res.body;
}



void handle_request_for_single(Request* req, Response* res) {
    if (req->body.empty())  
        res->body = make_response("ERROR: Request body not Found", "HTTP/1.1 404 Not Found");   
    
    json requestBodyJson;
    json responseBodyJson;  
    string topologyVersion;
    shared_ptr<SinglePlannerContext> ptrPlanContext;
    PlannerType plannerType;   
    
    auto ptrTaskManager = task_manager::TaskManager::getInstance();
    
    try 
    {
        requestBodyJson = json::parse(req->body);
        string requestType = requestBodyJson.at("type");
        string chassisType = requestBodyJson.at("chassisType");
        topologyVersion = requestBodyJson.at("topologyVersion");
      
        if (requestType == "findPath") {            
            if (chassisType == "forkLift") {                
                plannerType = PlannerType::motionConstraint;
                //ptrPlanContext = make_shared<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                    //requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType);      
                    ptrPlanContext = ptrTaskManager->produceSharedPtr<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                    requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType);   
            }

            else if (chassisType == "differential") {
                plannerType = PlannerType::aStarBasic;
                //ptrPlanContext = make_shared<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                    //ORIENTATION_0, ORIENTATION_0, plannerType);
                ptrPlanContext = ptrTaskManager->produceSharedPtr<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                    ORIENTATION_0, ORIENTATION_0, plannerType);
            }

            else {
                res->body = make_response("ERROR: Unknow chassisType for: " + requestType, "HTTP/1.1 400 Bad Request");  
                return;
            }
        }

        else if (requestType == "loadGoods") {
            if (chassisType == "forkLift") {                
                plannerType = PlannerType::loadGoods;
                
                string beforeGoal;
                if (!requestBodyJson.contains("beforeGoal")) 
                    beforeGoal = string();                
                else 
                    beforeGoal = requestBodyJson.at("beforeGoal");

                // ptrPlanContext = make_shared<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                //     requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType,
                //     string(), string(), string(), false, beforeGoal, requestBodyJson.at("height"));
                ptrPlanContext = ptrTaskManager->produceSharedPtr<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                    requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType,
                    string(), string(), string(), false, beforeGoal, requestBodyJson.at("height"));
            }
            else {
                res->body = make_response("ERROR: Only support chassisType: forklift", "HTTP/1.1 400 Bad Request");  
                return;
            }
        }

        else if (requestType == "unloadGoods") {
            if (chassisType == "forkLift") {             
                plannerType = PlannerType::unloadGoods;

                string beforeGoal;
                if (!requestBodyJson.contains("beforeGoal")) 
                    beforeGoal = string();                
                else 
                    beforeGoal = requestBodyJson.at("beforeGoal");

                // ptrPlanContext = make_shared<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                //     requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType,
                //     string(), string(), string(), false, beforeGoal, requestBodyJson.at("height")); 
                ptrPlanContext = ptrTaskManager->produceSharedPtr<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                    requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType,
                    string(), string(), string(), false, beforeGoal, requestBodyJson.at("height")); 
            }
            else {
                res->body = make_response("ERROR: Only support chassisType: forklift", "HTTP/1.1 400 Bad Request");  
                return;
            }
        }

        else if (requestType == "parking") {            
            if (chassisType == "forkLift") {                
                plannerType = PlannerType::motionConstraintForParking;
                // ptrPlanContext = make_shared<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                //     requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType); 
                ptrPlanContext = ptrTaskManager->produceSharedPtr<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                    requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType);
            }
            else {
                res->body = make_response("ERROR: Only support chassisType: forklift", "HTTP/1.1 400 Bad Request");  
                return;
            }
        }

        else if (requestType == "charging") {            
            if (chassisType == "forkLift") {                
                plannerType = PlannerType::motionConstraintForCharging;
                // ptrPlanContext = make_shared<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                //     requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType);   
                ptrPlanContext = ptrTaskManager->produceSharedPtr<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                    requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType);     
            }
            else {
                res->body = make_response("ERROR: Only support chassisType: forklift", "HTTP/1.1 400 Bad Request");  
                return;
            }
        }

        else if (requestType == "stopCharging") {            
            if (chassisType == "forkLift") {                
                plannerType = PlannerType::motionConstraintForStopCharging;
                // ptrPlanContext = make_shared<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                //     requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType);  
                ptrPlanContext = ptrTaskManager->produceSharedPtr<SinglePlannerContext>(requestBodyJson.at("start"), requestBodyJson.at("goal"), 
                    requestBodyJson.at("agvOrientationAtStart"), requestBodyJson.at("agvOrientationAtGoal"), plannerType);
            }
            else {
                res->body = make_response("ERROR: Only support chassisType: forklift", "HTTP/1.1 400 Bad Request");  
                return;
            }
        }

        else {
            res->body = make_response("ERROR: Unknow type", "HTTP/1.1 400 Bad Request");  
            return;
        }

        // add additional actions for taskChain
        if (requestBodyJson.contains("actions")) {
            ptrPlanContext->addiActions = requestBodyJson.at("actions");
        } 
                                           
    }
    catch (std::exception& e) {
        res->body = make_response("ERROR: Request body is wrong", "HTTP/1.1 400 Bad Request");
        return;
    }              
    
    auto globalConfigInstance = GlobalConfig::getInstance();
	auto ptrGraph = globalConfigInstance->getTopologyGraph();
    //std::future<int> calcCache_future;

    if ((nullptr == ptrGraph) || (topologyVersion != ptrGraph->graphVersion)) {
		ASN_INFO(logger,  "TopologyVersion between request and path planning service is unmatched! Requesting latest ...");

        auto syncHttpClientInstance = SyncHttpClient::getInstance();
        int ret = getTopologyByHTTP(globalConfigInstance->getTopologyRequest());
		if(ret != 0)
		{
			res->body = make_response("ERROR: Request latest topology graph failed, please check ", "HTTP/1.1 400 Bad Request");
        	return;
		}
        else
		{
			ASN_INFO(logger, "**********Get topology success**********");
		}

        ptrGraph = globalConfigInstance->getTopologyGraph();   
    }
    ptrPlanContext->topologyVersion = ptrGraph->graphVersion;

    shared_ptr<SinglePlanner> planner;
            
    switch (plannerType) {
        case motionConstraint: {                        
            //planner = make_shared<MotionConstraintPlanner>(ptrPlanContext, ptrGraph);
            planner = ptrTaskManager->produceSharedPtr<MotionConstraintPlanner>(ptrPlanContext, ptrGraph);
            ASN_INFO(logger,  "choose motionConstraint");
            break;
        }
        case aStarBasic: {
            //planner = make_shared<AStarBasicPlanner>(ptrPlanContext, ptrGraph);
            planner = ptrTaskManager->produceSharedPtr<AStarBasicPlanner>(ptrPlanContext, ptrGraph);
            ASN_INFO(logger,  "choose AStarBasicPlanner");
            break;
        }  
        case loadGoods: case unloadGoods: {
            //planner = make_shared<LoadAndUnloadPlanner>(ptrPlanContext, ptrGraph);
            planner = ptrTaskManager->produceSharedPtr<LoadAndUnloadPlanner>(ptrPlanContext, ptrGraph);
            ASN_INFO(logger,  "choose LoadAndUnloadPlanner");
            break;
        }  
        case motionConstraintForParking: {
            //planner = make_shared<MotionConstraintPlannerForParking>(ptrPlanContext, ptrGraph);
            planner = ptrTaskManager->produceSharedPtr<MotionConstraintPlannerForParking>(ptrPlanContext, ptrGraph);
            ASN_INFO(logger,  "choose MotionConstraintPlannerForParking");
            break;
        }    
        case motionConstraintForCharging: {
            //planner = make_shared<MotionConstraintPlannerForCharging>(ptrPlanContext, ptrGraph);
            planner = ptrTaskManager->produceSharedPtr<MotionConstraintPlannerForCharging>(ptrPlanContext, ptrGraph);
            ASN_INFO(logger,  "choose MotionConstraintPlannerForCharging");
            break;
        }   
        case motionConstraintForStopCharging: {
            //planner = make_shared<MotionConstraintPlannerForStopCharging>(ptrPlanContext, ptrGraph);
            planner = ptrTaskManager->produceSharedPtr<MotionConstraintPlannerForStopCharging>(ptrPlanContext, ptrGraph);
            ASN_INFO(logger,  "choose MotionConstraintPlannerForStopCharging");
            break;
        }  
        case SpaceTimeAStar: case SpaceTimeOrientationAStar: case SIPP: case SpaceTimeAStarForParking: case SpaceTimeAStarForCharging: {
            ASN_INFO(logger,  "current plannerType = " << eSinglePlannerTypeName[plannerType] << " has not been handled, return");
            return;
        }
    }
    
    //shared_ptr<SinglePlannerCaller> plannerCaller = make_shared<SinglePlannerCaller>(planner);
    shared_ptr<SinglePlannerCaller> plannerCaller = ptrTaskManager->produceSharedPtr<SinglePlannerCaller>(planner);

    string responseMsg;
    if (plannerCaller->checkPlannerContextValid(responseMsg) == false) {
		ASN_DEBUG(logger,  "checkPlannerContextValid failed, response HTTP/1.1 400 Bad Reques.");
        res->body = make_response(responseMsg, "HTTP/1.1 400 Bad Request"); 
        return;
    }

    vector<std::shared_ptr<Node>> pathNodes = plannerCaller->getPathNodes();
    json pathWithActions = plannerCaller->getFinalPathWithAction(pathNodes); 

#if 0
    // study single planning path and save into map for MAPF
    if (!pathNodes.empty()) {
        auto mapfInstance = mapf::MAPF::getInstance();
        mapfInstance->addPathToSinglePathMap(ptrPlanContext, pathNodes);
        ASN_INFO(logger, "add ptrPlanContext and result pathNodes into m_mapSinglePath - start: " << ptrPlanContext->start 
                << " goal: " << ptrPlanContext->goal
                << " startOrien: " << ptrPlanContext->agvOrientationAtStart << " goalOrien: " << ptrPlanContext->agvOrientationAtGoal
                << " topology version: " << ptrPlanContext->topologyVersion
                << " in m_mapSinglePath");
    }
#endif
    
    if (!pathWithActions.empty()) {                          
        responseBodyJson["status"] = "success";
        responseBodyJson["assigned_path"] = pathWithActions;            
    }
    else {
        responseBodyJson["status"] = "failed";
        responseBodyJson["msg"] = "failed to assign a path to the task";
    } 
    res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 200 OK"); 

    return;  
}


void handle_request_for_mapf(Request* req, Response* res) {	
    ASN_TRACE(logger, "Enter."); 

    auto globalConfigInstance = GlobalConfig::getInstance();
    if (!globalConfigInstance->getbOpenMapfService()) {
        res->body = make_response("ERROR: MAPF service is disable, please enable it in config file", "HTTP/1.1 400 Bad Request");
        ASN_ERROR(logger, "MAPF service is disable, please enable it in config file");
        return;
    }

    bool bUseEcbs = globalConfigInstance->getEcbsSwitch();
	
    json requestBodyJson;
    json tasks;
    string topologyVersion;
    json responseBodyJson; 
    PlannerType plannerType = PlannerType::motionConstraint;
    string chassisType;

    try {
        requestBodyJson = json::parse(req->body);        
        tasks = requestBodyJson.at("subTasks");
        topologyVersion = requestBodyJson.at("topologyVersion");
        chassisType = requestBodyJson.at("chassisType");
    }
    catch (exception& e) {
		ASN_ERROR(logger,  "except: " << e.what());
        responseBodyJson["status"] = "failed";
        responseBodyJson["msg"] = "Unable to add task requests, please check request params";    
        res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 400 Bad Request"); 
        return;
    } 

    auto ptrGraph = globalConfigInstance->getTopologyGraph();

    if ((nullptr == ptrGraph) 
        || ((topologyVersion != "none") && (topologyVersion != ptrGraph->graphVersion)) ) 
    {		
        // if not equal, request latest topology version from server
        ASN_INFO(logger,  "TopologyVersion between request and path planning service is unmatched! Requesting latest ...");

        auto syncHttpClientInstance = SyncHttpClient::getInstance();
        int ret = getTopologyByHTTP(globalConfigInstance->getTopologyRequest());
        if(ret != 0)
        {
            res->body = make_response("ERROR: Request topology graph failed, please check path planning config.", "HTTP/1.1 400 Bad Request");
            return;
        }
        ptrGraph = globalConfigInstance->getTopologyGraph();
    }

    shared_ptr<timer::MapfClock> mapfClockInstance = timer::MapfClock::getInstance();
    uint64_t curTime = mapfClockInstance->getCurrentMapfTime();
	
    auto ptrTaskManager = task_manager::TaskManager::getInstance();
    unordered_map<string, shared_ptr<task_manager::Task>> allValidTasks;
  
    for (unsigned int i = 0; i < tasks.size(); ++i)
    {
        try 
        {
            const json& task = tasks.at(i);
            string taskId = task.at("subTaskId");
            shared_ptr<SinglePlannerContext> ptrPlanContext = nullptr;
            string invalidMsg;

            json response;

            if (ptrTaskManager->checkVolume()) [[likely]]
            {
                int ret = mapf::procMapfTaskJson(task, chassisType, bUseEcbs, plannerType, curTime, ptrPlanContext, invalidMsg); 
                if (ret != 0)
                {
                    response["subtaskId"] = taskId;
                    response["msg"] = invalidMsg;
                    response["success"] = false;
                }
                else
                {
                    //string originTaskId = task.at("taskId");
                    string agvId = task.at("agvId");
                    string agvIp = task.at("agvIp");
                    string taskType = task.at("type");

                    //shared_ptr<task_manager::Task> ptrTask = make_shared<task_manager::Task>(taskId, agvId, agvIp, taskType, taskType, ptrPlanContext, curTime);
                    //shared_ptr<task_manager::Task> ptrTask = ptrTaskManager->produceTask(taskId, agvId, agvIp, taskType, taskType, ptrPlanContext, curTime);
                    shared_ptr<task_manager::Task> ptrTask = ptrTaskManager->produceSharedPtr<task_manager::Task>(taskId, agvId, agvIp, taskType, taskType, ptrPlanContext, curTime);
                    allValidTasks.emplace(taskId, ptrTask); 
                    ASN_INFO(logger,  "Add task = " << taskId);

                    response["subtaskId"] = taskId;
                    response["msg"] = "Add MAPF task success";
                    response["success"] = true;
                }
            }
            else
            {
                response["subtaskId"] = taskId;
                response["msg"] = "Add MAPF task failed";
                response["success"] = false;
            }
            
            responseBodyJson["subTasks"].push_back(response);
        }
        catch (exception& e) {
            ASN_ERROR(logger,  "exception: " << e.what()); 
        }   
    }

    ptrTaskManager->processNewAddedTasks(move(allValidTasks));

    res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 200 OK");      
	ASN_TRACE(logger, "Success leave.");
}


void handle_request_for_topologyparser(Request* req, Response* res)
{
    ASN_TRACE(logger, "Enter."); 

    auto globalConfigInstance = GlobalConfig::getInstance();
	
    json rawTopologyJson;
    json parsedTopologyJson;
    json responseBodyJson; 

    try {
        rawTopologyJson = json::parse(req->body);     
        ASN_INFO(logger, "Call makeTopologyGraphFromJsonData");    
        int ret = makeTopologyGraphFromJsonData(rawTopologyJson);
        if (ret != 0) {           
            ASN_ERROR(logger, "***** Unable to parse topology *****");
            responseBodyJson["status"] = "failed";
            responseBodyJson["msg"] = "Unable to parse topology, please check";    
            res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 400 Bad Request"); 
            return;
        }

        shared_ptr<TopologyGraph> ptrGraph = globalConfigInstance->getTopologyGraph();
        if (ptrGraph != nullptr)
        {
            parsedTopologyJson = saveParsedGraphAsJson4CCMS(ptrGraph);
            ASN_TRACE(logger,  "parsedTopologyJson: " <<parsedTopologyJson);

            if (parsedTopologyJson.is_null()) {    
                ASN_ERROR(logger, "*****Unable to save parsed topology as json*****");
                responseBodyJson["status"] = "failed";
                responseBodyJson["msg"] = "Unable to save parsed topolog, please check mapf";    
                res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 400 Bad Request"); 
                return;
            }

            //ASN_DEBUG(logger, "parsedTopologyJson = " << parsedTopologyJson);
            responseBodyJson = parsedTopologyJson;
            responseBodyJson["status"] = "success";   
            res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 200 OK"); 
            return;
        }

    }
    catch (exception& e) {
		ASN_ERROR(logger,  "except: " << e.what());
        responseBodyJson["status"] = "failed";
        responseBodyJson["msg"] = "Unable to parse raw topology, please check";    
        res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 400 Bad Request"); 
        return;
    } 
}


void handle_request_for_agvlist(Request* req, Response* res)
{
    ASN_TRACE(logger, "Enter."); 

    json requestBodyJson;
    json responseBodyJson; 
    json agvs;    

    try {
        requestBodyJson = json::parse(req->body);        
        agvs = requestBodyJson.at("agvs");
    }
    catch (exception& e) {
		ASN_ERROR(logger,  "except: " << e.what());
        responseBodyJson["status"] = "failed";
        responseBodyJson["msg"] = "Unable to parse agv list, please check";    
        res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 400 Bad Request"); 
        return;
    } 

    auto ptrAgvManager = agv::agvManager::getInstance();
    auto mapf_instance = mapf::MAPF::getInstance();
    shared_ptr<MqttClientAsync> mqtt_instance = MqttClientAsync::getInstance();
    
    for (unsigned int i = 0; i < agvs.size(); ++i)
    {
        try 
        {
            json& agv = agvs.at(i);
            string agvIp = agv.at("agvIp");
            string operation = agv.at("operation");

            if (operation == "delete")
            {
                bool ret = ptrAgvManager->deleteAGVStatus(agvIp);
                if (ret)
                {
                    string topic = "task/assign/" + agvIp;
                    mqtt_instance->unSubsTopic(topic);
                    ASN_INFO(logger, "agv = " << agvIp << " is deleted from existing agv list");
                }
                
            }
            else if (operation == "add")
            {
                std::shared_ptr<agv::AgvStatus> ptrAgvStatus = ptrAgvManager->getAGVStatus(agvIp);
                if (ptrAgvStatus == nullptr)
                {
                    ptrAgvStatus = ptrAgvManager->createAGVStatus(agvIp);
                    if (ptrAgvStatus != nullptr) 
                    {
                        mapf_instance->addAgvToSubsTopicQ(ptrAgvStatus);
                        ASN_INFO(logger, "agv = " << agvIp << " not exists in existing agv list, add it");
                    }                    
                }
            }            
        }
        catch(exception& e)
        {
            ASN_ERROR(logger,  "exception: " << e.what()); 
            responseBodyJson["status"] = "failed";
            responseBodyJson["msg"] = "Unable to post agv list";    
            res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 400 Bad Request"); 
            return;
        }
    }

    responseBodyJson["msg"] = "Post agv list success";
    responseBodyJson["status"] = "success";   
    res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 200 OK"); 
    return;
}

void handle_request_for_agvtrafficfree(Request* req, Response* res)
{
    ASN_TRACE(logger, "Enter."); 

    json requestBodyJson;
    json responseBodyJson; 
    string agvIp;
    string msg;    

    try {
        requestBodyJson = json::parse(req->body);        
        agvIp = requestBodyJson.at("agvIp");
        msg = requestBodyJson.at("msg");
    }
    catch (exception& e) {
		ASN_ERROR(logger,  "except: " << e.what());
        responseBodyJson["status"] = "failed";
        responseBodyJson["msg"] = "Failed to agv traffic free, please check";    
        res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 400 Bad Request"); 
        return;
    } 

    auto ptrAgvManager = agv::agvManager::getInstance();
    std::shared_ptr<agv::AgvStatus> ptrAgvStatus = ptrAgvManager->getAGVStatus(agvIp);
    if (ptrAgvStatus)
    {
        // agv is offline, no need to lock here
        ptrAgvStatus->clearLockedVertexsAndStatus();
    }
    
    responseBodyJson["status"] = "success"; 
    responseBodyJson["msg"] = "Post agv traffic free success";  
    res->body = make_response(responseBodyJson.dump(), "HTTP/1.1 200 OK"); 
    return;
}


int getTopologyByHTTP(string request) {
    try 
    {
        auto client = SyncHttpClient::getInstance(); 
        string topologyString = client->request(request);

        if (topologyString == string()) {
            ASN_WARN(logger, "Request topology graph error, return graph is nullptr.");
            return -1;
        }

        auto globalConfigInstance = GlobalConfig::getInstance();	
        shared_ptr<TopologyGraph> ptrGraph = globalConfigInstance->getTopologyGraph();

        json topologyData = json::parse(topologyString);
        string topologyVersion = topologyData.at("topology").at("version");
    
        if (topologyData.at("success") == true) {
			ASN_TRACE(logger, "Get latest topology version success. " );

            if (ptrGraph == nullptr || ptrGraph->graphVersion != topologyVersion)
            {
                ASN_INFO(logger, "Map version in MAPF and CCMS is different, build latest..." );
                int ret = makeTopologyGraphFromJsonData(topologyData);
                return ret;
            }
            else
            {
                ASN_INFO(logger, "Map version in MAPF and CCMS is the same, no need to update" );
                return 0;
            }
        }
        else {
			ASN_WARN(logger, "Request topology graph error, return grap is nullptr.");
            return -1;
        }
    }   
    catch (exception& e) {
		ASN_WARN(logger, "Exception occurs when parsing topology map...");       
        return -1;
    }   
}

int getAgvAndTaskFromCCMS()
{
    auto globalConfigInstance = GlobalConfig::getInstance(); 
    string request = globalConfigInstance->getAgvAndTaskRequest();
    
    auto ptrGraph = globalConfigInstance->getTopologyGraph();

    auto client = SyncHttpClient::getInstance();
    auto ptrAgvManager = agv::agvManager::getInstance(); 
    auto ptrTaskManager = task_manager::TaskManager::getInstance();
    auto mapf_instance = mapf::MAPF::getInstance();
    shared_ptr<timer::MapfClock> mapfClockInstance = timer::MapfClock::getInstance();

    string agvAndTaskData = client->request(request);
    if (agvAndTaskData.empty()) 
    {
        ASN_ERROR(logger, "Request agv and task from CCMS error");
        return -1;
    }
    
    try
    {   
        json agvAndTaskJson = json::parse(agvAndTaskData);
        if (agvAndTaskJson.at("status") == "success")
        {            
            json agvs = agvAndTaskJson.at("agvs");
            for (unsigned int i = 0; i < agvs.size(); ++i)
            {
                json agv = agvs.at(i);
			    string agvIp = agv.at("agvIp");
                   
                std::shared_ptr<agv::AgvStatus> ptrAgvStatus = ptrAgvManager->getAGVStatus(agvIp);
                if (ptrAgvStatus == nullptr)
                {
                    ptrAgvManager->createAGVStatus(agvIp);
                    ptrAgvStatus = ptrAgvManager->createAGVStatus(agvIp);  
                    if (ptrAgvStatus != nullptr) 
                    {
                        mapf_instance->addAgvToSubsTopicQ(ptrAgvStatus);
                    }
                    ASN_DEBUG(logger, "add agv = " << agvIp << " into agv list and subscribe mqtt");
                }
            }
#if 0
            json executingTasks = agvAndTaskJson.at("executingTasks");
            ASN_INFO(logger, "json executingTasks.size = "<<executingTasks.size());
            bool bUseEcbs = globalConfigInstance->getEcbsSwitch();

            for (unsigned int i = 0; i < executingTasks.size(); ++i)
            {          
                const json& task = executingTasks.at(i);
                ASN_DEBUG(logger, "restore task = " << task);

                string taskId = task.at("subTaskId");
                string chassisType = task.at("chassisType");
                PlannerType plannerType{PlannerType::motionConstraint};
                shared_ptr<SinglePlannerContext> ptrPlanContext = nullptr;
                string invalidMsg;
                
                int ret = mapf::procMapfTaskJson(task, chassisType, bUseEcbs, plannerType, curTime, ptrPlanContext, invalidMsg); 
                if (ret == 0)
                {                    
                    string agvId = task.at("agvId");
                    string agvIp = task.at("agvIp");
                    string taskType = task.at("type");

                    // TODO: need to restore task status, lastFinishedIndex, ... from related AGV
                    shared_ptr<task_manager::Task> ptrTask = ptrTaskManager->produceTask(taskId, agvId, agvIp, taskType, taskType, ptrPlanContext, curTime);
                    ptrTaskManager->addToExecutingTaskMap(ptrTask);
                }
                else
                {
                    ASN_ERROR(logger, "failed to restore task = "<< taskId << ", msg = " << invalidMsg);
                }                                                            
            } 
#endif
        }
        else 
        {
            ASN_ERROR(logger, "Request agv and task from CCMS failed");
            return -1;
        }
    }
    catch(const std::exception& e)
    {
        ASN_WARN(logger, "Exception occurs when parsing agv or task: " << e.what());       
        return -1;
    }

    return 0;
}


string make_response(string responseBody, string responseHttpStatus) {
    string response;
    response += responseHttpStatus + "\r\n";
    response += "Content-Type: application/json\r\n";
    response += "Content-Length: " + to_string(responseBody.length()) + "\r\n";            
    response += "\r\n";        
    response += responseBody;    
    return response;
}
