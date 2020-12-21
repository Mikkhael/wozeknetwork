#pragma once

#include "Imported.hpp"


#include <string_view>
#include <memory>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <type_traits>


namespace fs = std::filesystem;

namespace tcp
{



class ControllerSessionClient
	: public BasicSession<ControllerSessionClient, ArrayBuffer< 1<<10 >, true>
{
	
public:
	ControllerSessionClient(asio::io_context& ioContext)
		: BasicSession(ioContext)
	{
		//assert(sharedFromThis().get() == this);
	}
	
	~ControllerSessionClient()
	{
		std::cout << "Client TCP Session Destroyed!\n";
	}
	
	void registerAsControllerRequest(std::string_view name);
	void startKeepAlive(const std::chrono::milliseconds interval);
	void stopKeepAlive();

protected:
	
	void receiveRegisterAsControllerResponse();
	void handleRegisterAsControllerResponse(const data::RegisterAsController::ResponseHeader& response);
	
	bool errorCritical(const Error& err) {
		if(err == asio::error::eof){
			std::cout << "TCP Connection unexpectedly closed\n";
		}else{
			std::cout << "TCP Critical Error: " << err << '\n';
		}
		returnCallbackCriticalError();
		return true;
	}
protected:
	virtual void timeoutHandler_impl()
	{
		std::cout << "TCP Socket timed out\n";
	}
	virtual bool start_impl()
	{
		std::cout << "TCP connection started (" << getRemote() << ")\n";
		return true;
	}
	virtual void shutdown_impl()
	{
		std::cout << "TCP Shutting down...\n";
	}
	virtual void startError_impl(const Error& err)
	{
		std::cout << "TCP Unknown error while connecting to the remote endpoint: " << err << "\n";
	}
};




}
