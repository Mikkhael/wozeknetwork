#pragma once

#include "Imported.hpp"
#include "ControllerSessionClient.hpp"
#include "ControllerSessionUDP.hpp"

#include "ThreadedQueues.hpp"

#include "State.hpp"
#include <thread>
#include <vector>
#include <chrono>


class App
{
public:
	State state;
	Device device;
	asio::io_context& ioContext;
	
	tcp::ControllerSessionClient tcpConnection;
	udp::ControllerUDPServer udpServer;
	udp::ControllerUDPSender udpSender;
	
	struct CustomRequest{ byte opcode; std::vector<byte> data; int expectedResponseLength;};
	MutexCopyQueue<CustomRequest> customRequestsQueue;
	
	data::IdType controllerId;
	std::string controllerName;
	
	std::chrono::milliseconds autoFetchStateInterval = std::chrono::milliseconds(5000);
	std::atomic<bool> enableAutoFetchState = true;
	
private:
	
	template <typename ...Args>
	void log(const Args& ...args)
	{
		(std::clog << ... << args) << '\n';
	}
	template <typename ...Args>
	void logInline(const Args& ...args)
	{
		(std::clog << ... << args);
	}
	
	std::vector< std::thread > asioThreadsPool;
	auto getAsioThreadHandler()
	{
		return [this]{
			try
			{
				log("Started Asio Thread [", std::this_thread::get_id(), "]");
				ioContext.run();
			}
			catch(std::exception& e)
			{
				log("Cought exception in Asio Thread [", std::this_thread::get_id(), "]: ", e.what());
			}
			log("Asio Thread [", std::this_thread::get_id(), "] Treminated.");
		};
	}
	void destroyAsioThreads()
	{
		log("Restarting asio context...");
		ioContext.stop();
		for(auto& t : asioThreadsPool)
		{
			t.join();
		}
		ioContext.restart();
		asioThreadsPool.resize(0);
	}
	std::thread mainLoopThread;
	
	constexpr static auto ReconnectionInterval   = std::chrono::milliseconds(500);
	constexpr static auto ReconnectionRelaxation = std::chrono::milliseconds(1000);
	
	void reconnectDevice()
	{
		logInline("Reconnecting");
		device.disconnect();
		while(!device.connect())
		{
			logInline('.');
			std::this_thread::sleep_for(ReconnectionInterval);
		}
		log("\nConnected.");
		std::this_thread::sleep_for(ReconnectionRelaxation);
		log("Ready.");
	}
	
	template <typename T>
	auto checkAndUpdateSingleStateComponent(T& component)
	{
		auto res = component.checkIfUpdatedAndPostRequest(device);
		if(!res)
		{
			log("Error while writing: ", device.lastError);
			reconnectDevice();
		}
		return res;
	}
	
	static auto getTimeNow()
	{
		return std::chrono::high_resolution_clock::now();
	}
	
	std::chrono::time_point<std::chrono::high_resolution_clock> autoFetchStateCounter;
	inline void loop();
	
public:
	
	App(asio::io_context& ioContext_)
		: ioContext(ioContext_), tcpConnection(ioContext_), udpServer(ioContext_), udpSender(ioContext_, udpServer.getSocket())
	{
		udp::ControllerUDPReceiver::setAssociatedState(&state);
	}
	
	std::atomic<bool> runMainLoopThread = true;
	void runLoop()
	{
		runMainLoopThread.store(true, order_relaxed);
		mainLoopThread = std::thread([this]{
			try
			{
				while(runMainLoopThread.load(order_relaxed))
				{
					loop();
				}
				log("Main Loop Thread terminated");
			}
			catch(std::exception& e)
			{
				log("Cought exception in main loop: ", e.what());
			}
		});
	}
	
	void terminate()
	{
		destroyAsioThreads();
		runMainLoopThread.store(false, order_relaxed);
		std::this_thread::sleep_for(std::chrono::seconds(2));
	}
	
	bool performDeviceAndNetworkingSetup(const int comPort, const int bdRate, const std::string serverHostname, const std::string tcpPort, const short udpPort, const std::string controllerName, const int asioThreads, const unsigned int autoFetchStateIntervalValue)
	{
		autoFetchStateInterval = std::chrono::milliseconds(autoFetchStateIntervalValue);
		
		terminate();
		
		log("-- Connecting to device.");
		device.setPort(comPort);
		device.setBdRate(bdRate);
		reconnectDevice();
		
		log("-- Setting asio threads. (", asioThreads, ")");
		
		asioThreadsPool.reserve(asioThreads);
		for(int i=0; i<asioThreads; i++)
		{
			asioThreadsPool.emplace_back(getAsioThreadHandler());
		}
		
		log("-- Connecting to server.");
		if(!tcpConnection.resolveAndConnect(serverHostname,tcpPort))
		{
			log("Unable to connect to server with hostanme " ,serverHostname, " and port ", tcpPort);
			return false;
		}
		
		log("-- Registering as Controller with name: ", controllerName);
		tcpConnection.pushCallbackStack([this, udpPort](CallbackResult::Ptr result){
			if (result->status == CallbackResult::Status::CriticalError)
			{
				std::cout << "Failed to register due to a critical error\n";
			}
			else if(result->status == CallbackResult::Status::Error)
			{
				std::cout << "Failed to register due to an error\n";
			}
			else
			{
				auto valueResult = dynamic_cast< ValueCallbackResult<data::IdType>* >(result.get());
				controllerId = valueResult->value;
				log("Registerd as Controller with id: ", controllerId);
				
				log("-- Setting UDP endpoint");
				udpSender.connect( asioudp::endpoint(tcpConnection.getRemote().address(), udpPort) );
				udpServer.start(0);
				
				log("-- Starting the main loop");
				runLoop();
			}
			tcpConnection.shutdownSession();
		});
		tcpConnection.registerAsControllerRequest(controllerName);
		return true; // TODO
	}
	
	void join()
	{
		mainLoopThread.join();
		for(auto& t : asioThreadsPool)
		{
			t.join();
		}
	}
	
	
	void fetchStateUpdate()
	{
		//log("Fetching State from server");
		udpSender.pushCallbackStack([this](CallbackResult::Ptr result)
		{
			if (result->isCritical()) {
				log("Critical Error occured during Fetching");
			} else {
				//log("Fetched successfully");
			}
		});
		udpSender.sendFetchStateRequest(controllerId);
	}
	
};

void App::loop()
{	
	std::this_thread::sleep_for(std::chrono::microseconds(500));
	
	if(checkAndUpdateSingleStateComponent(state.rotationState) == 1)
	{
		log("Updated Rotation State");
	}
	
	while(!customRequestsQueue.isEmpty())
	{
		const auto request = customRequestsQueue.topAndPop().value();
		
		auto res = device.postEncodedRequestAndAwaitResponse(request.opcode, request.data.data(), request.data.size(), request.expectedResponseLength);
		if(res < 0)
		{
			log("Error while performing custom request");
		}
		else
		{
			logInline("Response (", res, "): ");
			lookupBytes(device.readBuffer, res);
			log();
		}
	}
	
	if(autoFetchStateCounter < getTimeNow())
	{
		//std::cout << std::chrono::duration_cast<std::chrono::milliseconds>(getTimeNow().time_since_epoch()).count() << '\n';
		if(enableAutoFetchState.load(order_relaxed))
		{
			fetchStateUpdate();
		}
		autoFetchStateCounter = getTimeNow() + autoFetchStateInterval;
	}
}
