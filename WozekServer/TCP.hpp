#ifndef TCP_HPP_INCLUDED
#define TCP_HPP_INCLUDED

#include "asioWrapper.hpp"

#include <functional>
#include <memory>
#include <iostream>

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
using Error = boost::system::error_code;
inline std::ostream& operator<<(std::ostream& os, const Error& error)
{
	return os << '[' << error.value() << "] " << error.message();
}

struct ConnectionErrorHandlerIgnore
{
	operator()(const Error& err)
	{
		// Ignore errors
		return false;
	}
};

template <typename ConnectionHandler, typename ConnectionErrorHandler = ConnectionErrorHandlerIgnore>
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
		else
		{
			//std::cerr << "Started handler " << handler->getRemote() << '\n';
			(*handler)();
			//std::cerr << "Returned handler " << handler->getRemote() << '\n';
		}
		
		
		awaitNewConnection();
	}
	
};

}
#endif // TCP_HPP_INCLUDED
