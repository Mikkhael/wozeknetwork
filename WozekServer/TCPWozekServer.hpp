#pragma once

#include "TCP.hpp"
#include "DatabaseManager.hpp"
#include "fileManager.hpp"

#include "segmentedFileTransfer.hpp"

#include <type_traits>
#include <chrono>
#include <array>

#include "states.hpp"


namespace tcp
{

constexpr bool ShutdownOnDestruction = true;

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
	
	bool isShutDown = false;
	
	asio::steady_timer timeoutTimer;
	std::chrono::seconds timeoutDuration = std::chrono::seconds(35);
	
	asio::steady_timer debugTimer;
	void debugHandler() { 
		std::cout << shared_from_this().use_count() << std::endl;
		debugTimer.expires_after(std::chrono::seconds(1));
		debugTimer.async_wait([=](const Error& err){debugHandler();});
	}
	
	static constexpr size_t bufferSize = 4096;
	std::array<char, bufferSize> buffer = {0};
	
	/// State
	
	States::Type globalState;
	
	template <typename T>
	T* setState()
	{
		return &globalState.emplace<T>();
	}
	
	void resetState()
	{
		globalState.emplace<States::Empty>();
	}
	
public:
	WozekConnectionHandler(asio::io_context& ioContext)
		: socket(ioContext), timeoutTimer(ioContext), debugTimer(ioContext)
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
	
	Type type = Type::None;
	db::IdType id = 0;
	
	void operator()();
private:
	
	
	/// Logging
	
