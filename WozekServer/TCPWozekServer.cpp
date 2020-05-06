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
	logger.log(Logger::Log::TcpActiveConnections);
	logger.log(Logger::Log::TcpTotalConnections);
	
	remoteEndpoint = socket.remote_endpoint();
	
	//debugHandler();
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
		case data::StartTheWorld::Code : // Host starting the world 
		{
			handleStartTheWorld();
			break;
		}
		default:
		{
			logError(Logger::Error::TcpInvalidRequests, "Request code not recognized");
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
		
		fs::path path = fileManager.getPathToMapFile(reqHeader.hostId);
		fileManager.deleteMapFile(reqHeader.hostId);
		
		resHeader.code = data::UploadMap::ResponseHeader::AcceptCode;
		asyncWriteObject(resHeader, asyncBranch([=]{ initiateFileTransferReceive(reqHeader.totalMapSize, path, [=](bool success){
			if(success)
				finalizeUploadMap(reqHeader);
		}); }));
	}));
}


void WozekConnectionHandler::finalizeUploadMap(data::UploadMap::RequestHeader header)
{
	log("Map upload with id", header.hostId, " and size ", header.totalMapSize, ", completed successfully");
	awaitRequest();
}


void WozekConnectionHandler::handleDownloadMapRequest()
{
	log("Handling Download Map Request");
	asyncReadObject<data::DownloadMap::RequestHeader>(asyncBranch(
	[=](auto& reqHeader)
	{
		data::DownloadMap::ResponseHeader resHeader;
		
		fs::path path = fileManager.getPathToMapFile(reqHeader.hostId);
		if(!fs::is_regular_file(path))
		{
			log("Map with id ", reqHeader.hostId, " dosen't exist");
			resHeader.code = data::DownloadMap::ResponseHeader::DenyAccessCode;
			asyncWriteObject(resHeader, asyncBranch(awaitRequest));
			return;
		}
		
		// Request accepted
		
		resHeader.code = data::DownloadMap::ResponseHeader::AcceptCode;
		resHeader.totalMapSize = fs::file_size(path);
		
		log("Sending map with id ", reqHeader.hostId, " and size of ", resHeader.totalMapSize, " bytes");
		
		asyncWriteObject(resHeader, asyncBranch([=]{ initiateFileTransferSend(path, [=](bool success){
			if(success)
				finalizeDownloadMap(reqHeader);
		}); }));
	}));
}
void WozekConnectionHandler::finalizeDownloadMap(data::DownloadMap::RequestHeader header)
{
	log("Map downloaded with id", header.hostId, " completed successfully");
	awaitRequest();
}


/// Start the World ///

void WozekConnectionHandler::handleStartTheWorld()
{
	static auto failRoutine = [this]{
		data::StartTheWorld::ResponseHeader resHeader;
		resHeader.code = data::StartTheWorld::ResponseHeader::Failure;
		sendTerminatingMessageObject(resHeader);
	};
	
	log("Handling Start The World Request");
	asyncReadObject<data::StartTheWorld::RequestHeader>(asyncBranch(
	[=](auto& reqHeader)
	{
		if(id == 0 || type != Type::Host || reqHeader.port == 0)
		{
			
			log("Unable to start world on current host");
			failRoutine();
			return;
		}
		
		asyncPost(db::databaseManager.getStrand(), [=]{
			auto worldId = db::databaseManager.createAndAddRecord<db::Database::Table::World>();
			if(worldId == 0)
			{
				logError("Failure while creating a new world in the database");
				failRoutine();
				return;
			}
			
			auto& world = *db::databaseManager.get<db::Database::Table::World>(worldId);
			world.createStrand(getContext());
			world.setMainHost(id, {getRemote().address(), reqHeader.port});
			
			
			log("World started successfuly. Id: ", worldId);
			data::StartTheWorld::ResponseHeader resHeader;
			resHeader.code = data::StartTheWorld::ResponseHeader::Success;
			resHeader.worldId = worldId;
			sendTerminatingMessageObject(resHeader);
		});
		
	}));
}



/// File Transfer ///

