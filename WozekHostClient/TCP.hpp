#pragma once

#include <string_view>
#include <memory>
#include "Imported.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <type_traits>

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
	
	
	
	static constexpr size_t bufferSize = 4096;
	std::array<char, bufferSize> buffer;
	
	
	
	data::IdType id = 0;
	
	asio::steady_timer heartbeatTimer;
	
	void defaultErrorHandler(const Error& err)
	{
		std::cout << " Error: " << err << '\n';
	}
	
public:
	Connection(asio::io_context& ioContext)
		: strand(ioContext), resolver(strand), socket(strand), heartbeatTimer(strand)
	{
	}
	
	/// Logging
	
	std::ostream& printPrefix(std::ostream& os) // TODO: add timestamp
	{
		os << "[ ";
		if(socket.is_open())
			os << socket.remote_endpoint() << ' ';
		return os << "] ";
		
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
		asio::async_read(socket, buffer, handler);
	}
	template <typename Handler>
	void asyncRead(const size_t length, Handler handler)
	{
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
	
	
	/// Basics ///
	
	void resolveAndConnect(std::string_view host, std::string_view service, std::function<void(bool)> requestCallback )
	{
		resolver.async_resolve(host, service, [=](const Error& err, auto results)
			{
				if(err)
				{
					logError("During Relove: ", err);
					requestCallback(false);
					return;
				}
				
				asio::async_connect(socket, results, [=](const Error& err, const auto& endpoint)
					{
						if(err)
						{
							logError("During Connection: ", err);
							requestCallback(false);
							return;
						}
						log("Connected to ", endpoint);
						requestCallback(true);
					});
			});
	}
	
	Socket& getSocket() {return socket;}
	const Endpoint& getRemote() {return remoteEndpoint;}
	
	data::IdType getId() {return id;}
	
	
	
	/// Functionality ///
	
	// Register As New Host
	
	void registerAsNewHost(data::RegisterNewHost::Request reqHeader, std::function<void(data::IdType)> requestCallback );
	
	
};





}