	std::ostream& printPrefix(std::ostream& os) // TODO: add timestamp
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
		((printPrefix(std::cerr) << "Error ") << ... << args) << '\n'; 
	}
	
	/// Functionality ///
	
	void awaitRequest(bool silent = false);
	void handleReceivedRequestId(char id);
	
	void sendTerminatingMessage(const char* bytes, size_t length);
	void sendTerminatingMessageFromMainBuffer(size_t length) {sendTerminatingMessage(buffer.data(), length);}
	template<typename T>
	void sendTerminatingMessageObject(T&& object)
	{
		saveObjectToMainBuffer(object);
		sendTerminatingMessageFromMainBuffer(sizeof(object));
	}	
	
	// SegFmented ile Transfer
	
	void initiateFileTransferReceive(const size_t totalSize, fs::path path, std::function<void(bool)> callback, bool silent = false);
	void initiateFileTransferSend(fs::path path, std::function<void(bool)> callback, bool silent = false);
	
	// Register host
	
	void handleRegisterHostRequest();
	void tryRegisteringNewHost(db::Host::Header& header);
	void registerNewHostRequestSuccess();
	void registerNewHostRequestFailed();
	
	// Upload Map
	
	void handleUploadMapRequest();
	void finalizeUploadMap(data::UploadMap::RequestHeader header);
	
	// Download Map
	
	void handleDownloadMapRequest();
	void finalizeDownloadMap(data::DownloadMap::RequestHeader header);
	
	/// Async and Helper Functions ///
	
	template<typename Handler, typename Executor>
	auto asyncPost(const Executor& executor, Handler handler)
	{
		return asio::post(executor, [me = shared_from_this(), handler]
		{
			if constexpr (std::is_member_function_pointer<Handler>::value)
				((*me).*handler)();
			else
				handler();
		});
	}
	template<typename Handler>
	auto asyncPost(Handler handler) { return asyncPost(getExecutor(), handler); }
	
	template<typename Handler> 
	void asyncReadToMainBuffer(const size_t length, Handler handler)
	{
		asyncReadToMemory(buffer.data(), length, handler);
	}
	
	template<typename Handler> 
	void asyncReadToMemory(char* buffer, const size_t length, Handler handler)
	{
		startTimeoutTimer();
		asio::async_read(socket, asio::buffer(buffer, length), [me = shared_from_this(), handler](const Error& err, const size_t bytesLength)
		{
			me->cancelTimeoutTimer();
			handler(err, bytesLength);
		});
	}
	
	template<typename T, typename Handler> 
	void asyncReadObject(Handler handler)
	{
		static_assert ( sizeof(T) <= bufferSize );
		
		startTimeoutTimer();
		asio::async_read(socket, asio::buffer(buffer, sizeof(T)), [me = shared_from_this(), handler](const Error& err, const size_t bytesLength)
		{
			me->cancelTimeoutTimer();
			T obj;
			me->loadObjectFromMainBuffer(obj);
			handler(err, obj);
		});
	}
	
	template<typename T>
	void loadObjectFromMainBuffer(T& object)
	{
		static_assert( sizeof(T) <= bufferSize );
		std::memcpy(&object, buffer.data(), sizeof(T));
	}
	template<typename T>
	void loadObjectFromMemory(T& object, const char* buffer)
	{
		std::memcpy(&object, buffer, sizeof(T));
	}
	
	template<typename Handler> 
	void asyncWriteFromMemory(const char* buffer, size_t length, Handler handler)
	{
		asio::async_write(socket, asio::buffer(buffer, length), handler);
	}
	template<typename Buffer, typename Handler> 
	void asyncWriteFromBuffer(Buffer&& buffer, Handler handler)
	{
		asio::async_write(socket, std::forward<Buffer>(buffer), handler);
	}
	template<typename Handler> 
	void asyncWriteFromMainBuffer(size_t length, Handler handler)
	{
		asio::async_write(socket, asio::buffer(buffer, length), handler);
	}
	template<typename Handler, typename T> 
	void asyncWriteObject(T&& object, Handler handler)
	{
		saveObjectToMainBuffer(object);
		asyncWriteFromMainBuffer(sizeof(T), handler);
	}
	
	template<typename T>
	void saveObjectToMainBuffer(T&& object)
	{
		static_assert( sizeof(T) <= bufferSize );
		std::memcpy(buffer.data(), &object, sizeof(T));
	}
	template<typename T>
	void saveObjectToMemory(T&& object, char* buffer)
	{
		std::memcpy(buffer, &object, sizeof(T));
	}
	
	template<typename SuccessHandler>
	auto asyncBranch(SuccessHandler successHandler) { return asyncBranch(successHandler, &WozekConnectionHandler::logAndAbortErrorHandler); }
	template<typename SuccessHandler, typename ErrorHandler>
	auto asyncBranch(SuccessHandler successHandler, ErrorHandler errorHandler);
	
	/// Timer ///
	
	void handleTimeout(const Error& error)
	{
		if(error || isShutDown)
			return;
		
		timeoutHandler();
		shutConnection();
	}
	
	void startTimeoutTimer() {
		timeoutTimer.expires_after(timeoutDuration);
		activateTimeoutTimer();
	}
	
	void cancelTimeoutTimer() {
		timeoutTimer.cancel();
	}
	
	void activateTimeoutTimer() {
		timeoutTimer.async_wait([me = shared_from_this()](const Error& err){me->handleTimeout(err);});
	}
	
	void refreshTimeoutTimer() {
		if (!isShutDown && timeoutTimer.expires_after(timeoutDuration) > 0){
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
	
	bool logAndAbortErrorHandlerOrDisconnect(const Error& err) {
		if(err == asio::error::eof)
		{
			log("Disconnected");
			return true;
		}
		logError(err);
		return true;
	}
	
	bool logAndAbortErrorHandler(const Error& err) {
		if(err == asio::error::eof)
		{
			logError("Connection unexpectedly closed");
			return true;
		}
		logError(err);
		return true;
	}

	void timeoutHandler();
	void shutdownHandler();
};

struct WozekConnectionErrorHandler
{
	bool operator()(const Error& err)
	{
		std::cout << "Error occured while connecting:\n" << err << '\n';
		return false;
	}
};


template<typename SuccessHandler, typename ErrorHandler>
auto WozekConnectionHandler::asyncBranch(SuccessHandler successHandler, ErrorHandler errorHandler)
{
	return [me = shared_from_this(), successHandler, errorHandler](const Error& err, auto&&... args)
	{
		if(me->isShutDown)
		{
			return;
		}
		
		if(err)
		{
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
	};
}


using WozekServer = GenericServer<WozekConnectionHandler, WozekConnectionErrorHandler>;

}

