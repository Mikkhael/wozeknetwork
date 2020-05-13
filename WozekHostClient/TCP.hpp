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
	
	asio::steady_timer heartbeatTimer;
	asio::steady_timer timeoutTimer;
	
	std::chrono::seconds timeoutDuration = std::chrono::seconds(35);
	std::chrono::seconds heartbeatInterval = std::chrono::seconds(15);
	
	void defaultErrorHandler(const Error& err)
	{
		std::cout << " Error: " << err << '\n';
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
		#ifndef SILENT
		(printPrefix(std::clog) << ... << args) << '\n'; 
		#endif // SILENT
	}
	template <typename ...Ts>
	void logError(Ts ...args)
	{
		#ifndef SILENT
		((printPrefix(std::cerr) << "Error ") << ... << args) << '\n'; 
		#endif // SILENT
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
		heartbeatTimer.expires_after(heartbeatInterval);
		heartbeatTimer.async_wait([this](const Error& err){heartbeatHandler(err);});
	}
	void cancelHeartbeat()
	{
		heartbeatTimer.cancel();
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

