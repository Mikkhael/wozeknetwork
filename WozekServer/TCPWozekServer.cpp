#include "TCPWozekServer.hpp"

#include <filesystem>
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


void WozekConnectionHandler::awaitRequest(bool silent)
{
	if(!silent)
		log("Awaiting request");
	
	initBigBuffer();
	
	startTimeoutTimer();
	asio::async_read(socket, asio::buffer(buffer, 1), asyncBranch(
		[this](size_t length)
		{
			cancelTimeoutTimer();
			handleReceivedRequestId(buffer[0]);
		})
	);
}

void WozekConnectionHandler::sendTerminatingBytes(const void* bytes, size_t length)
{
	log("Finishing response (length: ", length, ")");
	asio::async_write(socket, asio::buffer(bytes, length), asyncBranch(&WozekConnectionHandler::awaitRequest));
}


void WozekConnectionHandler::handleReceivedRequestId(char id)
{
	if(id == data::HeartbeatCode) // heartbeat
	{
		awaitRequest(true);
		return;
	}
	
	log("Handling request code: ", static_cast<int>(id));
	switch (id)
	{
		case data::RegisterNewHost::Code : // Register as new host
		{
			receiveRegisterHostRequestData();
			break;
		}
		case data::UploadMap::Code : // Host uploading a map file
		{				
			handleUploadMapRequest();
			break;
		}
		case data::DownloadMap::Code : // Host downloading a map file
		{				
			handleDownloadMapRequest();
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
		
		if(type != Type::None)
			res = false;
		
		if(res)
		{
			db::databaseManager.get<db::Database::Table::Host>(newId)->network.tcpConnection = shared_from_this();
			id = newId;
			type = Type::Host;
			
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
	startTimeoutTimer();
	asyncRead(sizeof(data::UploadMap::RequestHeader), [=]
	{
		cancelTimeoutTimer();
		
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
			size_t bigBufferSize = std::min(header.totalMapSize, maxBigBufferSize);
			initBigBuffer(bigBufferSize);
			asyncStrand(fileManager, [=]
			{
				fileManager.deleteMapFile(id);
				log("Deleted old map file");
				asyncWrite(&data::UploadMap::ResponseHeader::AcceptCode, 1, [=]
				{
					receiveSegmentHeader(header.totalMapSize,
					[=](const char* data, size_t length, auto handler)
					{
						size_t bytesToWriteToBigBuffer = std::min(getRemainingBigBufferSize(), length);
						writeToBigBuffer(data, bytesToWriteToBigBuffer);
						
						if(bytesToWriteToBigBuffer < length)
						{
							asyncStrand(fileManager, [=]
							{
								auto path = fileManager.getPathToMapFile(id);
								log("Writing " , bigBufferTop, " bytes into map file: ", path);
								if(!fileManager.appendBufferToFile(path, bigBuffer.data(), bigBufferTop))
								{
									logError("File writing failed");
									return;
								}
								log("Writing complete");
								resetBigBufferTop();
								writeToBigBuffer(data, length - bytesToWriteToBigBuffer);
								asyncPost(handler);
							});
							return;
						}
						asyncPost(handler);
					},
					[=]
					{
						if(bigBufferTop == 0)
						{
							awaitRequest();
							return;
						}
						asyncStrand(fileManager, [=]
						{
							auto path = fileManager.getPathToMapFile(id);
							log("Writing " , bigBufferTop, " bytes into map file: ", path);
							if(!fileManager.appendBufferToFile(path, bigBuffer.data(), bigBufferTop))
							{
								logError("File writing failed");
								return;
							}
							log("Writing complete");
							awaitRequest();
						});
					});
				});
			});
		}
		
	});
}

void WozekConnectionHandler::handleDownloadMapRequest()
{
	log("Handling download map request");
	startTimeoutTimer();
	asyncRead(sizeof(data::DownloadMap::RequestHeader), [=]
	{
		cancelTimeoutTimer();
		data::DownloadMap::RequestHeader header;
		std::memcpy(&header, buffer.data(), sizeof(header));
		
		log("Id: ", header.hostId);
		
		asyncStrand(fileManager, [=]
		{
			auto pathToMapFile = fileManager.getPathToMapFile(header.hostId);
			
			data::DownloadMap::ResponseHeader resHeader;
			if(!fs::is_regular_file(pathToMapFile))
			{
				log("File does not exist.");
				resHeader.code = data::DownloadMap::ResponseHeader::DenyAccessCode;
				sendTerminatingBytes(&resHeader, sizeof(resHeader));
				return;
			}
			
			auto size = fs::file_size(pathToMapFile);
			
			resHeader.code = data::DownloadMap::ResponseHeader::AcceptCode;
			resHeader.totalMapSize = size;
			
			size_t headerSize = sizeof(data::SegmentedTransfer::SegmentHeader);
			initBigBuffer(std::min(size + headerSize, maxBigBufferSize));
			size_t fileSize = bigBuffer.size() - headerSize;
			remainingBytesToSend = size;
			
			if(!fileManager.writeFileToBuffer(pathToMapFile, 0, (char*)bigBuffer.data() + headerSize, fileSize))
			{
				logError("Cannot read the map file");
				return;
			}
			log("Loaded ", fileSize, " bytes from map file: ", pathToMapFile);
			
			asyncWrite(&resHeader, sizeof(resHeader), [=]
			{
				sendSegmentedBuffers(1300, bigBuffer.size(), bigBuffer.data(),
				[=](auto continuation)
				{
					log("Reloading file buffer. Offset: ", size - remainingBytesToSend);
					asyncStrand(fileManager, [=]
					{
						auto newBufferSize = std::min(fileSize, remainingBytesToSend);
						if(!fileManager.writeFileToBuffer(pathToMapFile, size - remainingBytesToSend, (char*)bigBuffer.data() + headerSize, newBufferSize))
						{
							logError("Cannot read the map file");
							return;
						}
						log("Loaded ", newBufferSize, " bytes from map file: ", pathToMapFile);
						asyncPost([=]{continuation(newBufferSize + headerSize);});
					});
				},
				[=](size_t length, auto continuation)
				{
					if(length == 0)
					{
						logError("Fatal internal error. Tried to send 0-length header");
						return;
					}
					//log("ACK: ", length, "  ", remainingBytesToSend);
					remainingBytesToSend -= length;
					startTimeoutTimer();
					asyncRead(sizeof(data::SegmentedTransfer::SegmentAck), [=]
					{
						cancelTimeoutTimer();
						data::SegmentedTransfer::SegmentAck ackHeader;
						std::memcpy(&ackHeader, buffer.data(), sizeof(ackHeader));
						if(remainingBytesToSend == 0)
						{
							if(ackHeader.code == data::SegmentedTransfer::FinishedCode)
							{
								log("Transfer completed");
								awaitRequest();
								return;
							}
						}
						else if(ackHeader.code == data::SegmentedTransfer::ContinueCode)
						{
							asyncPost(continuation);
							return;
						}
						logError("during file transmition");
					});
				});
			});
		});
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

