#include "tcp.hpp"

namespace tcp
{

bool Connection::registerAsHost(data::Host::Header header, Error& err)
{
	buffer[0] = data::RegisterNewHost::Code;
	writeToBuffer(header, 1);
	
	auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	std::cout << "Time: " << time << '\n';
	if(time % 3 == 0)
	{
		std::cout << "Waiting...";
		asio::steady_timer t(socket.get_executor(), std::chrono::seconds(10));
		t.wait();
		std::cout << "end\n";
	}
	
	sendFromBuffer(sizeof(header) + 1, err);
	if(err)
	{
		return false;
	}
	
	receiveToBuffer(1, err);
	if(err)
	{
		return false;
	}
	
	if(buffer[0] != data::RegisterNewHost::Success)
	{
		return false;
	}
	
	receiveToBuffer(sizeof(header.id), err);
	
	if(err)
	{
		return false;
	}
	
	
	std::memcpy(&id, buffer.data(), sizeof(id));
	
	return true;
}




}
