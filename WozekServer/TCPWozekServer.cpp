#include "TCPWozekServer.hpp"

#include <string_view>
#include <algorithm>

namespace tcp
{
	
/// Basic ///

void WozekConnectionHandler::operator()()
{
	log("New connection");
	remoteEndpoint = socket.remote_endpoint();
	
	startTimeoutTimer();
	awaitRequest();
}


void WozekConnectionHandler::awaitRequest()
{
	log("Awaiting request");
	asio::async_read(socket, asio::buffer(buffer, 1), asyncBranch(
		[this](size_t length)
		{
			refreshTimeout();
			handleReceivedRequestId(buffer[0]);
		})
	);
}

void WozekConnectionHandler::sendTerminatingBytes(const char* bytes, size_t length)
{
	log("Finishing response with: ", std::string_view(bytes, std::min(length, size_t(20))), length > 20 ? "..." : "");
	asio::async_write(socket, asio::buffer(bytes, length), asyncBranch(awaitRequest));
}


void WozekConnectionHandler::handleReceivedRequestId(char id)
{
	log("Handling request id: ", std::ios::hex, static_cast<int>(id));
	switch (id)
	{
		case data::HeartbeatCode : // heartbeat
		{
			awaitRequest();
			break;
		}
		case data::RegisterNewHost::Code : // Register as new host
		{
			if(type != Type::None) // If connection already has a type, disallow registration
				return;
				
			receiveRegisterHostRequestData();
			break;
		}
		default:
		{
			log("Id not recognized");
			// if not recognized, close connection
		}
	}
}


/// Host ///

void WozekConnectionHandler::receiveRegisterHostRequestData()
{
	log("Receiving Host Register Data");
	asio::async_read(socket, asio::buffer(buffer, sizeof(db::Host::Header)), asyncBranch(
		[this](size_t length)
		{
			refreshTimeout();
			
			buffer[length - 1] = '\0'; // Profilacticly, set last character to null
			
			db::Host::Header header;
			std::memcpy(&header, buffer.data(), length);
			tryRegisteringNewHost(header);
		}
	));
}

void WozekConnectionHandler::tryRegisteringNewHost(db::Host::Header& header)
{
	log("Trying to register host: ", header.id, " , " ,header.name);
	db::IdType id;
	bool res;
	if(header.id == 0)
	{
		id = db::databaseManager.createAndAddRecord<db::Database::Table::Host>(header);
		res = id != 0;
	}
	else
	{
		res = db::databaseManager.addRecord<db::Database::Table::Host>(header);
	}
	if(res)
	{
		db::databaseManager.get<db::Database::Table::Host>(id)->network.tcpConnection = shared_from_this();
		buffer[0] = data::RegisterNewHost::Success;
		std::memcpy(buffer.data() + 1, &id, sizeof(id));
		log("Success registering host");
		sendTerminatingBytes(1 + sizeof(id));
	}
	else
	{
		log("Failure registering host");
		buffer[0] = data::RegisterNewHost::Failure;
		sendTerminatingBytes(1);
	}
	
	
}




/// Other ///

void WozekConnectionHandler::timeoutHandler()
{
	log("Socket timed out");
}

void WozekConnectionHandler::shutdownHandler()
{
	log("Shutting down...");
}
	
}

