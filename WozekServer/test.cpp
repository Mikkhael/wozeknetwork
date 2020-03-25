#include <iostream>
#include <string>
#include <array>
#include <boost\asio.hpp>

namespace asio = boost::asio;
using asio::ip::udp;
using asio::ip::tcp;

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

int main()
{
	runTest(8081);
	return 0;
}
