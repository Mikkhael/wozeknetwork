#include "tcp.hpp"


namespace tcp
{

void Connection::registerAsNewHost(data::RegisterNewHost::Request reqHeader, Callback requestCallback )
{	
	auto errorHandler = [=](const Error& err)
	{
		logError(err);
		requestCallback(CallbackCode::CriticalError);
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
				requestCallback(CallbackCode::Error);
				return;
			}
			
			id = resHeader.id;
			log("Successfully registerd as host with id: ", id);
			requestCallback(CallbackCode::Success);
			return;
			
		}, errorHandler));
	}, errorHandler));
}

void Connection::uploadMap(fs::path path, Callback requestCallback)
{
	auto errorHandler = [=](const Error& err)
	{
		logError(err);
		requestCallback(CallbackCode::CriticalError);
	};
		
	if(!fs::is_regular_file(path))
	{
		logError("File dosen't exist: ", path);
		requestCallback(CallbackCode::Error);
		return;
	}
	
	data::UploadMap::RequestHeader reqHeader;
	reqHeader.hostId = id;
	reqHeader.totalMapSize = fs::file_size(path);
	
	size_t off = 0;
	off += writeObjectToBuffer(data::UploadMap::Code, off);
	off += writeObjectToBuffer(reqHeader, off);
	asyncWrite(off, ioHandler(
	[=]{
		asyncRead(sizeof(data::UploadMap::ResponseHeader), ioHandler([=]{
			data::UploadMap::ResponseHeader resHeader;
			readObjectFromBuffer(resHeader);
			if(resHeader.code == data::UploadMap::ResponseHeader::DenyAccessCode)
			{
				log("Access denied");
				requestCallback(CallbackCode::Error);
				return;
			}
			if(resHeader.code == data::UploadMap::ResponseHeader::InvalidSizeCode)
			{
				log("Invalid size specified");
				requestCallback(CallbackCode::Error);
				return;
			}
			if(resHeader.code != data::DownloadMap::ResponseHeader::AcceptCode)
			{
				log("Unknown response code received");
				requestCallback(CallbackCode::CriticalError);
				return;
			}
			
			log("Initiating transfer of file ", path, " with size of ", reqHeader.totalMapSize, " bytes.");
			initiateFileUpload(path, 4*1024*1024, 1300, requestCallback);
			
		}, errorHandler));
	}, errorHandler ));
	
}

void Connection::downloadMap(data::IdType id, fs::path path, Callback requestCallback)
{
	auto errorHandler = [=](const Error& err)
	{
		logError(err);
		requestCallback(CallbackCode::CriticalError);
	};
	
	std::ofstream fileTest(path);
	if(!fileTest.is_open())
	{
		logError("Cannot open file: ", path);
		requestCallback(CallbackCode::Error);
		return;
	}
	
	data::DownloadMap::RequestHeader reqHeader;
	reqHeader.hostId = id;
	
	size_t off = 0;
	off += writeObjectToBuffer(data::DownloadMap::Code, off);
	off += writeObjectToBuffer(reqHeader, off);
	asyncWrite(off, ioHandler(
	[=]{
		asyncRead(sizeof(data::DownloadMap::ResponseHeader), ioHandler([=]{
			data::DownloadMap::ResponseHeader resHeader;
			readObjectFromBuffer(resHeader);
			if(resHeader.code == data::DownloadMap::ResponseHeader::DenyAccessCode)
			{
				log("Access denied");
				requestCallback(CallbackCode::Error);
				return;
			}
			if(resHeader.code != data::DownloadMap::ResponseHeader::AcceptCode)
			{
				log("Unknown response code received");
				requestCallback(CallbackCode::CriticalError);
				return;
			}
			
			log("Initiating download of file ", path);
			initiateFileDownload(path, resHeader.totalMapSize, 4*1024*1024, requestCallback);
			
		}, errorHandler));
	}, errorHandler ));
	
}

void Connection::initiateFileDownload(fs::path path, const size_t totalSize, const size_t maxBigBufferSize, Callback requestCallback)
{
	auto errorHandler = [=](const Error& err)
	{
		logError(err);
		requestCallback(CallbackCode::CriticalError);
	};
	
	using State = States::FileTransferReceive;
	State* state = setState<State>();
	
	state->file.open(path, std::ios::binary);
	if(!state->file.is_open())
	{
		logError("Cannot open file: ", path);
		requestCallback(CallbackCode::CriticalError);
		return;
	}
	
	const size_t bigBufferSize = std::min(totalSize, maxBigBufferSize);
	state->bigBuffer.resize(bigBufferSize);
	state->bigBufferTop = 0;
	
	const auto flushBufferToFile = [=]
	{
		if(state->bigBufferTop == 0)
			return false;
		state->file.write(state->bigBuffer.data(), state->bigBufferTop);
		state->bigBufferTop = 0;
		return state->file.fail();
	};
	
	segFileTransfer::readFile(totalSize, 
		[=](auto callback){
			asyncRead(sizeof(data::SegmentedTransfer::SegmentHeader), ioHandler([=]{ callback(buffer.data()); }, errorHandler));
		},
		[=](const size_t remainingSegmentSize, auto callback){
			
			assert( remainingSegmentSize + state->totalBytesReceived <= totalSize);
			
			if(flushBufferToFile()){
				logError("Cannot write to file: ", path);
				requestCallback(CallbackCode::CriticalError);
				return;
			}
			
			const size_t bytesToRead = std::min(remainingSegmentSize, state->getRemainingBigBufferSize());
			asyncRead(asio::buffer(state->getBigBufferEnd(), bytesToRead), ioHandler([=]{
				state->advance(bytesToRead);
				callback(bytesToRead);
			}, errorHandler));
			
		},
		[=](const auto& header, auto callback){
			if(header.size + state->totalBytesReceived > totalSize)
			{
				logError("Total segments size exceed file size");
				requestCallback(CallbackCode::CriticalError);
				return;
			}
			callback();
		},
		[=](auto callback){
			data::SegmentedTransfer::SegmentAck ackHeader;
			if(state->totalBytesReceived == totalSize)
			{
				if(flushBufferToFile()){
					logError("Cannot write to file: ", path);
					requestCallback(CallbackCode::CriticalError);
					return;
				}
				
				log("File transfer completed");
				ackHeader.code = data::SegmentedTransfer::FinishedCode;
				writeObjectToBuffer(ackHeader);
				asyncWrite(sizeof(ackHeader), ioHandler([=]{ requestCallback(CallbackCode::Success); }, errorHandler));
				return;
			}
			const unsigned int currentProgress = 100.f * float(state->totalBytesReceived) / totalSize;
			if(currentProgress > state->progress)
			{
				state->progress = currentProgress;
				log("Received ", state->totalBytesReceived, " / ", totalSize, " bytes (", currentProgress, "%)");
			}
			ackHeader.code = data::SegmentedTransfer::ContinueCode;
			writeObjectToBuffer(ackHeader);
			asyncWrite(sizeof(ackHeader), ioHandler([=]{ callback(); }, errorHandler));
		});
}


