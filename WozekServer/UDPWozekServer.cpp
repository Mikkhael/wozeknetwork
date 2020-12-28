#include "UDPWozekServer.hpp"

#include "DatabaseManager.hpp"

namespace udp
{




void WozekUDPReceiver::handleRequest()
{
	char requestId;
	buffer.loadBytes(&requestId, 1);
	
	log("Handling request id: ", int(requestId));
	
	switch(requestId)
	{
		case data::EchoRequest::request_id :
		{
			handleEchoRequest();
			break;
		}
		case data::UdpFetchState::request_id :
		{
			handleFetchStateRequest();
			break;
		}
		case data::UdpUpdateState::request_id :
		{
			handleUpdateStateRequest();
			break;
		}
		default:
		{
			logError(Logger::Error::UdpUnknownCode, "Unrecognized UDP request code ", int(requestId));
		}
	}
}

void WozekUDPReceiver::handleFetchStateRequest()
{
	data::IdType id;
	buffer.loadObjectAt(1, id);
	
	auto& table = db::databaseManager.getDatabase().controllerTable;
	
	buffer.saveObject(char(data::UdpFetchState::response_id));
	
	table.accessSafeRead(id, [this, id](auto record){
		if(!record)
		{
			log("Id ", id, " not found.");
			return;
		}
		
		if(record->endpoint.address() != remoteEndpoint.address()) // TODO check port
		{
			log("Invalid endpoint. Expected: ", record->endpoint, ", received from: ", remoteEndpoint);
			return;
		}
		
		buffer.saveObjectAt(1, record->rotation);
		static_assert(sizeof(record->rotation) == sizeof(data::UdpFetchState::Response));
	});
	
	log("Sending Update State to ", remoteEndpoint);
	
	asyncWriteTo(
		buffer.get(1 + sizeof(data::UdpFetchState::Response)),
		remoteEndpoint,
		[]{},
		&WozekUDPReceiver::errorAbort
	);
	
	return;
}

void WozekUDPReceiver::handleEchoRequest()
{
	log("Handling Echo Request. Sending back to ", remoteEndpoint);
	buffer.saveObject(char(data::EchoRequest::response_id));
	asyncWriteTo(
		buffer.get(bytesTransfered),
		remoteEndpoint,
		[](){},
		&WozekUDPReceiver::errorAbort
	);
}

void WozekUDPReceiver::handleUpdateStateRequest()
{
	log("Handling Update State Request");
	auto& table = db::databaseManager.getDatabase().controllerTable;
	
	data::IdType id;
	buffer.loadObjectAt(1, id);
	
	table.accessSafeWrite(id, [this, &id](auto record){
		if(!record)
		{
			log("Invalid id");
			return;
		}
		
		// TODO authorization
		
		buffer.loadBytesAt(1 + sizeof(id), (char*)&record->rotation, sizeof(record->rotation));
		log("Updated state of ", id, " to ", (int)record->rotation.X , ' ', (int)record->rotation.Y , ' ', (int)record->rotation.Z);
	});
}



}
