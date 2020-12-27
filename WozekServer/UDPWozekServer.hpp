#pragma once

#include "asio_lib.hpp"
#include "logging.hpp"
#include "Datagrams.hpp"

namespace udp
{

/*
class WozekUDPSender : public BasicHandler<WozekUDPSender, ArrayBuffer< 1<<10 >, true>
{
protected:
	
	virtual void handle_impl(const size_t bytesTransfered){ assert(false);};
	
	
public:
	
	WozekUDPSender(asio::io_context& ioContext)
		: BasicHandler(ioContext)
	{
	}
	
	
};
*/



class WozekUDPReceiver : public BasicHandler<WozekUDPReceiver,ArrayBuffer< 512 >, false>
{
	/// Logging
	
	std::string getPrefix() // TODO: add timestamp
	{
		std::stringstream ss;
		ss << "[UDP " << remoteEndpoint << "] ";
		return ss.str();
	}
	
	template <typename ...Ts>
	void log(Ts&& ...args)
	{
		logger.output(getPrefix(), std::forward<Ts>(args)...);
	}
	template <typename ...Ts>
	void logError(Logger::Error name, Ts&& ...args)
	{
		if constexpr (sizeof...(args) > 0)
		{
			logger.output(getPrefix(), "Error {code: ", static_cast<int>(name), "} ", std::forward<Ts>(args)...);
		}
		logger.error(name);
	}
	template <typename ...Ts>
	void logError(Ts&& ...args)
	{
		logger.output(getPrefix(), "Error ", std::forward<Ts>(args)...);
		logger.error(Logger::Error::UnknownError);
	}
	
protected:
	
	constexpr static int ControllerReceiverPort = 8088;
	constexpr static int HostReceiverPort = 8089;
	Endpoint receiverEndpoint; 
	
	
	virtual void connectionErrorHandler_impl(const Error& err)
	{
		logError(Logger::Error::UdpConnectionError, "Error while connecting to remote endpoint: ", err);
	}
	virtual void resolutionErrorHandler_impl(const Error& err)
	{
		logError(Logger::Error::UdpResolutionError, "Error while resolving endpoint address: ", err);
	}
	
	virtual void handle_impl()
	{
		log("Received ", bytesTransfered, " bytes from endpoint: ", remoteEndpoint);
		
		handleRequest();
	}
	
public:
	
	WozekUDPReceiver(asio::io_context& ioContext, Socket& socket)
		: BasicHandler(ioContext, socket)
	{
	}
	
	void handleRequest();
	
	void handleFetchStateRequest();
	void handleEchoRequest();
	void handleUpdateStateRequest();
	
	
	bool errorAbort(const Error& err) {
		std::cout << "Error during handling UDP request." << '\n'; 
		logError(Logger::Error::UdpTransmissionError, err);
		return true;
	}
	
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


