#pragma once

#include "Everything.hpp"

#include <string>
#include <iostream>
#include "ClientTCP.hpp"
#include "ClientUDP.hpp"
#include <filesystem>
namespace fs = std::filesystem;

class Commander
{
	asio::io_context& ioContext;
	
	std::map< std::string, std::pair< std::string, void (Commander::*)() > > MenuMap;

public:
	
	
	tcp::WozekSessionClient tcpConnection;
	udp::WozekUDPServer udpServer;
	udp::WozekUDPSender udpSender;
	//udp::Connection udpConnection;
	
	Commander(asio::io_context& ioContext_)
		: ioContext(ioContext_), tcpConnection(ioContext_), udpServer(ioContext_), udpSender(ioContext_, udpServer.getSocket()) //, udpConnection(ioContext_)
	{
		MenuMap.insert({ "1", {"Connect to TCP Server", &Commander::connectToServer} });
		MenuMap.insert({ "2", {"Send TCP Heartbeat", &Commander::sendTcpHeartbeat} });
		MenuMap.insert({ "3", {"Send TCP Echo Message", &Commander::sendTcpEchoMessage} });
		MenuMap.insert({ "4", {"Set UDP Endpoint", &Commander::setUdpEndpoint} });
		MenuMap.insert({ "5", {"Send UDP Echo Message", &Commander::sendUdpEchoMessage} });
		MenuMap.insert({ "6", {"Send UDP State Update", &Commander::sendUdpStateUpdate} });
		
		udpServer.start(0);
		udpSender.connect(asioudp::endpoint(asio::ip::make_address_v4("127.0.0.1"), 8081));
	}
	
	void start()
	{
		AsioAsync::post([=]{enterMenu();});
	}
	void enterMenuAsync()
	{
		AsioAsync::post([=]{enterMenu();});
	}
	
	void enterMenu()
	{
		std::cout << "========\nChoose an option:\n";
		for(const auto& op : MenuMap) {
			std::cout << op.first << '\t' << op.second.first << '\n';
		}
		std::cout << "======== ";
		std::string option;
		std::cin >> option;
		
		auto op = MenuMap.find(option);
		if(op != MenuMap.end())
		{
			AsioAsync::post([=]{ ((this)->*(op->second.second))(); });
			return;
		}
		enterMenuAsync();
	}
	
	void connectToServer()
	{
		tcpConnection.shutdownSession();
		std::string hostname, port;
		std::cout << "Hostname & Port: ";
		std::cin >> hostname >> port;
		
		if(!tcpConnection.resolveAndConnect( hostname, port ))
		{
			std::cout << "Failed to connect\n";
		}
		else
		{
			std::cout << "Connected successfully\n";
		}
		
		enterMenuAsync();
	}
	void sendTcpHeartbeat()
	{
		std::cout << "Send Heartbeat\n";
		tcpConnection.pushCallbackStack([=](CallbackResult::Ptr result){
			if (result->isCritical()) {
				std::cout << "Critilac error occured\n";
			} else {
				std::cout << "Heartbeat Sent Successfully\n";
			}
			enterMenuAsync();
		});
		tcpConnection.sendHeartbeat();
	}
	void sendTcpEchoMessage()
	{
		std::cout << "Send TCP Echo Message\nEnter Messgae:";
		std::string message;
		std::cin >> message;
		tcpConnection.pushCallbackStack([=](CallbackResult::Ptr result){
			if (result->isCritical()) {
				std::cout << "Critical error occured\n";
			} else {
				auto valueResult = dynamic_cast< ValueCallbackResult<std::string>* >(result.get());
				std::cout << "Received Echo response with message: " << valueResult->value << '\n';
			}
			enterMenuAsync();
		});
		tcpConnection.performEchoRequest(message);
	}
	void setUdpEndpoint()
	{
		std::cout << "UDP Address: ";
		std::string address;
		std::cin >> address;
		std::cout << "UDP Port: ";
		short port;
		std::cin >> port;
		
		asioudp::endpoint endpoint(asio::ip::make_address_v4(address), port);
		if(!udpSender.connect(endpoint))
		{
			std::cout << "Setting the endpoint failed. (" << endpoint << ")\n";
		}
		else
		{
			std::cout << "Setting the endpoint succeded. (" << endpoint << ")\n";
		}
		
		enterMenuAsync();
	}
	void sendUdpEchoMessage()
	{
		std::cout << "Send UDP Echo Message\nEnter Message:";
		std::string message;
		std::cin >> message;
		udpSender.pushCallbackStack([=](CallbackResult::Ptr result){
			if (result->isCritical()) {
				std::cout << "Critical error occured\n";
			} else {
				std::cout << "UDP Echo Request Callback Status: " << int(result->status) << '\n';
			}
			enterMenuAsync();
		});
		udpSender.sendEchoMessage(message);
	}
	void sendUdpStateUpdate()
	{
		
		int temp;
		data::RotationType rotation[3];
		data::IdType id;
		
		std::cout << "New Rotation 1:";
		std::cin >> temp;
		rotation[0] = temp;
		std::cout << "New Rotation 2:";
		std::cin >> temp;
		rotation[1] = temp;
		std::cout << "New Rotation 3:";
		std::cin >> temp;
		rotation[2] = temp;
		std::cout << "Controller Id: ";
		std::cin >> id;
		
		udpSender.pushCallbackStack([=](CallbackResult::Ptr result){
			if (result->isCritical()) {
				std::cout << "Critical error occured\n";
			} else {
				std::cout << "UDP Echo Request Callback Status: " << int(result->status) << '\n';
			}
			enterMenuAsync();
		});
		udpSender.sendUdpStateUpdate(rotation, id);
	}
};



