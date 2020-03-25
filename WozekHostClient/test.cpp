#include <iostream>
#include <string>
#include <array>
#include <boost\asio.hpp>

namespace asio = boost::asio;
using asio::ip::udp;
using asio::ip::tcp;


void runSingleTest(const std::string& host, const std::string& port, const std::string& messageToSend)
{
	try
	{
		boost::system::error_code err;
		asio::io_context ioContext;
		
		tcp::resolver resolver(ioContext);
		auto resolvedEndpoints = resolver.resolve(host, port);
		
		if(resolvedEndpoints.empty())
		{
			std::cout << "Host cannot be resolved\n";
			return;
		}
		
		std::cout << "Resolved endpoints:\n";
		for(auto& result : resolvedEndpoints)
		{
			std::cout << "  " << result.endpoint() << '\n';
		}
		
		
		tcp::socket socket(ioContext);
		tcp::endpoint connectedEndpoint = asio::connect(socket, resolvedEndpoints, err);
		
		if(err)
		{
			std::cout << "Error while connecting:\n  [" << err.value() << "] " << err.message() << '\n';
			return;
		}
		
		std::cout << "Socket connected to: " << connectedEndpoint << '\n';
		asio::steady_timer timer(ioContext, std::chrono::seconds(1));
		timer.wait();
		
		std::cout << "Writing... (size: " << messageToSend.size() << "\n";
		asio::write(socket, asio::buffer(messageToSend), err);
		
		if(err)
		{
			std::cout << "Error while sending message:\n  [" << err.value() << "] " << err.message() << '\n';
			return;
		}
		
		std::cout << "Message send successfully\n";
		
		
		std::string receivedMessage;
		receivedMessage.resize(1 << 8);
		
		std::cout << "Available to read: " << socket.available() << '\n';
		size_t receivedBytes = asio::read(socket, asio::buffer(receivedMessage), err);
		
		if(err)
		{
			if(err == asio::error::eof)
			{
				std::cout << "Reciving transmision completed\n";
			}
			else
			{
				std::cout << "Error occured while receiving\n: [" << err.value() << "] " << err.message() << '\n';
				return;
			}
		}
		else
		{
			std::cout << "Receiving transmision not completed. Buffer size to small\n";
		}
		
		
		receivedMessage.resize(receivedBytes);
		std::cout << "Received bytes length: " << receivedBytes << '\n';
		std::cout << "Received message: " << receivedMessage << '\n';
		std::cout << "Available to read after read: " << socket.available() << '\n';
	}
	catch(std::exception& err)
	{
		std::cout << "ERROR: " << err.what() << '\n';
	}
}

void runTest(const std::string& host, size_t s = (1 << 16) + (1 << 15))
{
	while(true)
	{
		size_t testStringSize = s;
		std::string testString;
		testString.resize(testStringSize);
		for(size_t i = 0; i < testString.size(); i++)
		{
			testString[i] = 'a' + (i % ('z'-'a'));
		}
		
		runSingleTest(host, "8081", testString);
		
		std::cout << "Press any key...\n";
		std::getchar();
	}
}

int main(int argc, char* argv[])
{
	if(argc > 2)
	{
		runTest(argv[1], std::atoi(argv[2]));
	}
	else if(argc > 1)
	{
		runTest(argv[1]);
	}
	else
	{
		runTest("localhost");
	}
	
	return 0;
}
