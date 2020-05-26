#pragma once
#ifdef DLL

#include "Everything.hpp"


struct Handle
{
	asio::io_context ioContext;
public:
	
	Handle()
		: ioContext(), tcpConnection(this->ioContext), udpConnection(this->ioContext)
	{
	}
	
	tcp::Connection tcpConnection;
	udp::Connection udpConnection;
	
	void run()
	{
		if(ioContext.stopped())
			ioContext.restart();
		ioContext.run();
	}
};

auto getDefaultTcpCallback(Handle* handle, std::function<void(int)> callback )
{
	return [=](const tcp::Connection::CallbackCode r)
	{
		handle->tcpConnection.reset();
		if(r == tcp::Connection::CallbackCode::Success)
		{
			//std::cout << "Success\n";
			callback(0);
		}
		else if(r == tcp::Connection::CallbackCode::Error)
		{
			//std::cout << "Error\n";
			callback(1);
		}
		else if(r == tcp::Connection::CallbackCode::CriticalError)
		{
			//std::cout << "Critical Error. Disconnecting.\n";
			handle->tcpConnection.disconnect();
			callback(2);
		}
	};
}

#endif // DLL
