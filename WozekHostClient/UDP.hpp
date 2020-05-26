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



namespace udp
{
using Error = boost::system::error_code;


class Connection
{
	asio::io_context::strand strand;
	
	asioudp::resolver resolver;
	
	
	using Socket = asioudp::socket;
	using Endpoint = asioudp::endpoint;
	
	Socket socket;
	Endpoint remoteEndpoint;
	
	static constexpr size_t maxDatagramDataSize = 65507;
	static constexpr size_t bufferSize = 65550;
	std::array<char, bufferSize> buffer;
	
	template<typename TFirst, typename ...TRest>
	size_t writeObjectsToBufferFromOffset(size_t off, TFirst&& first, TRest&& ...rest)
	{
		std::memcpy(buffer.data() + off, &first, sizeof(first));
		if constexpr( sizeof...(rest) > 0)
		{
			return writeObjectsToBufferFromOffset(off + sizeof(first), std::forward<TRest>(rest)...);
		}
		else
		{
			return off + sizeof(first);
		}
	}
	
	/// Communication
	
	void send(size_t length, Error& err)
	{
		assert( remoteEndpoint != Endpoint{} );
		
		socket.send_to(asio::buffer(buffer, length), remoteEndpoint, 0, err);
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
		: strand(ioContext), resolver(ioContext), socket(ioContext)
	{
		socket.open(asioudp::v4());
	}
	
	bool setRemoteEndpoint(const std::string& ip, const std::string& port)
	{
		Error err;
		auto results = resolver.resolve(asioudp::v4(), ip, port, err);
		
		if(err)
		{
			logError("Cannot resolve UDP endpoint: ", err);
			return false;
		}
			
		
		remoteEndpoint = *results.begin();
		return true;
	}
	
	/// Logging
	
	std::ostream& printPrefix(std::ostream& os) // TODO: add timestamp
	{
		
		os << "[ ";
		if(socket.is_open())
			os << remoteEndpoint << ' ';
		return os << "] ";
		
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
	
	
	/// Basics ///
	
	
	Socket& getSocket() {return socket;}
	const Endpoint& getRemote() {return remoteEndpoint;}
	
	// Callbacks
	
	enum class CallbackCode {Success=0, Error=1};
	using Callback = std::function<void(CallbackCode)>;
	
	/// Functionality ///
	
	// Update State
	
	void updateState(Callback requestCallback);
	
};





}