/*
class Commander
{
	asio::io_context& ioContext;
	
public:
	
	tcp::Connection tcpConnection;
	udp::Connection udpConnection;
	
	Commander(asio::io_context& ioContext_)
		: ioContext(ioContext_), tcpConnection(ioContext_), udpConnection(ioContext_)
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

1. Connect to a server
2. Register as host
3. Send a map file
4. Download a map file
5. Start the world

6. Upload any file
7. Download any file
---
UDP functions:
a. Send "UpdateState"

=============
)";
	}
	auto getDefaultUdpCallback()
	{
		return [this](const udp::Connection::CallbackCode r){
			tcpConnection.reset();
			if(r == udp::Connection::CallbackCode::Success)
			{
				std::cout << "Success\n";
			}
			else if(r == udp::Connection::CallbackCode::Error)
			{
				std::cout << "Error\n";
			}
			postGetCommand();
		};
	}
	auto getDefaultCallback()
	{
		return [this](const tcp::Connection::CallbackCode r){
			tcpConnection.reset();
			if(r == tcp::Connection::CallbackCode::Success)
			{
				std::cout << "Success\n";
			}
			else if(r == tcp::Connection::CallbackCode::Error)
			{
				std::cout << "Error\n";
			}
			else if(r == tcp::Connection::CallbackCode::CriticalError)
			{
				std::cout << "Critical Error. Disconnecting.\n";
				tcpConnection.disconnect();
			}
			postGetCommand();
		};
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
			case '5':
				{
					startTheWorld();
					return;
				}
			case '6':
				{
					uploadAnyFile();
					return;
				}
			case '7':
				{
					downloadAnyFile();
					return;
				}
			case 'a':
				{
					udpUpdateState();
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
		
		std::cout << "Connecting..";
		
		udpConnection.setRemoteEndpoint(ip, port);
		
		std::cout << "..";
		
		tcpConnection.cancelHeartbeat();
		
		std::cout << "..\n";
		
		tcpConnection.resolveAndConnect(ip, port, getDefaultCallback());
	}


	void registerHost()
	{
		std::string name;
		data::IdType id;
		
		std::cout << "Input host name (max 30 cahracters): ";
		if(!std::cin.eof())
			while(std::cin.peek() == '\n') std::cin.get();
		std::getline(std::cin, name);
		
		std::cout << "Input host id (set to 0 to automaticly get id from the server): ";
		std::cin >> id;
		
		data::Host::Header header;
		header.id = id;
		std::memcpy(&header.name, name.data(), sizeof(header.name));
		
		tcpConnection.cancelHeartbeat();
		tcpConnection.registerAsNewHost(header, getDefaultCallback());
	}

	void sendMapFile()
	{
		std::string path;
		
		std::cout << "Input path to the file to send: ";
		if(!std::cin.eof())
			while(std::cin.peek() == '\n') std::cin.get();
		std::getline(std::cin, path);
		
		tcpConnection.cancelHeartbeat();
		tcpConnection.uploadMap(path, getDefaultCallback());
	}
	
	void downloadMapFile()
	{
		
		std::string path;
		data::IdType id;
		
		std::cout << "Input id of the map: ";
		std::cin >> id;
		std::cout << "Input path where to save the file: ";
		if(!std::cin.eof())
			while(std::cin.peek() == '\n') std::cin.get();
		std::getline(std::cin, path);
		
		tcpConnection.cancelHeartbeat();
		tcpConnection.downloadMap(id, path, getDefaultCallback());
	}
	
	void startTheWorld()
	{		
		tcpConnection.cancelHeartbeat();
		tcpConnection.startTheWorld(getDefaultCallback());
	}
	
	void uploadAnyFile()
	{
		std::string path;
		std::string name;
		
		std::cout << "Input path to the file to send: ";
		if(!std::cin.eof())
			while(std::cin.peek() == '\n') std::cin.get();
		std::getline(std::cin, path);
		std::cout << "Input name of the file to send (only letters, numbers, '.' and '_' ; max 125 characters): ";
		if(!std::cin.eof())
			while(std::cin.peek() == '\n') std::cin.get();
		std::getline(std::cin, name);
		if(name.size() > 125)
			name.resize(125);
		
		tcpConnection.cancelHeartbeat();
		tcpConnection.uploadFile(path, name, getDefaultCallback());
	}
	
	void downloadAnyFile()
	{
		
		std::string path;
		std::string name;
		
		std::cout << "Input path to save the file as: ";
		if(!std::cin.eof())
			while(std::cin.peek() == '\n') std::cin.get();
		std::getline(std::cin, path);
		std::cout << "Input name of the file to download: ";
		if(!std::cin.eof())
			while(std::cin.peek() == '\n') std::cin.get();
		std::getline(std::cin, name);
		if(name.size() > 125)
			name.resize(125);
		
		tcpConnection.cancelHeartbeat();
		tcpConnection.downloadFile(path, name, getDefaultCallback());
	}
	
	void udpUpdateState()
	{
		udpConnection.updateState(getDefaultUdpCallback());
	}
};
*/
