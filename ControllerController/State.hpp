#pragma once

#include "Device.hpp"

#include <thread>
#include <memory>
#include <atomic>
#include <mutex>
#include <cstdlib>
#include <cstring>

template <typename T, int Opcode>
struct UpdateableStateComponent
{
	constexpr static int opcode = Opcode;
	T value;
	
	std::atomic_bool updatedFlag = false;
	std::mutex mutex;
	
	void updateValue(const void* newValueData)
	{
		mutex.lock();
		std::memcpy(&value, newValueData, sizeof(T));
		updatedFlag.store(true, std::memory_order::memory_order_relaxed);
		mutex.unlock();
	}
	
	void postRequest(Device& device)
	{
		mutex.lock();
		device.postEncodedRequest<Opcode, sizeof(T)>(&value);
		mutex.unlock();
	}
	
	
	// 0 - error 1 - updated 2 - up-to-date
	int checkIfUpdatedAndPostRequest(Device& device)
	{
		int res = 2;
		if(updatedFlag.load(std::memory_order::memory_order_relaxed))
		{
			mutex.lock();
			res = device.postEncodedRequest<Opcode, sizeof(T)>((const byte*)&value);
			updatedFlag.store(false, std::memory_order::memory_order_relaxed);
			mutex.unlock();
		}
		return res;
	}
};

struct State
{
	
	struct RotationState
	{
		data::RotationType X = 0, Y = 0, Z = 0;
	};
	static_assert(sizeof(RotationState) == 3 * 1 && sizeof(data::RotationType) == 1);
	
	struct Trzy
	{
		char a1 = 0, a2 = 0;
	};
	
	UpdateableStateComponent<RotationState, 4> rotationState;
	UpdateableStateComponent<Trzy, 3> trzy;
	
	
	byte* customRequestBuffer[512]{};
	std::atomic_int customRequestLength = 0;
};


template <typename ...Args>
void log(const Args& ...args)
{
	(std::cout << ... << args) << '\n';
}


inline void sustainConnectionAndUpdate(Device& device, State& state)
{
	constexpr static auto ReconnectionInterval   = std::chrono::milliseconds(500);
	constexpr static auto ReconnectionRelaxation = std::chrono::milliseconds(1000);
	
	try
	{
		auto reconnect = [&]()
		{
			//log("Reconnecting...");
			std::cout << "Reconnecting";
			device.disconnect();
			while(!device.connect())
			{
				std::cout << '.';
				std::this_thread::sleep_for(ReconnectionInterval);
			}
			log("\nConnected.");
			std::this_thread::sleep_for(ReconnectionRelaxation);
			log("Ready.");
		};
		
		auto updateComponent = [&](auto& component)
		{
			auto res = component.checkIfUpdatedAndPostRequest(device);
			if(!res)
			{
				log("Error while writing: ", device.lastError);
				reconnect();
			}
			return res;
			//component.mutex.lock();
			//showHeaderAndData("S: ", header, (byte*)&component.value, sizeof(component.value));
			//auto res = device.postEncodedRequest<decltype(component)::opcode, sizeof(component.value)>((byte*)&component.value);
			//component.mutex.unlock();
		};
		
		reconnect();
		while(true)
		{
			if(updateComponent(state.rotationState) == 1)
			{
				std::cout << "Updated\n";
			}
			//std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}
		
	}
	catch(std::exception& e)
	{
		std::cout << "Exception in Sustain: " << e.what() << '\n';
	}
}
