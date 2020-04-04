#pragma once

#include "TCP.hpp"
#include "DatabaseManager.hpp"
#include <type_traits>
#include <chrono>
#include <array>


namespace tcp
{
	
constexpr bool ShutdownOnDestruction = false;

class WozekConnectionHandler
	: public std::enable_shared_from_this<WozekConnectionHandler>
{
public:
	using Socket = asiotcp::socket;
	using Endpoint = asiotcp::endpoint;
	enum class Type {None, Host, Controller};
	
private:
	Socket socket;
	Endpoint remoteEndpoint;
	
	Type type = Type::None;
	db::IdType id = 0;
	
	bool isShutDown = false;
	
	
	asio::steady_timer timeoutTimer;
	std::chrono::seconds baseTimeoutDuration = std::chrono::seconds(15);
	
	static constexpr size_t bufferSize = 256;
	std::array<char, bufferSize> buffer;
		
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
	
	/// Getters and Setters
	
	Socket& getSocket() {return socket;}
	auto getExecutor() {return socket.get_executor();}
	const Endpoint& getRemote() {return remoteEndpoint;}
	bool isRunning() {return !isShutDown;}
	
	/// Type and Database
	
	Type getType() {return type;}
	db::IdType getId() {return id;}
	
	
	
	void operator()();
	
private:
	// Logging
	
	std::ostream& printPrefix(std::ostream& os)
	{
		os << "[ ";
		if(socket.is_open())
			os << socket.remote_endpoint() << ' ';
		if(type != Type::None)
			os << (type == Type::Host ? 'H' : 'C') << id << ' ';
		return os << "] ";
		
	}
	template <typename ...Ts>
	void log(Ts ...args)
	{
		(printPrefix(std::clog) << ... << args) << '\n'; 
	}
	template <typename ...Ts>
	void logError(Ts ...args)
	{
		(printPrefix(std::cerr) << "Error " << ... << args) << '\n'; 
	}
	
	/// Functionality ///
	
	void awaitRequest();
	void handleReceivedRequestId(char id);
	
	void sendTerminatingBytes(const char* bytes, size_t length);
	void sendTerminatingBytes(size_t length) {sendTerminatingBytes(buffer.data(), length);}
	
	// Register host
	
	void receiveRegisterHostRequestData();
	void tryRegisteringNewHost(db::Host::Header& header);
	
	/// Async ///

	template<typename Handler>
	auto asyncDatabaseStrand(Handler handler)
	{
		return db::databaseManager.post([me = shared_from_this(), handler]
		{
			handler();
		});
	}
	
	template<typename Handler>
	auto asyncPost(Handler handler)
	{
		return asio::post(getExecutor(),[me = shared_from_this(), handler]
		{
			if constexpr (std::is_member_function_pointer<Handler>::value)
			{
				((*me).*handler)();
			}
			else
			{
				handler();
			}
		});
	}
	
	template<typename SuccessHandler>
	auto asyncBranch(SuccessHandler successHandler) {
		return asyncBranch(successHandler, logAndAbortErrorHandler);
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
				if constexpr(std::is_invocable<SuccessHandler, WozekConnectionHandler >::value)
				{
					((*me).*successHandler)();
				}
				else
				{
					((*me).*successHandler)( std::forward<decltype(args)>(args)... );
				}
			}
			else
			{
				if constexpr(std::is_invocable<SuccessHandler>::value)
				{
					successHandler();
				}
				else
				{
					successHandler( std::forward<decltype(args)>(args)... );
				}
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
		
		if constexpr (!ShutdownOnDestruction)
		{
			shutdownHandler();
		}
		
		Error ignoredError;
		socket.close(ignoredError);
		
	}
	
	/// Handlers ///
	
	bool abortErrorHandler(const Error& err) {
		return true;
	}
	
	bool logAndAbortErrorHandler(const Error& err) {
		std::cout << "Error in socket connected to " << getRemote() << ":\n " << err << '\n';
		logError("during async operation:\n", err);
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

