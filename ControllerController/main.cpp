#include <iostream>
#include <thread>
#include <memory>
#include <atomic>
#include <chrono>
#include <cstring>
#include <time.h>
#include <cstdlib>
#include <vector>

#include "State.hpp"


void readAll(Device& device)
{
	auto& serial = device.serialPort;
	byte buffer;
	int err;
	
	while(true)
	{
		while(serial.available())
		{
			err = serial.readChar((char*)&buffer);
			std::cout << "R: ";
			lookupBytes(&buffer, 1);
			std::cout << " (" << err << ")\n";
		}
	}
}



void f1(State& state)
{
	try
	{
		char op;
		while(true)
		{
			std::cout << R"(
=======
Operations:
	1 - Update Rotation
	2 - Show Rotation
	3 - Set Rotation to FF and Update
	4 - Custom Request
)";
			
			std::cin >> op;
			
			if(op == '1')
			{
				float vals[3];
				
				for(int i=0; i<3; i++)
				{
					std::cout << "Value " << (i+1) << ": ";
					std::cin >> vals[i];
				}
				
				state.rotationState.updateValue(vals);				
				continue;
			}
			if(op == '2')
			{
				byte encoded[getEncodedLength(sizeof(state.rotationState.value))];
				encode<4, sizeof(state.rotationState.value)>((byte*)&state.rotationState.value, encoded);
				std::cout << "Raw: ";
				lookupBytes((byte*)&state.rotationState.value, sizeof(state.rotationState.value));
				std::cout << "\nEnc: ";
				lookupBytes(encoded, sizeof(encoded));
				std::cout << '\n';
				continue;
			}
			if(op == '3')
			{
				for(auto i=0u; i<sizeof(state.rotationState.value); i++)
				{
					((byte*)&state.rotationState.value)[i] = byte(255);
				}
				state.rotationState.updatedFlag.store(true, std::memory_order::memory_order_relaxed);
				std::cout << "Set to FF and Updated\n";
				continue;
			}
			if(op == '4')
			{
				/*
				std::vector<byte> data;
				std::cout << "(-1 to end): ";
				while(true)
				{
					int temp;
					std::cin >> temp;
					if(temp == -1)
					{
						if(data.size() < 1)
						{
							continue;
						}
						std::memcpy(state.customRequestBuffer, data.data(), data.size());
					}
				}
				*/
				continue;
			}
		}
	}
	catch(std::exception& e)
	{
		std::cout << "Exception in f1: " << e.what() << '\n';
	}
}

int main()
{
	srand(time(NULL));
	rand();
	try
	{	
		State state;
		Device device(4);
		
		std::thread controlThread([&](){ f1(state); });
		std::thread readThread([&](){ readAll(device); });
		//std::thread updateThread([&](){ sustainConnectionAndUpdate(device, state); });	
		
		sustainConnectionAndUpdate(device, state);
		controlThread.join();
		//updateThread.join();
	}
	catch(std::exception& e)
	{
		std::cout << "Exception in Main: " << e.what() << '\n';
	}
}


