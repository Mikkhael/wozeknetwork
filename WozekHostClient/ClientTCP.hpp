#pragma once

#include <string_view>
#include <memory>
#include "Imported.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <type_traits>
#include "GameManager.hpp"


namespace fs = std::filesystem;

namespace tcp
{



class WozekSessionClient
	: public BasicSession<WozekSessionClient, ArrayBuffer< 1<<13 >, true>, public Statefull
{
	
public:
	WozekSessionClient(asio::io_context& ioContext)
		: BasicSession(ioContext)
	{
		//assert(sharedFromThis().get() == this);
	}
	
	~WozekSessionClient()
	{
		std::cout << "Client TCP Session Destroyed!\n";
	}
	
	
	void performEchoRequest(const std::string& message);
	void sendHeartbeat();

protected:
	
	
	constexpr static size_t DefaultBigBufferSize = 1024 * 1024 * 16;
	constexpr static size_t DefaultSegmentsInBuffer = 16;
	constexpr static size_t DefaultSegmentSize = DefaultBigBufferSize / DefaultSegmentsInBuffer;
	void startSegmentedFileSend(const fs::path sourcePath);
	void sendSegmentHeader();
	void handleSegmentFileHeaderResponseCode(const data::SegmentedFileTransfer::Error error);
	void sendSegmentFileData();
	void handleSegmentFileDataResponseCode(const data::SegmentedFileTransfer::Error error);
	void finalizeSegmentFileSend();
	
	
	void receiveEchoResponsePart();
	void handleEchoResponsePart(const char c);
	void finilizeEcho(std::string& receiveedMessage);
	
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


/*

namespace tcp
{
using Error = boost::system::error_code;


class Connection
{
	asio::io_context::strand strand;
	
	asiotcp::resolver resolver;
	
	
	using Socket = asiotcp::socket;
	using Endpoint = asiotcp::endpoint;
	
	Socket socket;
	Endpoint remoteEndpoint;
	
	bool isConnected = false;
	
	static constexpr size_t bufferSize = 4096;
	std::array<char, bufferSize> buffer;
	
	bool isHeartbeatEnabled = false;
	
	asio::steady_timer heartbeatTimer;
	asio::steady_timer timeoutTimer;
	
	std::chrono::seconds timeoutDuration = std::chrono::seconds(35);
	std::chrono::seconds heartbeatInterval = std::chrono::seconds(15);
	
	void defaultErrorHandler(const Error& err)
	{
		logError(err);
	}
	
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
	Connection(asio::io_context& ioContext)
		: strand(ioContext), resolver(ioContext), socket(ioContext), heartbeatTimer(ioContext), timeoutTimer(ioContext)
	{
	}
	
	/// Logging
	
	std::ostream& printPrefix(std::ostream& os) // TODO: add timestamp
	{
		return os <<  "[ " << remoteEndpoint << "] ";
		
	}
	template <typename ...Ts>
	void log(Ts ...args)
	{
		#ifndef DLL_SILENT
		(printPrefix(std::clog) << ... << args) << '\n'; 
		#endif // DLL_SILENT
	}
	template <typename ...Ts>
	void logError(Ts ...args)
	{
		#ifndef DLL_SILENT
		((printPrefix(std::cerr) << "Error ") << ... << args) << '\n'; 
		#endif // DLL_SILENT
	}
	
	/// Async ///
	
	template <typename Handler, typename ... Args>
	void execute(Handler handler, Args&& ... args)
	{
		if constexpr (std::is_member_function_pointer_v<Handler>)
		{
			this->*handler( std::forward<Args>(args)... );
		}
		else
		{
			handler( std::forward<Args>(args)... );
		}
	}
	template <typename Handler>
	auto getNormalizedHandler(Handler handler)
	{
		if constexpr (std::is_member_function_pointer_v<Handler>)
			return [this, handler](auto&& ... args){this->*handler(std::forward<decltype(args)>(args)...); };
		else
			return handler;
	}
	
	
	template <typename Handler>
	auto post(Handler handler)
	{
		return asio::post(strand, getNormalizedHandler(handler));
	}
	
	auto& getStrand() {return strand;}
	
	template <typename Handler, typename ErrorHandler>
	auto ioHandler(Handler handler, ErrorHandler errorHandler)
	{
		return [handler, errorHandler, this](const Error& err, auto&& ... args) {
			if(!isConnected){
				execute(errorHandler, asio::error::operation_aborted);
				return;
			}
			cancelTimeoutTimer();
			if(err) 
				{ execute(errorHandler, err); return; } 
			if constexpr (std::is_invocable_v<Handler>)
				execute(handler); 
			else 
				execute(handler, std::forward<decltype(args)>(args)... ); };
	}
	
	template <typename Buffer, typename Handler>
	void asyncRead(Buffer buffer, Handler handler)
	{
		startTimeoutTimer();
		asio::async_read(socket, buffer, handler);
	}
	template <typename Handler>
	void asyncRead(const size_t length, Handler handler)
	{
		startTimeoutTimer();
		asio::async_read(socket, asio::buffer(buffer, length), handler);
	}
	
	template <typename Buffer, typename Handler>
	void asyncWrite(Buffer buffer, Handler handler)
	{
		asio::async_write(socket, buffer, handler);
	}
	template <typename Handler>
	void asyncWrite(const size_t length, Handler handler)
	{
		asio::async_write(socket, asio::buffer(buffer, length), handler);
	}
	
	template <typename T>
	size_t writeObjectToBuffer(T& object, size_t off = 0)
	{
		std::memcpy(buffer.data() + off, &object, sizeof(T));
		return sizeof(T);
	}
	template <typename T>
	size_t readObjectFromBuffer(T& object, size_t off = 0)
	{
		std::memcpy(&object, buffer.data() + off, sizeof(T));
		return sizeof(T);
	}
	
	/// Timer ///
	
	void startTimeoutTimer()
	{
		timeoutTimer.expires_after(timeoutDuration);
		timeoutTimer.async_wait([this](const Error& err){timeoutHandler(err);});
	}
	void cancelTimeoutTimer()
	{
		timeoutTimer.cancel();
	}
	void timeoutHandler(const Error& err)
	{
		if(err || !isConnected)
			return;
		disconnect();
		cancelHeartbeat();
	}
	
	void startHeartbeat()
	{
		if(!isHeartbeatEnabled)
			return;
		
		heartbeatTimer.expires_after(heartbeatInterval);
		heartbeatTimer.async_wait([this](const Error& err){heartbeatHandler(err);});
	}
	void cancelHeartbeat()
	{
		heartbeatTimer.cancel();
	}
	void disableHeartbeat()
	{
		isHeartbeatEnabled = false;
		cancelHeartbeat();
	}
	void enableHeartbeat()
	{
		isHeartbeatEnabled = true;
	}
	void heartbeatHandler(const Error& err)
	{
		if(err || !isConnected)
			return;
		Error heartbeatError;
		asio::write(socket, asio::buffer(&data::HeartbeatCode, sizeof(data::HeartbeatCode)), heartbeatError);
		if(heartbeatError)
		{
			disconnect();
			return;
		}
		startHeartbeat();
	}
	
	void reset()
	{
		startHeartbeat();
		resetState();
	}
	
	
	/// Basics ///
	
	bool getIsConnected()
	{
		return isConnected;
	}
	
	void disconnect()
	{
		isConnected = false;
		timeoutTimer.cancel();
		heartbeatTimer.cancel();
		Error ignored;
		socket.shutdown(asiotcp::socket::shutdown_both, ignored);
		socket.close();
		gameManager.setHostId(0);
		gameManager.setWorldId(0);
		resetState();
	}
	
	
	Socket& getSocket() {return socket;}
	const Endpoint& getRemote() {return remoteEndpoint;}
	
	// Callbacks
	
	enum class CallbackCode {Success=0, Error=1, CriticalError=2};
	using Callback = std::function<void(CallbackCode)>;
	
	// Connect
	
	void resolveAndConnect(std::string_view host, std::string_view service, Callback requestCallback)
	{
		if(isConnected)
			disconnect();
		
		log("Starting resolve...");
		
		resolver.async_resolve(host, service, [=](const Error& err, auto results)
			{
				log("Starting connect...");
				if(err)
				{
					logError("During Relove: ", err);
					requestCallback(CallbackCode::Error);
					return;
				}
				
				asio::async_connect(socket, results, [=](const Error& err, const auto& endpoint)
					{
						if(err)
						{
							logError("During Connection: ", err);
							requestCallback(CallbackCode::Error);
							return;
						}
						remoteEndpoint = endpoint;
						log("Connected to ", endpoint);
						isConnected = true;
						requestCallback(CallbackCode::Success);
					});
			});
	}	
	
	/// Functionality ///
	
	
	// Register As New Host
	
	void registerAsNewHost(data::RegisterNewHost::Request reqHeader, Callback requestCallback );
	
	// Upload Map
	
	void uploadMap( fs::path path, Callback requestCallback );
	void downloadMap( data::IdType id, fs::path path, Callback requestCallback );
	
	// Any Files
	
	void uploadFile( fs::path path, std::string name, Callback requestCallback );
	void downloadFile( fs::path path, std::string name, Callback requestCallback );
	
	// World
	
	void startTheWorld( Callback requestCallback );
	
	// Segmented File Transfer
	
	void initiateFileUpload(fs::path path, const size_t maxBigBufferSize, const size_t maxSegmentLength, Callback requestCallback);
	void initiateFileDownload(fs::path path, const size_t id, const size_t maxBigBufferSize, Callback requestCallback);
	
	
};





}

*/
