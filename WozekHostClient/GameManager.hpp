#pragma once

#include "Imported.hpp"



class GameManager
{
public:
	
private:
	
	data::IdType hostId = 0;
	data::IdType worldId = 0;
	
	unsigned short udpStatePort = 12345; // TODO generate somehow
	
public:
	
	auto getTimestamp() { return std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now().time_since_epoch() ).count(); }
	
	void setHostId(data::IdType newId) {hostId = newId;}
	auto getHostId() {return hostId;}
	void setWorldId(data::IdType newId) {worldId = newId;}
	auto getWorldId() {return worldId;}
	void setUdpStatePort(unsigned short newPort) {udpStatePort = newPort;}
	auto getUdpStatePort() {return udpStatePort;}
	
	
	auto getUpdateStateHeader()
	{
		data::UpdateHostState::Header res;
		res.worldId = worldId;
		res.timepoint = getTimestamp();
		res.controllersCount = 0;
		
		return res;
	}
	
};

extern GameManager gameManager;
