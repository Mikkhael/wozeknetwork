/*
#include <iostream>
#include <string>
#include <array>
#include <memory>
#include <boost\asio.hpp>

namespace test__
{

namespace asio = boost::asio;
using asio::ip::udp;
using asio::ip::tcp;
/*
void runTest(short port)
{
	try
	{
		boost::system::error_code err;
		asio::io_context ioContext;
		
		tcp::acceptor acceptor(ioContext, tcp::endpoint(tcp::v4(), port));
		
		std::cout << "Server started\n";
		
		std::string message;
		
		while(true)
		{
			
			message.resize(1 << 17);
			std::cout << "Awaiting connection \n";
			
			tcp::endpoint peerEndpoint;
			tcp::socket peerSocket(ioContext);
			acceptor.accept(peerSocket, peerEndpoint);
			
			std::cout << "Accepted connection from " << peerEndpoint << '\n';
			std::cout << "Available before wait: " << peerSocket.available() << '\n';
			
			asio::steady_timer timer(ioContext, std::chrono::seconds(2));
			timer.wait();
			
			std::cout << "Available after wait: " << peerSocket.available() << '\n';
			size_t receivedBytes = peerSocket.read_some(asio::buffer(message), err);
			if(err)
			{
				std::cout << "Error occured while receiving\n: [" << err.value() << "] " << err.message() << '\n';
				continue;
			}
			std::cout << "Received message of length " << receivedBytes << '\n';
			std::cout << "Available after read: " << peerSocket.available() << '\n';
						
			timer.expires_after(std::chrono::seconds(2));
			timer.wait();
			
			std::cout << "Available after read and wait: " << peerSocket.available() << '\n';
			
			timer.expires_after(std::chrono::seconds(2));
			timer.wait();
			
			std::cout << "Writing...\n";
			asio::write(peerSocket, asio::buffer(message, receivedBytes), err);
			if(err)
			{
				std::cout << "Error dufing transmission:\n  [" << err.value() << "] " << err.message() << '\n'; 
			}
			else
			{
				std::cout << "Message send successfully\n";
			}
			
		}
		
	}
	catch(boost::system::error_code& err)
	{
		std::cout << "ERROR: " << err.message();
	}
}

void showError(const boost::system::error_code err)
{
	std::cout << " [" << err.value() << "] " << err.message() << '\n';
}


class EchoHandler
	: public std::enable_shared_from_this<EchoHandler>
{
	asio::io_context& ioContext;
	tcp::socket socket;
	
	static constexpr size_t bufferSize = 3000;
	
	std::string messageBuffer = std::string(bufferSize, 0);
	
public:
	
	EchoHandler(asio::io_context& ioContext_)
		: ioContext(ioContext_), socket(ioContext_)
	{
	}
	
	void start()
	{
		receiveMessage();
	}
	
	tcp::socket& getSocket()
	{
		return socket;
	}
	
private:
	
	void receiveMessage()
	{
		std::cout << "Begin receive\n";
		socket.async_read_some(asio::buffer(messageBuffer),
			[this, me = shared_from_this()](auto err, size_t bytesSize){
				if(err)
				{
					std::cout << "Error while receiving:\n";
					showError(err);
					return;
				}
				sendMessage(bytesSize);
			}
		);
	}
	
	void sendMessage(size_t bytesSize = 0)
	{
		std::cout << "Begin send\n";
		asio::async_write(socket, asio::buffer(messageBuffer, bytesSize), 
			[this, me = shared_from_this()](auto err, size_t bytesLength){
				if(err)
				{
					std::cout << "Error while receiving:\n";
					showError(err);
					return;
				}
				receiveMessage();
			}
		);
	}
	
};

template <typename ConnectionHandler>
class ServerTCP
{
	asio::io_context& ioContext;
	tcp::acceptor acceptor;
	
	using HandlerPointer = std::shared_ptr<ConnectionHandler>;
	
public:
	
	ServerTCP(asio::io_context& ioContext_)
		: ioContext(ioContext_), acceptor(ioContext)
	{
	}
	
	bool start(uint16_t port)
	{
		tcp::endpoint localEndpoint(tcp::v4(), port);
		acceptor = tcp::acceptor(ioContext, localEndpoint);
		
		awaitNewConnection();
		return true;
	}
	
private:
	
	void awaitNewConnection()
	{
		auto handler = std::make_shared<ConnectionHandler>(ioContext);
		
		acceptor.async_accept( handler->getSocket(), [=](auto err){
			handleNewConnection(handler, err);
		});
		
		std::cout << "Awaiting new connection...\n";
	}
	
	
	void handleNewConnection(HandlerPointer handler, const boost::system::error_code& err)
	{
		if(err)
		{
			std::cout << "Error while new connection:\n";
			showError(err);
		}
		else
		{
			handler->start();
		}
		
		awaitNewConnection();
	}
	
};

void runAsync()
{
	asio::io_context ioContext;
	ServerTCP<EchoHandler> server(ioContext);
	if(server.start(8081))
	{
		std::cout << "Server running\n";
	}
	
	ioContext.run();
}

}

/*
int main()
{
	//runTest(8081);
	test__::runAsync();
	return 0;
}
*/
