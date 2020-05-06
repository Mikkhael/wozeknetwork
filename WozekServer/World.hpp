#pragma once

#include "asioWrapper.hpp"
#include "Datagrams.hpp"
#include <vector>

class World
{
	std::optional<asio::io_context::strand> strand;
	
	struct Host
	{
		data::IdType id = 0;
		asioudp::endpoint remoteEndpoint;
	};
	
	struct Controller
	{
		data::IdType id = 0;
		asioudp::endpoint remoteEndpoint;
		
		data::Controller::DesiredState desiredState;
		data::Controller::MeasuredState measuredState;
	};
	
	Host mainHost;
	std::vector<Controller> controllers;
	
public:
	
	void createStrand(asio::io_context& ioContext) {strand.emplace(ioContext);}
	bool hasStrand() {return strand.has_value();}
	auto& getStrand() {return strand.value();}
	
	void setMainHost(data::IdType id, const asioudp::endpoint& remoteEndpoint)
	{
		assert(mainHost.id == 0);
		mainHost.id = id;
		mainHost.remoteEndpoint = remoteEndpoint;
	}
	
	const auto& getMainHostRemoteEndpoint()
	{
		return mainHost.remoteEndpoint;
	}
	
	bool validateMainHostEndpoint(const asioudp::endpoint& senderEndpoint)
	{
		return senderEndpoint.address() == mainHost.remoteEndpoint.address();
	}
	
};
