#pragma once

#include "TCP.hpp"
#include <type_traits>
#include <chrono>

namespace tcp
{
	
constexpr bool ShutdownOnDestruction = false;

class WozekConnectionHandler
	: public std::enable_shared_from_this<WozekConnectionHandler>
{
	using Socket = asiotcp::socket;
	using Endpoint = asiotcp::endpoint;
	Socket socket;
	Endpoint remoteEndpoint;
	
	bool isShutDown = false;
	
	asio::steady_timer timeoutTimer;
	std::chrono::seconds baseTimeoutDuration = std::chrono::seconds(15);
	
	std::string messageBuffer = std::string(1024, ' ');
		
public:
	WozekConnectionHandler(asio::io_context& ioContext)
		: socket(ioContext), timeoutTimer(ioContext)
	{
	}
	
	~WozekConnectionHandler()
	{
		if constexpr(ShutdownOnDestruction)
		{
			if(!isShutDown)
			{
				shutdownHandler();
			}
		}
	}
	
	Socket& getSocket() {return socket;}
	const Endpoint& getRemote() {return remoteEndpoint;}
	
	void operator()()
	{
		//std::cout << "New connection from " << socket.remote_endpoint() << '\n';
		remoteEndpoint = socket.remote_endpoint();
		
		startTimeoutTimer();
		receiveSomeMessage();
	}
	
private:
	
	/// Functionality ///
	

	void receiveSomeMessage(size_t length = 0);
	void sendMessage(size_t length = 0);
	
	/// Async ///

	template<typename SuccessHandler>
	auto asyncBranch(SuccessHandler successHandler) {
		return asyncBranch(successHandler, abortErrorHandler);
	}
	
	template<typename SuccessHandler, typename ErrorHandler>
	auto asyncBranch(SuccessHandler successHandler, ErrorHandler errorHandler)
	{
		//std::cerr << "Posted Branch\n";
		return [me = shared_from_this(), successHandler, errorHandler](const Error& err, auto&&... args)
		{
			if(me->isShutDown)
			{
				return;
			}
			
			//std::cerr << "Branch Start\n";
			if(err)
			{
				//std::cerr << "Branch Error\n";
				bool abort = false;
				if constexpr (std::is_member_function_pointer<ErrorHandler>::value)
				{
					abort = ((*me).*errorHandler)(err);
				}
				else
				{
					abort = errorHandler(err);
				}
				
				if(abort)
				{
					me->shutConnection();
					return;
				}
			}
			//std::cerr << "Branch Passed\n";
			if constexpr (std::is_member_function_pointer<SuccessHandler>::value)
			{
				((*me).*successHandler)( std::forward<decltype(args)>(args)... );
			}
			else
			{
				successHandler( std::forward<decltype(args)>(args)... );
			}
			//std::cerr << "Branch Executed\n";
		};
	}
	
	/// Timer ///
	
	void handleTimeout(const Error& error)
	{
		if(error || isShutDown)
			return;
		
		timeoutHandler();
		shutConnection();
	}
	
	void startTimeoutTimer()
	{
		startTimeoutTimer(baseTimeoutDuration);
	}
	void startTimeoutTimer(const std::chrono::seconds& newDuration)
	{
		timeoutTimer.expires_after(std::chrono::seconds(5));
		activateTimeoutTimer();
	}
	
	void activateTimeoutTimer()
	{
		timeoutTimer.async_wait([me = shared_from_this()](const Error& err){me->handleTimeout(err);});
	}
	
	void refreshTimeout()
	{
		refreshTimeout(baseTimeoutDuration);
	}
	void refreshTimeout(const std::chrono::seconds& newDuration)
	{
		if (!isShutDown && timeoutTimer.expires_after(std::chrono::seconds(5)) > 0)
		{
			activateTimeoutTimer();
		}
	}

	/// Shutdown ///
	
	void shutConnection()
	{
		isShutDown = true;
		
		timeoutTimer.cancel();
		
		socket.shutdown(Socket::shutdown_type::shutdown_both);
		Error ignoredError;
		socket.close(ignoredError);
		
		if constexpr (!ShutdownOnDestruction)
		{
			shutdownHandler();
		}
	}
	
	/// Handlers ///
	
	bool abortErrorHandler(const Error& err) {
		return true;
	}
	
	bool logAndAbortErrorHandler(const Error& err) {
		std::cout << "Error in socket connected to " << getRemote() << ":\n " << err << '\n';
		return true;
	}

	void timeoutHandler();
	void shutdownHandler();
	
};

struct WozekConnectionErrorHandler
{
	operator()(const Error& err)
	{
		std::cout << "Error occured while connecting:\n" << err << '\n';
		return false;
	}
};

using WozekServer = GenericServer<WozekConnectionHandler, WozekConnectionErrorHandler>;



}

