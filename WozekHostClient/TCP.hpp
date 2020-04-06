#pragma once

#include <string_view>
#include <memory>
#include "Imported.hpp"
#include <atomic>
#include <filesystem>
#include <fstream>
#include <algorithm>

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
	
	
	template <typename T>
	void writeToBuffer(T& data, size_t offset = 0)
	{
		std::memcpy(buffer.data() + offset, &data, sizeof(data));
	}
	template <typename T>
	void loadFromBuffer(T& data, size_t offset = 0)
	{
		std::memcpy(&data, buffer.data() + offset, sizeof(data));
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
		: strand(ioContext), resolver(strand), socket(strand), heartbeatTimer(strand)
	{
	}
	
	template <typename Handler>
	auto post(Handler handler)
	{
		return asio::post(strand, handler);
	}
	
	auto& getStrand() {return strand;}
	
	/// Timer ///
	
	std::atomic<bool> isDoingWork = false;
	std::chrono::seconds heartbeatInterval = std::chrono::seconds(15);
	void startHeartbeatTimer()
	{		
		heartbeatTimer.expires_after(heartbeatInterval);
		heartbeatTimer.async_wait([=](const Error& aborted)
		{
			if(aborted)
				return;
			
			Error err;
			if(!isDoingWork.load())
				asio::write(socket, asio::buffer(&data::HeartbeatCode, 1), err);
			if(err)
			{
				std::cout << "Error while sending heartbeat:\n " << err;
				return;
			}
			//std::cout << '+';
			startHeartbeatTimer();
		});
	}
	
	/// Basics ///
	
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
	
	// Register As Host
	
	bool registerAsHost(const data::Host::Header& header) {Error ignore; return registerAsHost(header, ignore);};
	bool registerAsHost(data::Host::Header header, Error& err);
	
	// Upload Map File
	
	bool uploadMapFile(const fs::path& mapFilePath, size_t maxSegmentLength = 0);
	
	// Download Map File
	
	bool downloadMapFile(data::IdType id, const fs::path& mapFilePath);
	
};





}

