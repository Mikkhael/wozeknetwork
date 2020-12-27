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
	
	UpdateableStateComponent<RotationState, 5> rotationState;
};
