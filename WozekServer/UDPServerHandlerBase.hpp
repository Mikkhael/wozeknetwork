#pragma once

#include "UDP.hpp"

namespace udp
{

class ServerHandlerBase
{
protected:
	
	template <typename T>
	bool loadObjectFromBufferSafe(T& obj, bufferType& buffer, const size_t length, size_t& off)
	{
		if(length < sizeof(T) + off)
			return false;
		
		std::memcpy(&obj, buffer.data() + off, sizeof(T));
		off += sizeof(T);
		return true;
	}
	
	template <typename T>
	void loadObjectFromBuffer(T& obj, bufferType& buffer, size_t& off)
	{		
		std::memcpy(&obj, buffer.data() + off, sizeof(T));
		off += sizeof(T);
	}
	
	
	template <typename ...Ts>
	void log(Ts&& ...args)
	{
		logger.output("UDP: ", std::forward<Ts>(args)... , '\n');
	}
	template <typename ...Ts>
	void logError(Logger::Error name, Ts&& ...args)
	{
		if constexpr (sizeof...(args) > 0)
		{
			logger.output("UDP Error {code: ", static_cast<int>(name), "} ", std::forward<Ts>(args)... , '\n');
		}
		logger.error(name);
	}
	
public:
	
};



}

