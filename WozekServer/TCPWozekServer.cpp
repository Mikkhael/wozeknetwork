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
	
	
	asyncTimeoutReadToMainBuffer(1, asyncBranch(
		[=]{handleReceivedRequestId(buffer[0]);},
		&WozekConnectionHandler::logAndAbortErrorHandlerOrDisconnect
	));
}

void WozekConnectionHandler::sendTerminatingMessage(const char* bytes, size_t length)
{
	asyncWriteFromMemory(bytes, length, asyncBranch(&WozekConnectionHandler::awaitRequest));
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
			handleRegisterHostRequest();
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
			logError("Request code not recognized");
			// if not recognized, close connection
		}
	}
}


/// Host ///

void WozekConnectionHandler::handleRegisterHostRequest()
{
	log("Handling Register Host Request");
	
	asyncTimeoutReadObject<db::Host::Header>(asyncBranch(
		[=](auto& header)
		{
			// Profilacticy, set the las character of name to NULL
			header.name[sizeof(header.name) - 1] = '\0';
			
			tryRegisteringNewHost(header);
		}
	));
}

void WozekConnectionHandler::tryRegisteringNewHost(db::Host::Header& header)
{
	if(type != WozekConnectionHandler::Type::None)
	{
		registerNewHostRequestFailed();
		return;
	}
	
	asyncPost(db::databaseManager.getStrand(), [=]
	{
		db::IdType newId = header.id;
		if(header.id == 0)
		{
			newId = db::databaseManager.createAndAddRecord<db::Database::Table::Host>(header);
			if(newId == 0)
			{
				registerNewHostRequestFailed();
				return;
			}
		}
		else if(!db::databaseManager.addRecord<db::Database::Table::Host>(header))
		{
			registerNewHostRequestFailed();
			return;
		}
		
		// Success
		
		type = WozekConnectionHandler::Type::Host;
		id = newId;
		db::databaseManager.get<db::Database::Table::Host>(newId)->network.tcpConnection = shared_from_this();
		
		registerNewHostRequestSuccess();
	});
}
void WozekConnectionHandler::registerNewHostRequestSuccess()
{
	log("New Host Registration completed");
	sendTerminatingMessageObject(data::RegisterNewHost::Response::make(data::RegisterNewHost::Response::Success, id));
}

void WozekConnectionHandler::registerNewHostRequestFailed()
{
	log("New Host Registration failed");
	sendTerminatingMessageObject(data::RegisterNewHost::Response::make(data::RegisterNewHost::Response::Failure, 0));
}


/// Upload Map ///

void WozekConnectionHandler::handleUploadMapRequest()
{
	log("Handling Upload Map Request");
	asyncTimeoutReadObject<data::UploadMap::RequestHeader>(asyncBranch(
	[=](auto& reqHeader)
	{
		data::UploadMap::ResponseHeader resHeader;
		if(reqHeader.totalMapSize <= 0)
		{
			log("Invalid file size specified");
			resHeader.code = data::UploadMap::ResponseHeader::InvalidSizeCode;
			sendTerminatingMessageObject(resHeader);
			return;
		}
		if(type != Type::Host || id != reqHeader.hostId)
		{
			log("Access denied");
			resHeader.code = data::UploadMap::ResponseHeader::DenyAccessCode;
			sendTerminatingMessageObject(resHeader);
			return;
		}
		
		// Request accepted
		resHeader.code = data::UploadMap::ResponseHeader::AcceptCode;
		asyncWriteObject(resHeader, asyncBranch([=]{ initiateMapFileUpload(reqHeader); }));
	}));
}

void WozekConnectionHandler::initiateMapFileUpload(data::UploadMap::RequestHeader reqHeader, bool silent)
{
	fs::path path = fileManager.getPathToMapFile(reqHeader.hostId);
	
	initBigBuffer( std::min(maxBigBufferSize, reqHeader.totalMapSize) );
	
	auto flushBuffer = [path, this](auto callback)
	{
		if(bigBufferTop == 0)
		{
			callback();
			return;
		}
		
		asyncPost(fileManager.getStrand(), [=]
		{
			if(!fileManager.appendBufferToFile(path, bigBuffer.data(), bigBufferTop))
			{
				logError("Unknown error while writting to file ", path);
				return;
			}
			bigBufferTop = 0;
			asyncPost(callback);
		});
	};
	
	
	segFileTransfer::readFile(reqHeader.totalMapSize, 
		[=](const bool header, const size_t length, auto callback)
		{
			if(header)
			{
				asyncTimeoutReadToMainBuffer(length, asyncBranch([=](const size_t length){ callback(buffer.data(), length); }));
				return;
			}
			const size_t bytesToRead = std::min(getRemainingBigBufferSize(), length);
			asyncTimeoutReadToMemory(bigBuffer.data() + bigBufferTop, bytesToRead, asyncBranch(
			[=](const size_t length)
			{
				const auto bytesTemp = bigBuffer.data() + bigBufferTop;
				bigBufferTop += length;
				callback(bytesTemp, length);
			}));
		},
		[=](const char* receivedBytes, const size_t receivedBytesLength, auto callback)
		{
			if(bigBufferTop == bigBuffer.size())
			{
				flushBuffer(callback);
				return;
			}
			callback();
		},
		[=](const size_t remainingFileSize, const size_t readSegmentSize, auto callback)
		{
			if(remainingFileSize == readSegmentSize)
			{
				flushBuffer([=]
				{
					if(!silent)
						log("File transfer completed");
					asyncWriteObject(data::SegmentedTransfer::SegmentAck::make(data::SegmentedTransfer::FinishedCode), asyncBranch([=] {finalizeUploadMap(reqHeader);} ));
				});
				return;
			}
			asyncWriteObject(data::SegmentedTransfer::SegmentAck::make(data::SegmentedTransfer::ContinueCode), asyncBranch(callback) );
		},
		[=](const data::SegmentedTransfer::SegmentHeader& header, const size_t remainingFileSize, auto callback)
		{
			if(header.size > remainingFileSize)
			{
				log("Total segments sizes exceeded total file size. Canceling transfer.");
				sendTerminatingMessageObject(data::SegmentedTransfer::SegmentAck::make(data::SegmentedTransfer::ErrorCode));
				return;
			}
			
			// Logging
			if(!silent)
			{
				int percent1 = 100.f * float(remainingFileSize) / float(reqHeader.totalMapSize);
				int percent2 = 100.f * float(remainingFileSize - header.size) / float(reqHeader.totalMapSize);
				if(percent2 != percent1)
				{
					log("(", (100 - percent1) , "/100) ", (reqHeader.totalMapSize - remainingFileSize), " bytes transfered in total" );
				}
			}
			
			callback();
		}
	);
	
}

void WozekConnectionHandler::finalizeUploadMap(data::UploadMap::RequestHeader header)
{
	log("Map upload with id", header.hostId, " and size ", header.totalMapSize, ", completed successfully");
	awaitRequest();
}


void WozekConnectionHandler::handleDownloadMapRequest()
{
	
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

