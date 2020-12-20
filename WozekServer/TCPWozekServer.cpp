#include "TCPWozekServer.hpp"

#include <filesystem>
#include <string_view>
#include <algorithm>


namespace tcp
{
	
/// Basic ///

void WozekSession::awaitRequest(bool silent)
{
	if(!silent)
		log("Awaiting request");
	
	resetState();
	
	asyncReadObjects<char>(
		&WozekSession::handleReceivedRequestId,
		&WozekSession::errorDisconnect
	);
}


void WozekSession::handleReceivedRequestId(char id)
{
	if(id == data::HeartbeatCode) // heartbeat
	{
		awaitRequest(true);
		return;
	}
	
	log("Handling request code: ", static_cast<int>(id));
	switch (id)
	{
		case data::EchoRequest::request_id:
		{
			receiveEchoRequest();
			break;
		}
		/*
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
		case data::FileTransfer::Upload::Code :
		{
			handleUploadFile();
			break;
		}
		case data::FileTransfer::Download::Code :
		{
			handleDownloadFile();
			break;
		}
		*/
		default:
		{
			logError(Logger::Error::TcpInvalidRequests, "Request code not recognized");
			shutdownSession(); // if not recognized, close connection
		}
	}
}

/// Echo ///

void WozekSession::receiveEchoRequest()
{
	this->setState<States::EchoMessageBuffer>();
	log("Receiving Echo Request");
	receiveEchoRequestMessagePart();
}
void WozekSession::receiveEchoRequestMessagePart()
{	
	asyncReadObjects<char>(
		&WozekSession::handleEchoRequestMessagePart,
		&WozekSession::errorAbort
	);
}
void WozekSession::handleEchoRequestMessagePart(const char c)
{
	if(c == '\0')
	{
		sendEchoResponse();
		return;
	}
	auto& state = getState<States::EchoMessageBuffer>();
	if(state.buffer.size() >= MaxEchoRequestMessageLength)
	{
		abortEchoRequest();
		return;
	}
	state.buffer += c;
	receiveEchoRequestMessagePart();
}
void WozekSession::abortEchoRequest()
{
	logError(Logger::Error::TcpEchoTooLong, "Received echo message was too long (over ", MaxEchoRequestMessageLength, " characters)");
	shutdownSession();
}

void WozekSession::sendEchoResponse()
{
	auto& state = getState<States::EchoMessageBuffer>();
	log("Echoing message with length: ", state.buffer.size(), " \"", state.buffer, "\"");
	asyncWrite(
		asio::buffer(state.buffer.c_str(), state.buffer.size() + 1),
		&WozekSession::finilizeRequest,
		&WozekSession::errorAbort
	);
}




/// File ///

void WozekSession::startSegmentedFileReceive(const fs::path path, const size_t totalSize)
{
	log("Starting Segmented File Receive. ", path, " (", totalSize, " bytes)");
	auto& state = setState<States::SegmentedFileTransfer>();
	state.bigBuffer.resize(BigBUfferDefaultSize);
	state.bytesRemaining = totalSize;
	state.fileStream.emplace(getContext(), path, std::ios::trunc | std::ios::out);
	receiveSegmentFileHeader();
}


void WozekSession::receiveSegmentFileHeader()
{
	asyncReadObjects<data::SegmentedFileTransfer::Header>(
			&WozekSession::handleSegmentFileHeader,
			&WozekSession::errorCritical
		);
}

void WozekSession::handleSegmentFileHeader(const data::SegmentedFileTransfer::Header header)
{
	if(header.startHeader != data::SegmentedFileTransfer::Header::correctStartHeader)
	{
		logError(Logger::Error::TcpSegFileTransferError, "Received Segment Header has invalid code");
		sendSegmentFileError(data::SegmentedFileTransfer::SegmentTooLong);
		return;
	}
	
	auto& state = setState<States::SegmentedFileTransfer>();
	state.fileSegmentLengthLeft = header.segmentLength;
	
	if(state.bytesRemaining < header.segmentLength) // invalid segment length
	{
		logError(Logger::Error::TcpSegFileTransferError, "Received Segment Header is too long");
		sendSegmentFileError(data::SegmentedFileTransfer::SegmentTooLong);
		return;
	}
	
	asyncWriteObjects(
		[&]{ receiveSegmentFileData(state.fileSegmentLengthLeft); },
		&WozekSession::errorCritical,
		char(data::SegmentedFileTransfer::Error::Good)
	);
}

void WozekSession::receiveSegmentFileData(const size_t length)
{
	auto& state = setState<States::SegmentedFileTransfer>();
	auto toReceive = std::min(length, state.bigBuffer.size() - state.bufferFilled);
	asyncRead(
		asio::buffer(state.bigBuffer.data() + state.bufferFilled, toReceive),
		[=](){ handleSegmentFileData(toReceive); },
		&WozekSession::errorCritical
	);
}

void WozekSession::handleSegmentFileData(const size_t length)
{
	auto& state = setState<States::SegmentedFileTransfer>();
	
	state.bufferFilled += length;
	state.fileSegmentLengthLeft -= length;
	state.bytesRemaining -= length;
	// TODO file multitasking
	
	if(!state.internalFileError && state.bufferFilled == state.bigBuffer.size())
	{
		state.fileStream->stream.write(state.bigBuffer.data(), state.bigBuffer.size());
		if(state.fileStream->stream.fail())
		{
			state.internalFileError = true;
			logError(Logger::Error::FileSystemError, "File error while receiving segmented file. Awaiting completion of the segment.");
		}
		state.bufferFilled = 0;
	}
	
	if(state.fileSegmentLengthLeft > 0)
	{
		receiveSegmentFileData(state.fileSegmentLengthLeft);
		return;
	}
	
	if(state.internalFileError)
	{
		sendSegmentFileError(data::SegmentedFileTransfer::FileSystem);
		return;
	}
	
	if(state.bytesRemaining == 0)
	{
		if(state.bufferFilled > 0)
		{
			state.fileStream->stream.write(state.bigBuffer.data(), state.bigBuffer.size());
			if(state.fileStream->stream.fail())
			{
				logError(Logger::Error::FileSystemError, "File error while receiving last segment.");				
				sendSegmentFileError(data::SegmentedFileTransfer::FileSystem);
				return;
			}
		}
		
		finalizeSegmentFileReceive();
		
		return;
	}
	
	receiveSegmentFileHeader();
}

void WozekSession::finalizeSegmentFileReceive()
{
	returnCallbackGood();
}

void WozekSession::sendSegmentFileError(const data::SegmentedFileTransfer::Error error)
{
	asyncWriteObjects(
		&WozekSession::returnCallbackGood,
		&WozekSession::errorCritical,
		char(error)
	);
}

/// Host ///

/*

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
			asyncWriteObject(resHeader, asyncBranch(&WozekConnectionHandler::awaitRequest));
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

/// Files ///

void WozekConnectionHandler::handleUploadFile()
{
	log("Handling Upload File Request");
	asyncReadObject<data::FileTransfer::Upload::Request>(asyncBranch(
	[=](auto& reqHeader)
	{
		data::FileTransfer::Upload::Response resHeader;
		if(reqHeader.fileSize <= 0)
		{
			log("Invalid file size specified");
			resHeader.code = data::FileTransfer::Upload::Response::InvalidSizeCode;
			sendTerminatingMessageObject(resHeader);
			return;
		}
		size_t fileNameLength = strlen(reqHeader.fileName);
		for(size_t i=0; i < fileNameLength; i++)
		{
			char c = reqHeader.fileName[i];
			if(!( (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c == '.' || c == '_') ))
			{
				log("Invalid file name specified. ", "character on position ", i, ": \"", c, "\" (", static_cast<int>(c), ")");
				resHeader.code = data::FileTransfer::Upload::Response::InvalidNameCode;
				sendTerminatingMessageObject(resHeader);
				return;
			}
		}
		
		// Request accepted
		
		fs::path path = fileManager.getPathToFile(reqHeader.fileName);
		fileManager.deleteFile(path);
		
		resHeader.code = data::FileTransfer::Upload::Response::AcceptCode;
		asyncWriteObject(resHeader, asyncBranch([=]{ initiateFileTransferReceive(reqHeader.fileSize, path, [=](bool success){
			if(success)
			{
				log("File uploaded successfully: size: ", reqHeader.fileSize, ", path:", path);
				awaitRequest();
			}
		}); }));
	}));
}

void WozekConnectionHandler::handleDownloadFile()
{
	log("Handling Download File Request");
	asyncReadObject<data::FileTransfer::Download::Request>(asyncBranch(
	[=](auto& reqHeader)
	{
		data::FileTransfer::Download::Response resHeader;
		fs::path path = fileManager.getPathToFile(reqHeader.fileName);
		size_t fileSize = fileManager.getFileSize(path);
		
		if(fileSize == 0)
		{
			log("File ", path, " dosent exist");
			resHeader.code = data::FileTransfer::Download::Response::FileNotFoundCode;
			sendTerminatingMessageObject(resHeader);
			return;
		}
		
		// Request accepted
		
		resHeader.code = data::FileTransfer::Download::Response::AcceptCode;
		resHeader.fileSize = fileSize;
		asyncWriteObject(resHeader, asyncBranch([=]{ initiateFileTransferSend(path, [=](bool success){
			if(success)
			{
				log("File downloaded successfully: size: ", fileSize, ", path:", path);
				awaitRequest();
			}
		}); }));
	}));
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
*/
}