void WozekConnectionHandler::initiateFileTransferReceive(const size_t totalSize, fs::path path, std::function<void(bool)> mainCallback,  bool silent)
{
	using State = States::FileTransferReceiveThreadsafe;
	State* state = setState<State>();
	
	const size_t maxBigBufferSize = 4 * 1024 * 1024;
	state->bigBuffer.resize(std::min(totalSize, maxBigBufferSize) );	
	state->path = path;
	
	log("Initiating file sending | ", totalSize, " bytes | ", path);
	
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
				logError(Logger::Error::FileSystemError, "Cannot write to file ", state->path);
				mainCallback(false);
				return;
			}
			state->bigBufferTop = 0;
			asyncPost(callback);
		});
	};
	
	segFileTransfer::readFile(totalSize,
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
			if(header.size + state->totalBytesRead > totalSize)
			{
				logError(Logger::Error::TcpInvalidRequests, "Total segment sizes exceed file size");
				mainCallback(false);
				return;
			}
			callback();
		},
		[=](auto callback){
			if(state->totalBytesRead == totalSize)
			{
				flushBufferToFile([=]{
					log("File transfer completed (", totalSize, " bytes received)");
					data::SegmentedTransfer::SegmentAck ackHeader;
					ackHeader.code = data::SegmentedTransfer::FinishedCode;
					asyncWriteObject(ackHeader, asyncBranch([=]
						{ mainCallback(true);}));
				});
				return;
			}
			
			if(!silent)
			{
				const unsigned int newProgress = 100.f * float(state->totalBytesRead) / totalSize;
				if(newProgress > state->progress)
				{
					log(state->totalBytesRead, " / ", totalSize, " bytes received (", newProgress, "%)");
					state->progress = newProgress;
				}
			}
			
			data::SegmentedTransfer::SegmentAck ackHeader;
			ackHeader.code = data::SegmentedTransfer::ContinueCode;
			asyncWriteObject(ackHeader, asyncBranch(callback));
		});
}

void WozekConnectionHandler::initiateFileTransferSend(fs::path path, std::function<void(bool)> mainCallback, bool silent)
{
	using State = States::FileTransferSendThreadsafe;
	State* state = setState<State>();
	
	const size_t totalSize = fs::file_size(path);
	const size_t maxBigBufferSize = 4 * 1024 * 1024;
	
	state->path = path;
	state->bigBuffer.resize(std::min(totalSize, maxBigBufferSize) );
	state->bigBufferTop = state->bigBuffer.size();
	state->bufferSequence[0] = asio::buffer(buffer, sizeof(data::SegmentedTransfer::SegmentHeader));
	
	log("Initiating file sending | ", totalSize, " bytes | ", path);
	
	auto tryReloadBuffer = [=](auto callback)
	{
		if(state->getRemainingBigBufferSize() != 0)
		{
			callback();
			return;
		}
		
		asyncPost(fileManager.getStrand(), [=]{
			const size_t length = std::min(state->bigBuffer.size(), totalSize - state->totalBytesSent);
			if(!fileManager.writeFileToBuffer(state->path, state->totalBytesSent, state->bigBuffer.data(), length))
			{
				logError(Logger::Error::FileSystemError, "Cannot read from file ", state->path);
				mainCallback(false);
				return;
			}
			state->bigBufferTop = 0;
			asyncPost(callback);
		});
	};
	
	segFileTransfer::sendFile(totalSize, 1300,
		[=](auto& header, auto callback){
			//std::cout << "Header: " << header.size << std::endl;
			saveObjectToMainBuffer(header);
			callback();
		},
		[=](const bool withHeader, const size_t length, auto callback){
			
			tryReloadBuffer([=]{
				
				assert( length > 0 );
				assert( state->totalBytesSent + length <= totalSize );
				
				const size_t bytesToSend = std::min(length, state->getRemainingBigBufferSize());
				state->bufferSequence[1] = asio::buffer(state->getBigBufferEnd(), bytesToSend);
				state->advance(bytesToSend);
				
				if(withHeader)
					asyncWriteFromBuffer(state->bufferSequence, asyncBranch([=]{
						callback(bytesToSend);
					}));
				else
					asyncWriteFromBuffer(state->bufferSequence[1], asyncBranch([=]{
						callback(bytesToSend);
					}));
			});
		},
		[=](auto callback){
			asyncReadObject<data::SegmentedTransfer::SegmentAck>(asyncBranch([=](auto& ackHeader){
				//std::cout << "Ack: " << ackHeader.code << std::endl;
				if(ackHeader.code == data::SegmentedTransfer::ErrorCode)
				{
					logError(Logger::Error::TcpSegFileTransferError, "Unknown error during file transimison");
					mainCallback(false);
					return;
				}
				else if(state->totalBytesSent == totalSize && ackHeader.code == data::SegmentedTransfer::FinishedCode)
				{
					log("File transfer completed (", totalSize, " bytes sent)");
					mainCallback(true);
					return;
				}
				else if(ackHeader.code == data::SegmentedTransfer::ContinueCode)
				{
					if(!silent){
						const unsigned int newProgress = 100.f * float(state->totalBytesSent) / totalSize;
						if(newProgress > state->progress){
							log(state->totalBytesSent, " / ", totalSize, " bytes sent (", newProgress, "%)");
							state->progress = newProgress;
						}
					}
					callback();
					return;
				}
				logError(Logger::Error::TcpSegFileTransferError, "Unexpected ack code received");
				mainCallback(false);
			}));
		});
}




/// Other ///

void WozekConnectionHandler::timeoutHandler()
{
	logError(Logger::Error::TcpTimeout, "Socket timed out");
}

void WozekConnectionHandler::shutdownHandler()
{
	log("Shutting down...");
	logger.log(Logger::Log::TcpActiveConnections, -1);
}
	
}

