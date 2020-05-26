#ifndef DLL
static_assert(false);
#endif // DLL

#include "dllexports.h"

#define EXPORT extern "C" __declspec(dllexport) __stdcall

EXPORT Handle* initialize()
{
	return new Handle;
}

EXPORT void terminate(Handle* handle)
{
	delete handle;
}

EXPORT int connectToServer(Handle* handle, const char* address, const char* port)
{
	int res = 2;
	
	handle->udpConnection.setRemoteEndpoint(address, port);
	
	handle->tcpConnection.cancelHeartbeat();
	handle->tcpConnection.resolveAndConnect(address, port, 
		getDefaultTcpCallback( handle, [&res](const int code){
			res = code;
	}));
	
	handle->run();
	return res;
}

EXPORT int disconnectFromServer(Handle* handle)
{
	handle->tcpConnection.cancelHeartbeat();
	handle->tcpConnection.disconnect();
	
	handle->run();
	return 0;
}


EXPORT int uploadFile(Handle* handle, const char* filePath, const char* fileName)
{
	int res = 2;
	
	handle->tcpConnection.cancelHeartbeat();
	handle->tcpConnection.uploadFile(filePath, fileName,
		getDefaultTcpCallback( handle, [&res](const int code){
			res = code;
	}));
	
	handle->run();
	return res;
}

EXPORT int downloadFile(Handle* handle, const char* filePath, const char* fileName)
{
	int res = 2;
	
	handle->tcpConnection.cancelHeartbeat();
	handle->tcpConnection.downloadFile(filePath, fileName,
		getDefaultTcpCallback( handle, [&res](const int code){
			res = code;
	}));
	
	handle->run();
	return res;
}


#undef EXPORT
