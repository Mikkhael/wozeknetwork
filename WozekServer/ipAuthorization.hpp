#pragma once

#include "config.hpp"

#include <filesystem>
#include <mutex>

namespace fs = std::filesystem;

class IpAuthorizer
{
	struct Ipv4Address
	{
		uint32_t address;
		uint32_t netmask;
	};
	
	fs::file_time_type lastUpdatedIpv4;
	std::vector<Ipv4Address> allowedIpv4;
	std::optional<asio::steady_timer> updateIpv4Timer;
	
	std::mutex mutex;
	
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
		std::ifstream file(config.allowedIpv4FilePath);
		
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
		
		{
			std::lock_guard lock(mutex);
			allowedIpv4 = std::move(newAllowed);
		}
		
		return true;
	}
	
	void startUpdateIpv4Timer()
	{
		updateIpv4Timer.value().expires_after(config.updateIpv4TimerDuration);
		updateIpv4Timer.value().async_wait([this](const Error& err){
			if(err)
				return;
			const auto newLastUpdatedIpv4 = fs::last_write_time(config.allowedIpv4FilePath);
			if(lastUpdatedIpv4 != newLastUpdatedIpv4)
			{
				lastUpdatedIpv4 = newLastUpdatedIpv4;
				updateIpv4();
				startUpdateIpv4Timer();
			}
		});
	}
	
public:
	
	void init(asio::io_context& ioContext)
	{
		updateIpv4Timer.emplace(ioContext);
		
		std::fstream file(config.allowedIpv4FilePath, std::ios::app);
		file.close();
		
		updateIpv4();
		startUpdateIpv4Timer();
		lastUpdatedIpv4 = fs::last_write_time(config.allowedIpv4FilePath);
	}
	
	bool checkIfIpv4IsAllowed(const uint32_t address)
	{
		std::lock_guard lock(mutex);
		//std::cout << "Compare: " << address << "\n";
		for(auto& a : allowedIpv4)
		{
			//std::cout << '\t' << a.netmask << '\t' << (address & a.netmask) << '\t' << a.address << '\n';
			if((address & a.netmask) == a.address)
				return true;
		}
		return false;
	}
	
#ifndef RELEASE
	void show()
	{
		std::cout << "Adresses: \n";
		for(auto& add : allowedIpv4)
		{
			std::cout << add.address << ' ' << add.netmask << '\n';
		}
	}
#endif // RELEASE
};

extern IpAuthorizer ipAuthorizer;
