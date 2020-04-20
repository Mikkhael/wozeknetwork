#ifndef TCP_HPP_INCLUDED
#define TCP_HPP_INCLUDED

#include "asioWrapper.hpp"

#include <functional>
#include <memory>
#include <iostream>
#include "logging.hpp"
#include "config.hpp"

namespace tcp
{
/*
class Error : public boost::system::error_code
{
public:
	Error(const boost::system::error_code& err)
		: boost::system::error_code(err)
	{}
	Error()
		: boost::system::error_code()
	{}
};
*/


template <typename ConnectionHandler, typename ConnectionErrorHandler>
class GenericServer
{
	asio::io_context& ioContext;
	asiotcp::acceptor acceptor;
	bool running = false;
	
	using HandlerPointer = std::shared_ptr<ConnectionHandler>;
	
public:
	
	GenericServer(asio::io_context& ioContext_)
		: ioContext(ioContext_), acceptor(ioContext)
	{
	}
	
	bool start(uint16_t port)
	{
		if(running)
		{
			return false;
		}
		
		acceptor = asiotcp::acceptor(ioContext, asiotcp::endpoint(asiotcp::v4(), port));
		
		awaitNewConnection();
		return true;
	}
	
private:
	
	void awaitNewConnection()
	{
		//std::cerr << "Awaiting connection..";
		auto handler = std::make_shared<ConnectionHandler>(ioContext);
		
		acceptor.async_accept( handler->getSocket(), [=](const auto& err){
			handleNewConnection(handler, err);
		});
		//std::cerr << "..\n";
	}
	
	void handleNewConnection(HandlerPointer handler, const Error& err)
	{
		if(err)
		{
			if(ConnectionErrorHandler{}(err))
			{
				// If handler returned true, don't await any more connections
				return;
			}
		}
		
		const auto address = handler->getSocket().remote_endpoint().address();
		if(!address.is_v4())
		{
			logger.output("Connected from forbidden (v6) address: ", handler->getSocket().remote_endpoint(), '\n');
			logger.error(Logger::Error::TcpForbidden);
			awaitNewConnection();
		}
		else
		{			
			const uint32_t addressBinary = address.to_v4().to_uint();
			config.checkIfIpv4IsAllowedSafe(addressBinary, [=](const bool success){
				if(!success)
				{
					logger.output("Connected from forbidden address: ", handler->getSocket().remote_endpoint(), '\n');
					logger.error(Logger::Error::TcpForbidden);
				}
				else
				{
					(*handler)();
				}
				awaitNewConnection();
			});
		}
		
		
	}
	
};

}
#endif // TCP_HPP_INCLUDED
