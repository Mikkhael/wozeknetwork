#pragma once

#include "Imported.hpp"

namespace udp
{


class ControllerUDPSender : public BasicHandler<ControllerUDPSender, ArrayBuffer< 1<<10 >, true>
{
protected:
	
	virtual void handle_impl(const size_t bytesTransfered){ assert(false);};
	
	
public:
	
	ControllerUDPSender(asio::io_context& ioContext)
		: BasicHandler(ioContext)
	{
	}
	
	
};




class ControllerUDPReceiver : public BasicHandler<ControllerUDPReceiver,ArrayBuffer< 512 >, false>
{
protected:
	
	virtual void handle_impl(const size_t bytesTransfered)
	{
		std::cout << "Received " << bytesTransfered << " bytes from " << remoteEndpoint;
	}
	
public:
	
	ControllerUDPReceiver(asio::io_context& ioContext)
		: BasicHandler(ioContext)
	{
	}
	

	
	
	
};


class ControllerUDPServer : public BasicServer<ControllerUDPReceiver>
{
protected:
	virtual bool connectionErrorHandler_impl(const Error& err)
	{
		std::cout << "Error while receiving UDP request: " << err << '\n';
		return true;
	}
	
public:
	ControllerUDPServer(asio::io_context& ioContext)
		: BasicServer(ioContext)
	{
	}
	
};



}

