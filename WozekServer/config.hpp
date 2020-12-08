#pragma once

#include "asio_lib.hpp"
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
		
	static constexpr size_t SessionBufferSize = 4096;
	
	fs::path allowedIpv4FilePath;
	std::chrono::seconds updateIpv4TimerDuration;
	
	bool init(const fs::path& allowedIpv4FilePath, const std::chrono::seconds& updateTimerDuration)
	{
		this->allowedIpv4FilePath = allowedIpv4FilePath;
		updateIpv4TimerDuration = updateTimerDuration;
		
		return true;
	}
	
};





extern Config config;

