#include <iostream>
#include <thread>
#include <memory>
#include <atomic>
#include <chrono>
#include <cstring>
#include <time.h>
#include <cstdlib>
#include <vector>

#include "App.hpp"


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

void default_setup(App& app)
{
	std::string address;
	std::cout << "Server Address: ";
	std::cin >> address;
	
	app.performDeviceAndNetworkingSetup(4, 9600, address, "8081", 8081, "Krzek", 1, 5000);
	//app.performDeviceAndNetworkingSetup(4, 9600, "127.0.0.1", "8081", 8081, "KrzekLocal", 1, 5000);
}



bool loadSetupConfigFromArgs(char** argv, App& app)
{
	try
	{
		const int comPort = stoi( std::string(argv[1]) );
		const int bdRate  = stoi( std::string(argv[2]) );
		const int udpPort = stoi( std::string(argv[5]) );
		const int asioThreads = stoi( std::string(argv[7]) );
		const int autoFetchStateInterval = stoi( std::string(argv[8]) );
		
		app.performDeviceAndNetworkingSetup(comPort, bdRate, argv[3], argv[4], udpPort, argv[6], asioThreads, autoFetchStateInterval);
	}
	catch(std::exception& e)
	{
		return false;
	}
	
	return true;
}


bool setup(int argc, char** argv, App& app)
{
	if(argc == 9)
	{
		if(!loadSetupConfigFromArgs(argv, app))
		{
			std::cout << "Error while parsing arguments.\n";
			return false;
		}
	}
	else
	{
		std::cout << "Arguments:\n <COM port nr>\n <Boud rate>\n <Server Hostname>\n <Server TCP Port>\n <Server UDP Port>\n <Controller Name>\n <Asio threads (at least 1)>\n <Auto Fetch State Interval (in ms) (default = 5000)>\n";
		std::cout << "Performing default setup.\n";
		default_setup(app);
	}
	return true;
}


void startManualControl(int argc, char** argv, App& app)
{
	
	try
	{
		char op;
		while(true)
		{
			
			std::cout << R"(
=======
Operations:
	1 - Update Rotation Manually
	2 - Show Rotation
	
	s - Start Main Loop Manually
	r - Restart
	
	q - Send Fetch State Request
	w - Enable/Disable Auto Fetch State Request
	e - Send UDP Echo Request
	
	c - Custom RS232 Request
	
	x - Exit
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
				
				app.state.rotationState.updateValue(vals);				
				continue;
			}
			if(op == '2')
			{
				app.state.rotationState.mutex.lock();
				
				byte encoded[getEncodedLength(sizeof(app.state.rotationState.value))];
				encode<4, sizeof(app.state.rotationState.value)>((byte*)&app.state.rotationState.value, encoded);
				std::cout << "Raw: ";
				lookupBytes((byte*)&app.state.rotationState.value, sizeof(app.state.rotationState.value));
				std::cout << "\nEnc: ";
				lookupBytes(encoded, sizeof(encoded));
				std::cout << '\n';
				
				app.state.rotationState.mutex.unlock();
				continue;
			}
			if(op == 'e')
			{
				std::string message;
				std::cout << "Message: ";
				std::cin >> message;
				
				app.udpSender.pushCallbackStack([](CallbackResult::Ptr result)
				{
					if (result->isCritical()) {
						std::cout << "Critical error occured\n";
					} else {
						std::cout << "Callback returned: " << int(result->status) << '\n';
					}
				});
				app.udpSender.sendEchoMessage(message);
				
				continue;
			}
			if(op == 'q')
			{
				app.fetchStateUpdate();
				continue;
			}
			if(op == 'w')
			{
				app.enableAutoFetchState.store( !app.enableAutoFetchState.load(order_relaxed) , order_relaxed);
				continue;
			}
			if(op == 's')
			{
				app.runLoop();
				continue;
			}
			if(op == 'r')
			{
				setup(argc, argv, app);
				continue;
			}
			if(op == 'c')
			{
				int temp = 0;
				App::CustomRequest request;
				
				std::cout << "Opcode: ";
				std::cin >> temp;
				request.opcode = temp;
				
				std::cout << "Expected Response Length: ";
				std::cin >> temp;
				request.expectedResponseLength = temp;
				
				
				std::cout << "Data (-1 - end): ";
				while(true)
				{
					std::cin >> temp;
					if(temp < 0)
					{
						break;
					}
					request.data.push_back(temp);
				}
				
				
				app.customRequestsQueue.push(request);
				
				continue;
			}
			if(op == 'x')
			{
				app.terminate();
				return;
			}
		}
	}
	catch(std::exception& e)
	{
		std::cout << "Exception in f1: " << e.what() << '\n';
	}
}
int main(int argc, char** argv)
{
	srand(time(NULL));
	rand();
	
	try
	{	
		asio::io_context ioContext;
		asio::executor_work_guard<decltype(ioContext.get_executor())> work{ioContext.get_executor()};
		
		App app(ioContext);
		if(!setup(argc, argv, app))
		{
			return 0;
		}
		
		startManualControl(argc, argv, app);
		app.join();
	}
	catch(std::exception& e)
	{
		std::cout << "Exception in Main: " << e.what() << '\n';
	}
	
	std::string block;
	std::cin >> block;
	std::cin >> block;
	std::cin >> block;
}


