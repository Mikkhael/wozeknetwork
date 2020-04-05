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
	
	awaitRequest();
}


void WozekConnectionHandler::awaitRequest()
{
	log("Awaiting request");
	startTimeoutTimer();
	asio::async_read(socket, asio::buffer(buffer, 1), asyncBranch(
		[this](size_t length)
		{
			cancelTimeoutTimer();
			handleReceivedRequestId(buffer[0]);
		})
	);
}

void WozekConnectionHandler::sendTerminatingBytes(const char* bytes, size_t length)
{
	log("Finishing response (length: ", length, ")");
	asio::async_write(socket, asio::buffer(bytes, length), asyncBranch(&WozekConnectionHandler::awaitRequest));
}


void WozekConnectionHandler::handleReceivedRequestId(char id)
{
	log("Handling request code: ", static_cast<int>(id));
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
		case data::UploadMap::Code : // Host uploading a map file
		{				
			handleUploadMapRequest();
			break;
		}
		default:
		{
			log("Request code not recognized");
			// if not recognized, close connection
		}
	}
}


/// Host ///

void WozekConnectionHandler::receiveRegisterHostRequestData()
{
	log("Receiving Host Register Data");
	startTimeoutTimer();
	asio::async_read(socket, asio::buffer(buffer, sizeof(db::Host::Header)), asyncBranch(
		[this](size_t length)
		{
			cancelTimeoutTimer();
			
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
	asyncStrand(db::databaseManager, [header, this]
	{
		bool res;
		db::IdType newId = header.id;
		if(header.id == 0)
		{
			newId = db::databaseManager.createAndAddRecord<db::Database::Table::Host>(header);
			res = newId != 0;
		}
		else
		{
			res = db::databaseManager.addRecord<db::Database::Table::Host>(header);
		}
		
		if(res)
		{
			db::databaseManager.get<db::Database::Table::Host>(newId)->network.tcpConnection = shared_from_this();
			id = newId;
			
			asyncPost([this]{
				log("Success registering host");
				buffer[0] = data::RegisterNewHost::Success;
				std::memcpy(buffer.data() + 1, &id, sizeof(id));
				sendTerminatingBytes(1 + sizeof(id));
			});
		}
		else
		{
			asyncPost([this, res]{
				log("Failure registering host");
				buffer[0] = data::RegisterNewHost::Failure;
				sendTerminatingBytes(1);
			});
		}		
	});	
}

/// Upload Map ///

void WozekConnectionHandler::handleUploadMapRequest()
{
	log("Handling Upload Map Request");
	asyncRead(sizeof(data::UploadMap::RequestHeader), [=]
	{
		data::UploadMap::RequestHeader header;
		std::memcpy(&header, buffer.data(), sizeof(header));
		
		if(type != Type::Host || header.hostId != id)
		{
			log("Denied access for map upload");
			sendTerminatingBytes(&data::UploadMap::ResponseHeader::DenyAccessCode, 1);
		}
		else if(header.totalMapSize >= size_t(4000000000) || header.totalMapSize <= 0)
		{
			log("Invalid size for upload");
			sendTerminatingBytes(&data::UploadMap::ResponseHeader::InvalidSizeCode, 1);
		}
		else
		{
			log("Preparing for transfer with size: ", header.totalMapSize );
			asyncStrand(fileManager, [=]
			{
				fileManager.deleteMapFile(id);
				log("Deleted old map file");
				asyncWrite(&data::UploadMap::ResponseHeader::AcceptCode, 1, [=]
				{
					receiveSegmentHeader(header.totalMapSize,
					[=](const char* data, size_t length, auto handler)
					{
						log("Writing " , length, " bytes into map file");
						asyncStrand(fileManager, [=]
						{
							fileManager.appendBufferToFile(fileManager.getPathToMapFile(id), data, length);
							log("Writing complete");
							asyncPost(handler);
						});
					},
					[=]
					{
						// Do nothing specyfic
						awaitRequest();
					});
				});
			});
		}
		
	});
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

