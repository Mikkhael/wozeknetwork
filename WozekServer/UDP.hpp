#pragma once

#include "asioWrapper.hpp"

#include <functional>
#include <memory>
#include <iostream>
#include "logging.hpp"
#include "config.hpp"

namespace udp
{

constexpr size_t maxDatagramDataSize = 65507;
using bufferType = std::array<char, maxDatagramDataSize>;
	
template <typename DatagramReceiveHandler, typename DatagramReceiveErrorHandler, typename ShutdownHandler>
class GenericServer
{
	
	DatagramReceiveHandler datagramReceiveHandler;
	DatagramReceiveErrorHandler datagramReceiveErrorHandler;
	ShutdownHandler shutdownHandler;
	
	asio::io_context& ioContext;
	
	asioudp::socket socket;
	asioudp::endpoint senderEndpoint;
	
	bufferType buffer;
	
	bool running = false;
	
public:
	
	GenericServer(asio::io_context& ioContext_)
		: ioContext(ioContext_), socket(ioContext_)
	{
	}
	
	~GenericServer()
	{
		shutdownHandler();
	}
	
	bool start(uint16_t port)
	{
		if(running)
		{
			return false;
		}
		
		socket.open(asioudp::v4());
		socket.bind(asioudp::endpoint( asioudp::v4(), port ));
		
		running = true;
		
		awaitDatagram();
		return true;
	}
	
private:
	
	void awaitDatagram()
	{
		if(!running)
			return;
		
		//std::cout << "ASDASDA\n";
		
		socket.async_receive_from(asio::buffer(buffer), senderEndpoint, [=](const Error& err, size_t length){
			//std::cout << "2323232323\n";
			if(err)
			{
				//std::cout << "ERRERRERR\n";
				if(datagramReceiveErrorHandler(senderEndpoint, err))
				{
					shutdownHandler();
					return;
				}
				awaitDatagram();
			}
			else
			{
				//std::cout << "GDGDGDGD\n";
				datagramReceiveHandler(senderEndpoint, buffer, length);
				awaitDatagram();
			}
		});
	}
};

}

