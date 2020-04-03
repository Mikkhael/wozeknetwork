#include "tcp.hpp"

namespace tcp
{

bool Connection::registerAsHost(data::Host::Header header, Error& err)
{
	buffer[0] = data::RegisterNewHost::Code;
	writeToBuffer(header, 1);
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
