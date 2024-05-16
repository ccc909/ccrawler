#include <ixwebsocket/IXWebSocketServer.h>
#include <iostream>
#include "main.cpp" 

int main(){
ix::initNetSystem();
int port = 9001;
std::string host("127.0.0.1");
ix::WebSocketServer server(port, host);
Crawler *crawler;

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
        std::string message = msg->str;

		if (!message.empty() && message.front() == '"' && message.back() == '"') {
			message = message.substr(1, message.size() - 2);
		}

		std::cout << "Received: " << msg->str << std::endl;

		if (msg->str == "stop")
		{
			crawler->stop();
            delete crawler;
            crawler = nullptr;
            crawler = new Crawler(&webSocket);
		}
        else {
            crawler->queueFromWebsocket(message);
        }
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
