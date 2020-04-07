#include "tcp.hpp"

namespace tcp
{

void Connection::registerAsNewHost(data::RegisterNewHost::Request reqHeader, std::function<void(data::IdType)> requestCallback )
{	
	auto errorHandler = [=](const Error& err)
	{
		logError(err);
		requestCallback(0);
	};
	
	log("Initiating Registration As New Host");
	
	size_t off = 0;
	off += writeObjectToBuffer(data::RegisterNewHost::Code, off);
	off += writeObjectToBuffer(reqHeader, off);
	
	asyncWrite(off, ioHandler(
	[=]{
		asyncRead(sizeof(data::RegisterNewHost::Response), ioHandler(
		[=]{
			data::RegisterNewHost::Response resHeader;
			readObjectFromBuffer(resHeader);
			
			if(resHeader.code == data::RegisterNewHost::Response::Failure)
			{
				requestCallback(0);
				return;
			}
			
			id = resHeader.id;
			requestCallback(id);
			return;
			
		}, errorHandler));
	}, errorHandler));
}



}
