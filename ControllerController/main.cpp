#include <iostream>
#include <thread>
#include <memory>
#include <atomic>
#include <chrono>
#include <cstring>
#include <time.h>
#include <cstdlib>
#include <vector>
#include "ControllerSessionClient.hpp"
#include "ControllerSessionUDP.hpp"

#include "State.hpp"


void readAll(Device& device)
{
	auto& serial = device.serialPort;
	byte buffer;
	int err;
	
	int available;
	while(true)
	{
		//constexpr auto maxBufferSize = 425106752;
		while((available = serial.available()))
		{
			err = serial.readChar((char*)&buffer);
			std::cout << "R: ";
			lookupBytes(&buffer, 1);
			std::cout << std::dec << " (" << int(err) << ")  ";
			std::cout << " [" << available << "]\n";
		}
	}
}

void f1(State& state)
{
	data::IdType id = 0;
	
	asio::io_context ioContext;
	asio::executor_work_guard<decltype(ioContext.get_executor())> work{ioContext.get_executor()};
	
	tcp::ControllerSessionClient tcpConnection(ioContext);
	udp::ControllerUDPServer udpServer(ioContext);
	udp::ControllerUDPSender udpSender(ioContext, udpServer.getSocket());
	
	udp::ControllerUDPReceiver::setAssociatedState(&state);
	
	//udpSender.resolveAndConnect("localhost", "8081");
	udpSender.connect(asioudp::endpoint(asio::ip::make_address_v4("127.0.0.1"), 8081));
	
	udpServer.start(0);
	
	std::thread asioThread( [&]{ ioContext.run(); } );
	
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
	
	c - Connect to Server
	r - Register As Controller Request
	
	q - Send Fetch State Request
	e - Send UDP Echo Request
)";
			
			std::cin >> op;
			
			if(op == '1')
			{
				data::RotationType vals[3];
				
				for(int i=0; i<3; i++)
				{
					int temp;
					std::cout << "Value " << (i+1) << ": ";
					std::cin >> temp;
					vals[i] = temp;
				}
				
				state.rotationState.updateValue(vals);				
				continue;
			}
			if(op == '2')
			{
				state.rotationState.mutex.lock();
				
				byte encoded[getEncodedLength(sizeof(state.rotationState.value))];
				encode<4, sizeof(state.rotationState.value)>((byte*)&state.rotationState.value, encoded);
				std::cout << "Raw: ";
				lookupBytes((byte*)&state.rotationState.value, sizeof(state.rotationState.value));
				std::cout << "\nEnc: ";
				lookupBytes(encoded, sizeof(encoded));
				std::cout << '\n';
				
				state.rotationState.mutex.unlock();
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
			if(op == 'c')
			{
				std::string hostname, port;
				//int port;
				
				std::cout << "Hostname & port: ";
				std::cin >> hostname >> port;
				
				if(!tcpConnection.resolveAndConnect( hostname, port ))
				{
					std::cout << "Failed to connect\n";
				}
				else
				{
					std::cout << "Connected successfully\n";
				}
				
				continue;
			}
			if(op == 'r')
			{
				// c localhost 8081 r bajojajo2
				std::string name;
				std::cout << "Name: ";
				std::cin >> name;
				
				data::RegisterAsController::RequestHeader request;
				strcpy(request.name, name.data());
				
				tcpConnection.pushCallbackStack([=,&id](CallbackResult::Ptr result){
					if (result->isCritical()) {
						std::cout << "Received Register As Controller Critical Error\n";
					} else {
						auto valueResult = dynamic_cast< ValueCallbackResult<data::IdType>* >(result.get());
						std::cout << "Received Register As Controller Response With Id: " << (valueResult->value) << " Status: " << int(valueResult->status) << '\n';
						id = valueResult->value;
					}
				});
				tcpConnection.registerAsControllerRequest(name);
				
				continue;
			}
			if(op == 'e')
			{
				std::string message;
				std::cout << "Message: ";
				std::cin >> message;
				
				udpSender.pushCallbackStack([](CallbackResult::Ptr result)
				{
					if (result->isCritical()) {
						std::cout << "Critical error occured\n";
					} else {
						std::cout << "Callback returned: " << int(result->status) << '\n';
					}
				});
				udpSender.sendEchoMessage(message);
				
				continue;
			}
			if(op == 'q')
			{
				udpSender.pushCallbackStack([](CallbackResult::Ptr result)
				{
					if (result->isCritical()) {
						std::cout << "Critical error occured\n";
					} else {
						std::cout << "Callback returned: " << int(result->status) << '\n';
					}
				});
				udpSender.sendFetchStateRequest(id);
				
				continue;
			}
		}
	}
	catch(std::exception& e)
	{
		std::cout << "Exception in f1: " << e.what() << '\n';
	}
	
	asioThread.join();
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


