#pragma once

#include "Everything.hpp"

#include <string>
#include <iostream>
#include "TCP.hpp"

class Commander
{
	asio::io_context& ioContext;
	
public:
	
	tcp::Connection tcpConnection;
	
	Commander(asio::io_context& ioContext_)
		: ioContext(ioContext_), tcpConnection(ioContext_)
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
4. Download a map file
=============
)";
	}


	void getCommand()
	{
		while(true)
		{
			showHelp();
			
			char op;
			if(! (std::cin >> op) )
			{
				std::cin.clear();
				std::cin.ignore(std::numeric_limits<std::streamsize>::max());
				continue;
			}
			
			
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
			case '4':
				{
					downloadMapFile();
					return;
				}
			default:
				{
					std::cout << "Unknown command\n";
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
		
		auto callback = [=](bool result)
		{
			if(result)
			{
				std::cout << "Success\n";
			}
			else
			{
				std::cout << "Failure\n";
			}
			postGetCommand();
		};
		
		tcpConnection.resolveAndConnect(ip, port, callback);
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
		
		auto callback = [this](data::IdType id)
		{
			if(id == 0)
			{
				std::cout << "Failure\n";
			}
			else
			{
				std::cout << "Success\n Id: " << id << '\n';
			}
			postGetCommand();
		};
		
		tcpConnection.registerAsNewHost(header, callback);
	}

	void sendMapFile()
	{
		
	}
	
	void downloadMapFile()
	{
		
	}
	
};
