#ifndef DLL
static_assert(false);
#endif // DLL

#include "dllexports.h"

EXPORT Handle* initialize()
{
	return new Handle;
}

EXPORT void terminate(Handle* handle)
{
	delete handle;
}

EXPORT void pollOne(Handle* handle)
{
	if(handle->ioContext.stopped())
		handle->ioContext.restart();
	handle->ioContext.poll_one();
}
EXPORT void pollAll(Handle* handle)
{
	if(handle->ioContext.stopped())
		handle->ioContext.restart();
	handle->ioContext.poll();
}
EXPORT void run(Handle* handle)
{
	if(handle->ioContext.stopped())
		handle->ioContext.restart();
	handle->ioContext.run();
}
EXPORT void stop(Handle* handle)
{
	handle->ioContext.stop();
}


EXPORT bool connectToServer(Handle* handle, const char* address, const char* port)
{	
	if(!handle->tcpConnection.resolveAndConnect( address, port ))
	{
		return false;
	}
	
	asioudp::endpoint endpoint(handle->tcpConnection.getRemote().address(), handle->tcpConnection.getRemote().port());
	if(!handle->udpSender.connect(endpoint))
	{
		return false;
	}
	
	if(!handle->udpServer.start(0))
	{
		return false;
	}
	
	return true;
}

EXPORT void disconnectFromServer(Handle* handle)
{
	//handle->tcpConnection.cancelHeartbeat();
	handle->tcpConnection.shutdownSession();
}

EXPORT void setTcpEchoCallback( Handle* handle, Handle::EchoCallback callback ){
	handle->tcpEchoCallback = callback;
}
EXPORT void sendTcpEcho(Handle* handle, const char* message, const uint32_t messageLength)
{
	std::string s;
	s.resize(messageLength);
	std::memcpy(s.data(), message, messageLength);
	
	handle->tcpConnection.pushCallbackStack([handle, messageLength](CallbackResult::Ptr result){
		if (result->status != CallbackResult::Status::Good) {
			handle->tcpEchoCallback(nullptr, 0);
		} else {
			auto valueResult = dynamic_cast< ValueCallbackResult<std::string>* >(result.get());
			handle->tcpEchoCallback(valueResult->value.c_str(), valueResult->value.size());
		}
	});
	handle->tcpConnection.performEchoRequest(message);
}


EXPORT void setUdpEchoCallback( Handle* handle, Handle::EchoCallback callback ){
	handle->udpEchoCallback = callback;
}
EXPORT void sendUdpEcho( Handle* handle, const char* message, const uint32_t messageLength)
{
	std::string s;
	s.resize(messageLength);
	std::memcpy(s.data(), message, messageLength);
	
	handle->udpSender.pushCallbackStack([handle](CallbackResult::Ptr result){
		if (result->status != CallbackResult::Status::Good) {
			handle->udpEchoCallback(nullptr, 0);
		} else {
			// nop
		}
	});
	handle->udpSender.sendEchoMessage(message);
}

EXPORT void setUdpUpdateStateErrorCallback(Handle* handle, Handle::ErrorCallback callback)
{
	handle->udpUpdateStateErrorCallback = callback;
}
EXPORT void sendUdpUpdateState( Handle* handle, data::IdType id, data::RotationType r1, data::RotationType r2, data::RotationType r3)
{
	data::RotationType rotation[3];
	rotation[0] = r1;
	rotation[1] = r2;
	rotation[2] = r3;
	handle->udpSender.pushCallbackStack([handle](CallbackResult::Ptr result){
		if (result->status != CallbackResult::Status::Good) {
			handle->udpUpdateStateErrorCallback();
		}
	});
	handle->udpSender.sendUdpStateUpdate(rotation, id);
}



/*
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
*/


#undef EXPORT
