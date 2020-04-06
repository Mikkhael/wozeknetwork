#pragma once

#include "Everything.hpp"

#include <string>
#include <iostream>
#include "TCP.hpp"

class Commander
{
	asio::io_context& ioContext;
	
public:
	
	tcp::Connection connection;
	
	Commander(asio::io_context& ioContext_)
		: ioContext(ioContext_), connection(ioContext_)
	{
	}
	
	
	void start()
	{
		asio::dispatch(ioContext, [=]{connect();});
	}
	
	void postGetCommand()
	{
		asio::post(ioContext, [this]{ getCommand(); });
	}
	
	void showHelp()
	{
		std::cout <<
R"(
=============
What do you want to do?

1. Connect to a tcp server
2. Register as host
3. Send a map file
=============
)";
	}


	void getCommand()
	{
		while(true)
		{
			showHelp();
			
			char op;
			std::cin >> op;
			
			switch (op)
			{
			case '1':
				{
					connect();
					return;
				}
			case '2':
				{
					registerHost();
					return;
				}
			case '3':
				{
					sendMapFile();
					return;
				}
			default:
				{
					continue;
				}
			}
		}
		
		
	}


	void connect()
	{
		std::string ip;
		std::string port;
		
		std::cout << "Input the address and port to connect to:\n address: ";		
		std::cin >> ip;
		std::cout << " port: ";
		std::cin >> port;
	
		int hi;
		std::cout << "Heartbeat interval (in secconds): [default: 15] ";
		std::cin >> hi;
		
		std::cout << "Connecting...\n";
			
		Error err;
		connection.resolveAndConnect(ip, port, err);
		
		if(err)
		{
			std::cout << "Error during connection: " << err;
		}
		else
		{
			std::cout << "Connected successfully to " << connection.getRemote() << '\n';
			connection.heartbeatInterval = std::chrono::seconds(hi);
			connection.startHeartbeatTimer();
		}
		
		postGetCommand();
	}


	void registerHost()
	{
		std::string name;
		data::IdType id;
		
		std::cout << "Input host name (max 30 cahracters): ";
		std::cin >> name;
		if(name.size() > 30)
		{
			name.resize(30);
		}
		std::cout << "Input host id (set to 0 to automaticly get id from the server): ";
		std::cin >> id;
		
		data::Host::Header header;
		header.id = id;
		std::memcpy(&header.name, name.data(), name.size() + 1);
		
		connection.post([this, header]{
			if(connection.registerAsHost(header))
			{
				std::cout << "Registered successfuly with id: " << connection.getId() << '\n';
			}
			else
			{
				std::cout << "Registration failed\n";
			}
			postGetCommand();
		});
	}

	void sendMapFile()
	{
		fs::path filePath;
		size_t maxSegmentLength;
		
		std::cout << "Input path to the file: ";
		std::cin >> filePath;
		
		std::cout << "Input max segment length (0 for maximum buffer length): ";
		std::cin >> maxSegmentLength;
		
		connection.post([=]{
			if(connection.uploadMapFile(filePath, maxSegmentLength))
			{
				std::cout << "File sent successfuly\n";
			}
			else
			{
				std::cout << "File transfer failed\n";
			}
			postGetCommand();
		});
	}
	
};
