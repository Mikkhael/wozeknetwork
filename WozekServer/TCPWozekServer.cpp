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
	
	resetState();
	
	
	asyncReadToMainBuffer(1, asyncBranch(
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
	
	asyncReadObject<db::Host::Header>(asyncBranch(
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
	data::RegisterNewHost::Response header;
	header.code = data::RegisterNewHost::Response::Success;
	header.id = id;
	sendTerminatingMessageObject(header);
}

void WozekConnectionHandler::registerNewHostRequestFailed()
{
	log("New Host Registration failed");
	data::RegisterNewHost::Response header;
	header.code = data::RegisterNewHost::Response::Failure;
	sendTerminatingMessageObject(header);
}


/// Upload Map ///

void WozekConnectionHandler::handleUploadMapRequest()
{
	log("Handling Upload Map Request");
	asyncReadObject<data::UploadMap::RequestHeader>(asyncBranch(
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
	using State = States::FileTransferReceiveThreadsafe;
	State* state = setState<State>();
	
	const size_t maxBigBufferSize = 4 * 1024 * 1024;
	state->bigBuffer.resize(std::min(reqHeader.totalMapSize, maxBigBufferSize) );	
	state->path = fileManager.getPathToMapFile(reqHeader.hostId);
	fileManager.deleteMapFile(reqHeader.hostId);
	
	log("Initiating map upload | ", reqHeader.totalMapSize, " bytes");
	
	auto flushBufferToFile = [=](auto callback)
	{
		if(state->bigBufferTop == 0)
		{
			callback();
			return;
		}
		
		asyncPost(fileManager.getStrand(), [=]{
			if(!fileManager.appendBufferToFile(state->path, state->bigBuffer.data(), state->bigBufferTop))
			{
				logError("Cannot write to file ", state->path);
				return;
			}
			state->bigBufferTop = 0;
			asyncPost(callback);
		});
	};
	
	segFileTransfer::readFile(reqHeader.totalMapSize,
		[=](auto callback){
			asyncReadToMainBuffer(sizeof(data::SegmentedTransfer::SegmentHeader), asyncBranch([=]{
				callback(buffer.data());
			}));
		},
		[=](const size_t remainingSegmentSize, auto callback){
			const size_t remianingBufferSize = state->getRemainingBigBufferSize();
			if(remainingSegmentSize < remianingBufferSize)
			{
				asyncReadToMemory(state->getBigBufferEnd(), remainingSegmentSize, asyncBranch([=]{
					state->advance(remainingSegmentSize);
					callback(remainingSegmentSize);
				}));
				return;
			}
			asyncReadToMemory(state->getBigBufferEnd(), remianingBufferSize, asyncBranch([=]{
				state->advance(remianingBufferSize);
				flushBufferToFile([=] { callback(remianingBufferSize); } );
			}));
		},
		[=](const data::SegmentedTransfer::SegmentHeader& header, auto callback){
			if(header.size + state->totalBytesRead > reqHeader.totalMapSize)
			{
				logError("Total segment sizes exceed file size");
				return;
			}
			callback();
		},
		[=](auto callback){
			if(state->totalBytesRead == reqHeader.totalMapSize)
			{
				flushBufferToFile([=]{
					log("File transfer completed (", reqHeader.totalMapSize, " bytes received)");
					data::SegmentedTransfer::SegmentAck ackHeader;
					ackHeader.code = data::SegmentedTransfer::FinishedCode;
					asyncWriteObject(ackHeader, asyncBranch([=]
						{ finalizeUploadMap(reqHeader); }));
				});
				return;
			}
			
			if(!silent)
			{
				const unsigned int newProgress = 100.f * float(state->totalBytesRead) / reqHeader.totalMapSize;
				if(newProgress > state->progress)
				{
					log(state->totalBytesRead, " / ", reqHeader.totalMapSize, " bytes received (", newProgress, "%)");
					state->progress = newProgress;
				}
			}
			
			data::SegmentedTransfer::SegmentAck ackHeader;
			ackHeader.code = data::SegmentedTransfer::ContinueCode;
			asyncWriteObject(ackHeader, asyncBranch(callback));
		});
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

