#pragma once

#include "asioWrapper.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>
#include <optional>

namespace fs = std::filesystem;

class Config
{
public:
	
	struct Ipv4Address
	{
		uint32_t address;
		uint32_t netmask;
	};
	
private:
	
	fs::path allowedIpv4FilePath;
	std::vector<Ipv4Address> allowedIpv4;
	fs::file_time_type lastUpdatedIpv4;
	
	std::optional<Ipv4Address> parseAddressString(const std::string& str)
	{
		Ipv4Address newAddress {0, 0};
		if(str[0] < '0' || str[0] > '9')
			return {};
	
		std::stringstream ss(str);
		
		int temp = 256;
		char tempc;
		
		for(int i=0; i<4; i++)
		{
			if(!(ss >> temp >> tempc))
			{
				return {};
			}
			if(tempc != (i==3 ? '/' : '.') )
			{
				return {};
			}
			if(temp > 255)
			{
				return {};
			}
			newAddress.address <<= 8;
			newAddress.address += temp;
		}
		
		if(!(ss >> temp))
		{
			return {};
		}
		if(temp > 32)
		{
			return {};
		}
		newAddress.netmask = temp == 32 ? ~uint32_t(0) : ~(~uint32_t(0) >> temp);
		newAddress.address &= newAddress.netmask;
		return {newAddress};
	}
	
	
	
	bool updateIpv4()
	{
		std::vector<Ipv4Address> newAllowed;
		
		std::string line;
		std::ifstream file(allowedIpv4FilePath);
		
		if(!file.is_open())
		{
			return false;
		}
		
		while(std::getline(file, line))
		{
			auto newAddress = parseAddressString(line);
			if(newAddress.has_value())
			{
				newAllowed.push_back(newAddress.value());
			}
		}
		
		file.close();
		
		asio::post(getStrand(), [this, newAllowedMoved=std::move(newAllowed)]{
			allowedIpv4 = std::move(newAllowedMoved);
		});
		
		lastUpdatedIpv4 = fs::last_write_time(allowedIpv4FilePath);
		
		return true;
	}
	
	std::chrono::seconds updateIpv4TimerDuration;
	std::optional<asio::steady_timer> updateIpv4Timer;
	
	std::optional<asio::io_context::strand> strand;
	
	void startUpdateIpv4Timer()
	{
		updateIpv4Timer.value().expires_after(updateIpv4TimerDuration);
		updateIpv4Timer.value().async_wait([this](const Error& err){
			if(err)
				return;
			if(lastUpdatedIpv4 != fs::last_write_time(allowedIpv4FilePath))
			{
				updateIpv4();
			}
			startUpdateIpv4Timer();
		});
	}
	
public:
	
	
	
	void setStrand(asio::io_context& ioContext)
	{
		strand.emplace(ioContext);
		updateIpv4Timer.emplace(ioContext);
	}
	bool hasStrand() {return strand.has_value();}
	asio::io_context::strand& getStrand(){return strand.value();}
	
	bool init(const fs::path& allowedIpv4FilePath, const std::chrono::seconds& updateTimerDuration)
	{
		if(!hasStrand())
		{
			return false;
		}
		
		this->allowedIpv4FilePath = allowedIpv4FilePath;
		updateIpv4();
		
		updateIpv4TimerDuration = updateTimerDuration;
		startUpdateIpv4Timer();
		
		return true;
	}
	
	bool checkIfIpv4IsAllowed(const uint32_t address)
	{
		//std::cout << "Compare: " << address << "\n";
		for(auto& a : allowedIpv4)
		{
			//std::cout << '\t' << a.netmask << '\t' << (address & a.netmask) << '\t' << a.address << '\n';
			if((address & a.netmask) == a.address)
				return true;
		}
		return false;
	}
	
	template <typename Callback>
	void checkIfIpv4IsAllowedSafe(const uint32_t address, Callback callback)
	{
		asio::post(getStrand(), [=]{
			callback(checkIfIpv4IsAllowed(address));
		});
	}
	
	void show()
	{
		std::cout << "Adresses: \n";
		for(auto& add : allowedIpv4)
		{
			std::cout << add.address << ' ' << add.netmask << '\n';
		}
	}
	
};





extern Config config;