void Connection::initiateFileUpload(fs::path path, const size_t maxBigBufferSize, const size_t maxSegmentLength, Callback requestCallback)
{
	auto errorHandler = [=](const Error& err)
	{
		logError(err);
		requestCallback(CallbackCode::CriticalError);
	};
	
	using State = States::FileTransferSend;
	
	State* state = setState<State>();
	
	state->file.open(path, std::ios::binary);
	const size_t totalSize = fs::file_size(path);
	if(!state->file.is_open())
	{
		logError("Cannot open file: ", path);
		requestCallback(CallbackCode::CriticalError);
		return;
	}
	
	state->bufferSequence[0] = asio::buffer(buffer, sizeof(data::SegmentedTransfer::SegmentHeader));
	
	const size_t bigBufferSize = std::min(totalSize, maxBigBufferSize);
	state->bigBuffer.resize(bigBufferSize);
	state->file.read(state->bigBuffer.data(), bigBufferSize);
	if(state->file.fail())
	{
		logError("Cannot read from file: ", path);
		requestCallback(CallbackCode::CriticalError);
		return;
	}
	
	segFileTransfer::sendFile(totalSize, maxSegmentLength,
	[=](const data::SegmentedTransfer::SegmentHeader& header, auto callback)
	{
		writeObjectToBuffer(header);
		callback();
	},
	[=](const bool withHeader, const size_t remainingSegmentLength, auto callback)
	{
		//std::cout << withHeader << " " << remainingSegmentLength << " " << state->totalBytesSent << " / " << totalSize << '\n';
		assert(state->totalBytesSent + remainingSegmentLength <= totalSize);
		
		size_t remainingBuffer = state->getRemainingBigBufferSize();
		if(remainingBuffer == 0)
		{
			const size_t toReadFromFile = std::min(totalSize - state->totalBytesSent, state->bigBuffer.size());
			state->bigBufferTop = 0;
			remainingBuffer = toReadFromFile;
			state->file.read(state->bigBuffer.data(), toReadFromFile);
			if(state->file.fail())
			{
				logError("Cannot read from file: ", path);
				requestCallback(CallbackCode::CriticalError);
				return;
			}
		}
		const size_t lengthToSend = std::min(remainingSegmentLength, remainingBuffer);
		state->bufferSequence[1] = asio::buffer(state->getBigBufferEnd(), lengthToSend);
		//std::cout << "Sending " << lengthToSend << '\n';
		state->advance(lengthToSend);
		if(withHeader)
			asyncWrite(state->bufferSequence, ioHandler([=]{callback(lengthToSend);}, errorHandler));
		else
			asyncWrite(state->bufferSequence[1], ioHandler([=]{callback(lengthToSend);}, errorHandler));
	},
	[=](auto callback)
	{
		asyncRead(sizeof(data::SegmentedTransfer::SegmentAck), ioHandler(
		[=]{
			assert( state->totalBytesSent <= totalSize );
			data::SegmentedTransfer::SegmentAck ackHeader;
			readObjectFromBuffer(ackHeader);
			if(ackHeader.code == data::SegmentedTransfer::ErrorCode)
			{
				logError("File transfer failed");
				requestCallback(CallbackCode::Error);
				return;
			}
			else if(state->totalBytesSent == totalSize && ackHeader.code == data::SegmentedTransfer::FinishedCode)
			{
				log("Sent all ", totalSize, " bytes successfully");
				requestCallback(CallbackCode::Success);
				return;
			}
			else if(ackHeader.code == data::SegmentedTransfer::ContinueCode)
			{
				const unsigned int currentProgress = 100.f * float(state->totalBytesSent) / float(totalSize);
				if(currentProgress > state->progress)
				{
					state->progress = currentProgress;
					log("Sent ", state->totalBytesSent, " / ", totalSize, " bytes (", currentProgress, "%)");
				}
				callback();
				return;
			}
			
			logError("Unexpected ack code received");
			requestCallback(CallbackCode::CriticalError);
			
		} ,errorHandler));
	});
	
}



}
