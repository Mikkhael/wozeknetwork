#pragma once

#include "Imported.hpp"

namespace udp
{


class WozekUDPSender : public BasicHandler<WozekUDPSender, ArrayBuffer< 1<<10 >, true>
{
protected:
	
	virtual void connectionErrorHandler_impl(const Error& err)
	{
		std::cout << "Error while connecting to remote endpoint: " << err << '\n';
	}
	virtual void resolutionErrorHandler_impl(const Error& err)
	{
		std::cout << "Error while resolving endpoint address: " << err << '\n';
	}
	
	virtual void handle_impl(){ assert(false);};
	
	constexpr static auto BufferSize = 1 << 10;
	
public:
	
	WozekUDPSender(asio::io_context& ioContext, Socket& socket)
		: BasicHandler(ioContext, socket)
	{
	}
	
	void sendEchoMessage(const std::string& message);
	void sendUdpStateUpdate(const data::RotationType rotation[3], const data::IdType& id);
	//void sendFetchStateRequest(const data::IdType id);
	
	void errorCritical(const Error& err)
	{
		std::cout << "Error: " << err << '\n';
		returnCallbackCriticalError();
	}
};

class WozekUDPReceiver : public BasicHandler<WozekUDPReceiver,ArrayBuffer< 512 >, false>
{
protected:
	
	constexpr static auto BufferSize = 512;	
	
	virtual void connectionErrorHandler_impl(const Error& err)
	{
		std::cout << "Error while connecting to remote endpoint: " << err << '\n';
	}
	virtual void resolutionErrorHandler_impl(const Error& err)
	{
		std::cout << "Error while resolving endpoint address: " << err << '\n';
	}
	
	virtual void handle_impl()
	{		
		std::cout << "UDP: Received " << bytesTransfered << " bytes from " << remoteEndpoint << '\n';
		
		handleDatagram();
	}
	
	void handleDatagram();
	
public:
	
	WozekUDPReceiver(asio::io_context& ioContext, Socket& socket)
		: BasicHandler(ioContext, socket)
	{
	}
	
	void handleEchoResponse();
};


class WozekUDPServer : public BasicServer<WozekUDPReceiver>
{
protected:
	virtual bool connectionErrorHandler_impl(const Error& err)
	{
		std::cout << "Error while receiving UDP request: " << err << '\n';
		return true;
	}
	virtual bool bindingErrorHandler_impl(const Error& err)
	{
		std::cout << "Error while binding UDP socket: " << err << '\n';
		return true;
	}
	
public:
	WozekUDPServer(asio::io_context& ioContext)
		: BasicServer(ioContext)
	{
	}
	
};



}

