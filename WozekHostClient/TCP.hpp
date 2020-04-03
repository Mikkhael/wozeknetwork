#pragma once

#include <string_view>
#include <memory>
#include "Imported.hpp"

namespace tcp
{
using Error = boost::system::error_code;


class Connection
{
	asiotcp::resolver resolver;
	
	
	using Socket = asiotcp::socket;
	using Endpoint = asiotcp::endpoint;
	
	Socket socket;
	Endpoint remoteEndpoint;
	
	static constexpr size_t bufferSize = 256;
	std::array<char, bufferSize> buffer;
	
	
	data::IdType id = 0;
	
	
	template <typename T>
	void writeToBuffer(T& data, size_t offset = 0)
	{
		std::memcpy(buffer.data() + offset, &data, sizeof(data));
	}
	
	void sendFromBuffer(size_t length, Error& err)
	{
		asio::write(socket, asio::buffer(buffer, length), err);
	}
	void receiveToBuffer(size_t length, Error& err)
	{
		asio::read(socket, asio::buffer(buffer, length), err);
	}
	
public:
	Connection(asio::io_context& ioContext)
		: resolver(ioContext), socket(ioContext)
	{
	}
	
	
	bool resolveAndConnect(std::string_view host, std::string_view service) {Error ignored; return resolveAndConnect(host, service, ignored);}
	bool resolveAndConnect(std::string_view host, std::string_view service, Error& err)
	{
		auto results = resolver.resolve(host, service, err);
		if(err)
		{
			return false;
		}
		remoteEndpoint = asio::connect(socket, results, err);
		if(err)
		{
			return false;
		}
		
		return true;
	}
	
	Socket& getSocket() {return socket;}
	const Endpoint& getRemote() {return remoteEndpoint;}
	
	data::IdType getId() {return id;}
	
	/// Functionality ///
	
	bool registerAsHost(const data::Host::Header& header) {Error ignore; return registerAsHost(header, ignore);};
	bool registerAsHost(data::Host::Header header, Error& err);
	
	
};





}

