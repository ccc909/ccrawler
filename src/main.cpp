
#include <ixwebsocket/IXWebSocketServer.h>
#include "Crawler.cpp"

using json = nlohmann::json;

int main(){
ix::initNetSystem();
int port = 9001;
std::string host("127.0.0.1");
ix::WebSocketServer server(port, host, ix::SocketServer::kDefaultTcpBacklog, 1);
Crawler *crawler;
bool isStarted = false;

server.setOnClientMessageCallback([&](std::shared_ptr<ix::ConnectionState> connectionState, ix::WebSocket& webSocket, const ix::WebSocketMessagePtr& msg) {
    std::cout << "Remote ip: " << connectionState->getRemoteIp() << std::endl;
    

    if (msg->type == ix::WebSocketMessageType::Open)
    {
        
        crawler = new Crawler(&webSocket);
        
        std::cout << "New connection" << std::endl;

        std::cout << "id: " << connectionState->getId() << std::endl;

        // The uri the client did connect to.
        std::cout << "Uri: " << msg->openInfo.uri << std::endl;

        std::cout << "Headers:" << std::endl;
        for (auto it : msg->openInfo.headers)
        {
            std::cout << "\t" << it.first << ": " << it.second << std::endl;
        }
    }
	else if (msg->type == ix::WebSocketMessageType::Message)
	{
        std::cout << "Received: " << msg->str << std::endl;

        std::string message = msg->str;

		if (!message.empty() && message.front() == '"' && message.back() == '"') {
			message = message.substr(1, message.size() - 2);
		}

        json jsonMsg;

        try
        {
            jsonMsg = json::parse(message);
        }
        catch (json::parse_error& ex)
        {
            json parseException;
            parseException["error"] = "parse_error";
            std::cout<<"Error parsing json: " << ex.what() << std::endl;
            webSocket.sendText(parseException.dump());
            return;
        }

        if (jsonMsg["action"] == "start") {
            isStarted = true;
            bool domainsOnly = false;
            bool ignoreRobots = false;

            if (jsonMsg.contains("params")) {
				for (auto& [key, value] : jsonMsg["params"].items()) {
					if (key == "domainsOnly") {
						domainsOnly = value;
					}
					else if (key == "ignoreRobots") {
						ignoreRobots = value;
					}
				}
			}

            std::string domain = jsonMsg["domain"];

            crawler->setParams(ignoreRobots, domainsOnly);

            crawler->queueFromWebsocket(domain);
        }
        else if (jsonMsg["action"] == "stop") {
            if (!isStarted) {
                return;
            }
                
            
			json stopstatus;
			stopstatus["action"] = "stop_start";
            webSocket.sendText(stopstatus.dump());
			crawler->stop();
            stopstatus["action"] = "stop_end";
            webSocket.sendText(stopstatus.dump());
			delete crawler;
			crawler = nullptr;
			crawler = new Crawler(&webSocket);
            isStarted = false;
		}

		/*if (msg->str == "stop")
		{
			crawler->stop();
			delete crawler;
			crawler = nullptr;
			crawler = new Crawler(&webSocket);
		}
		else {
			crawler->queueFromWebsocket(message);
		}*/
	}
    else if (msg->type == ix::WebSocketMessageType::Close)
    {
		std::cout << "Closed connection" << std::endl;
		crawler->stop();
		delete crawler;
        crawler = nullptr;
	}
    else if (msg->type == ix::WebSocketMessageType::Error)
    {
		std::cout << "Error: " << msg->errorInfo.reason << std::endl;
	}
});



auto res = server.listen();
if (!res.first)
{
    std::cerr << "Error occurred while listening: " << res.second << std::endl;
    return 1;
}

server.start();

server.wait();


}
