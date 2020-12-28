#pragma once
#ifdef DLL

#include "Imported.hpp"
#include "ClientTCP.hpp"
#include "ClientUDP.hpp"

#define EXPORT extern "C" __declspec(dllexport) __stdcall


static __stdcall void NOOP_echo   (const char*, const uint32_t) {}
static __stdcall void NOOP_error  () {}
static __stdcall void NOOP_lookup (const int32_t) {}

struct Handle
{
	asio::io_context ioContext;
	
	tcp::WozekSessionClient tcpConnection;
	udp::WozekUDPServer udpServer;
	udp::WozekUDPSender udpSender;
	
	using EchoCallback   = decltype(&NOOP_echo);
	using ErrorCallback  = decltype(&NOOP_error);
	using LookupCallback = decltype(&NOOP_lookup);
	
	EchoCallback  	tcpEchoCallback;
	EchoCallback  	udpEchoCallback;
	ErrorCallback 	udpUpdateStateErrorCallback;
	LookupCallback 	tcpLookupIdForNameCallback;
	
	asio::executor_work_guard<asio::io_context::executor_type> work;
	
	Handle()
		: ioContext(), tcpConnection(this->ioContext), udpServer(this->ioContext), udpSender(this->ioContext, udpServer.getSocket()),
			tcpEchoCallback(&NOOP_echo), udpEchoCallback(&NOOP_echo), udpUpdateStateErrorCallback(&NOOP_error), tcpLookupIdForNameCallback(&NOOP_lookup),
			work(ioContext.get_executor())
	{
		AsioAsync::setGlobalAsioContext(ioContext);
		udp::WozekUDPReceiver::associatedHandle = this;
	}
};

/// DOCS

// Available functions: 

EXPORT Handle* initialize(); 				// Initialize the application and return the handle (64-bit). there cannot be more than 1 application running at a time
EXPORT void terminate(Handle* handle);		// Terminate the applictation attached to the handle. Frees up memory

// Connects to the specified server. Accepts null terminated strings as address (can be a domain name) and port
// Returns a bool, determining wheater the connection was sucessfull
EXPORT bool connectToServer(Handle* handle, const char* address, const char* port);

// Disconnects from TCP server
EXPORT void disconnectFromServer(Handle* handle);


// Asio is a (mostly) non blocking interface, so it has to be designated a thread or specifiad time, when it can execute

EXPORT void pollOne(Handle* handle); // Processes and runs execly one queued handler (if none event handlers are ready for execution, returns immidiatly)
EXPORT void pollAll(Handle* handle); // Processes and runs all queued handlers ready for execution (if none are ready, returns immidietly)
EXPORT void run(Handle* handle);	 // Processes handlers indefinetly, until stopped. Should be best tun on a seperate thread, or even multiple threads
EXPORT void stop(Handle* handle);	 // Stops the polling and execution of all handlers. run() adn runOne() return as only they stop processing current handler, or immidiatly, if no work is being done

// Bellow functions require a callback to be passed to them. Below functions and callbacks will be executed only in a thread with either pollOne() or run()
// Callbacks need to be set only once
// Sends an TCP/UDP Echo request. Callback will pass the pointer to the received response string and it's length via the parameter. If request failed, null-pointer will be passed. Ueed for testing.
EXPORT void setTcpEchoCallback( Handle* handle, Handle::EchoCallback callback);
EXPORT void sendTcpEcho( Handle* handle, const char* message, const uint32_t messageLength);

EXPORT void setUdpEchoCallback( Handle* handle, Handle::EchoCallback callback);
EXPORT void sendUdpEcho( Handle* handle, const char* message, const uint32_t messageLength);



// Sends UDP Update State Request with given rotation for controller with given id
EXPORT void setUdpUpdateStateErrorCallback( Handle* handle, Handle::ErrorCallback callback);
EXPORT void sendUdpUpdateState( Handle* handle, data::IdType id, data::RotationType r1, data::RotationType r2, data::RotationType r3);

static_assert(std::is_same_v<  data::IdType,       uint32_t  >);
static_assert(std::is_same_v<  data::RotationType, uint8_t   >);

// Sends TCP name lookup for given id
// Callback is executed with argument: -1 - error, 0 - name dosen't exist, else - result id
EXPORT void setTcpLookupIdForNameCallback( Handle* handle, Handle::LookupCallback callback);
EXPORT void sendTcpLookupIdForName( Handle* handle, const char* name, const uint32_t nameLength);


/// DOCS END


#endif // DLL
