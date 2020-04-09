#include "tcp.hpp"


namespace tcp
{

void Connection::registerAsNewHost(data::RegisterNewHost::Request reqHeader, std::function<void(data::IdType)> requestCallback )
{	
	auto errorHandler = [=](const Error& err)
	{
		logError(err);
		requestCallback(0);
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
				requestCallback(0);
				return;
			}
			
			id = resHeader.id;
			requestCallback(id);
			return;
			
		}, errorHandler));
	}, errorHandler));
}

void Connection::uploadMap(fs::path path, std::function<void(bool)>requestCallback)
{
	auto errorHandler = [=](const Error& err)
	{
		logError(err);
		requestCallback(false);
	};
		
	if(!fs::is_regular_file(path))
	{
		logError("File dosen't exist: ", path);
		requestCallback(false);
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
				requestCallback(false);
				return;
			}
			if(resHeader.code == data::UploadMap::ResponseHeader::DenyAccessCode)
			{
				log("Invalid size specified");
				requestCallback(false);
				return;
			}
			
			log("Initiating transfer of file ", path, " with size of ", reqHeader.totalMapSize, " bytes.");
			initiateFileUpload(path, 4*1024*1024, 1300, requestCallback);
			
		}, errorHandler));
	}, errorHandler ));
	
}

void Connection::initiateFileUpload(fs::path path, size_t maxBigBufferSize, const size_t maxSegmentLength, std::function<void(bool)>requestCallback)
{
	auto errorHandler = [=](const Error& err)
	{
		logError(err);
		requestCallback(false);
	};
	
	using State = States::FileTransferSend;
	
	State* state = setState<State>();
	
	state->file.open(path, std::ios::binary);
	const size_t totalSize = fs::file_size(path);
	if(!state->file.is_open())
	{
		logError("Cannot open file: ", path);
		requestCallback(false);
		return;
	}
	
	state->bufferSequence[0] = asio::buffer(buffer, sizeof(data::SegmentedTransfer::SegmentHeader));
	
	const size_t bigBufferSize = std::min(totalSize, maxBigBufferSize);
	state->bigBuffer.resize(bigBufferSize);
	state->file.read(state->bigBuffer.data(), bigBufferSize);
	if(state->file.fail())
	{
		logError("Cannot read from file a");
		requestCallback(false);
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
				logError("Cannot read from file");
				requestCallback(false);
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
				requestCallback(false);
				return;
			}
			else if(state->totalBytesSent == totalSize && ackHeader.code == data::SegmentedTransfer::FinishedCode)
			{
				log("Sent all ", totalSize, " bytes successfully");
				requestCallback(true);
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
			requestCallback(false);
			
		} ,errorHandler));
	});
	
}



}
