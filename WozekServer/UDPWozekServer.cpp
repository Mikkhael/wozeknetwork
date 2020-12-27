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
	
	table.postOnStrand([this, id, &table, me = sharedFromThis()]{
		auto res = table.getRecordById(id);
		if(!res)
		{
			log("Id ", id, " not found.");
			return;
		}
		if(res->endpoint.address() != remoteEndpoint.address())
		{
			log("Invalid endpoint. Expected: ", res->endpoint, ", received from: ", remoteEndpoint);
			return;
		}
		
		static_assert(sizeof(res->rotation) == sizeof(data::UdpFetchState::Response));
		
		buffer.saveObjectAt(1, res->rotation);
		asyncWriteTo(
			buffer.get(1 + sizeof(res->rotation)),
			remoteEndpoint,
			[]{},
			&WozekUDPReceiver::errorAbort
		);
	});
	
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
	table.postOnStrand([&table, me = sharedFromThis()]{
		data::IdType id;
		me->buffer.loadObjectAt(1, id);
		auto rec = table.getRecordById(id);
		if(!rec)
		{
			me->log("Invalid id");
			return;
		}
		// TODO authorization
		me->buffer.loadBytesAt(1 + sizeof(id), (char*)&rec->rotation, sizeof(rec->rotation));
		me->log("Updated state of ", id, " to ", (int)rec->rotation.X , ' ', (int)rec->rotation.Y , ' ', (int)rec->rotation.Z);
	});
}



}
