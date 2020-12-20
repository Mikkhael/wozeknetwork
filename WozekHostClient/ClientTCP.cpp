#include "ClientTCP.hpp"

namespace tcp
{
void WozekSessionClient::sendHeartbeat()
{
	Error err;
	const char null = '\0';
	
	asio::write(getSocket(), asio::buffer( &null , 1 ), err);
	if(err) { errorCritical(err); return; }
	
	returnCallbackGood();
}


void WozekSessionClient::performEchoRequest(const std::string& message)
{
	Error err;
	
	std::cout << "Performing Echo Request with message: \"" << message << "\"\n";
	const char requestId = data::EchoRequest::request_id;
	const char null = '\0';
	
	asio::write(getSocket(), asio::buffer( &requestId , 1 ), err);
	if(err) { errorCritical(err); return; }
	
	asio::write(getSocket(), asio::buffer( message ), err);
	if(err) { errorCritical(err); return; }
	
	asio::write(getSocket(), asio::buffer( &null, 1 ), err);
	if(err) { errorCritical(err); return; }
	
	setState<States::EchoMessageBuffer>();
	
	std::cout << "Receiving Echo Response...\n";
	receiveEchoResponsePart();	
}

void WozekSessionClient::receiveEchoResponsePart()
{
	asyncReadObjects<char>(
		&WozekSessionClient::handleEchoResponsePart,
		&WozekSessionClient::errorCritical
	);
}

void WozekSessionClient::handleEchoResponsePart(const char c)
{
	auto& state = getState<States::EchoMessageBuffer>();
	if(c == '\0')
	{
		finilizeEcho(state.buffer);
		return;
	}
	state.buffer += c;
	receiveEchoResponsePart();
}

void WozekSessionClient::finilizeEcho(std::string& receivedMessage)
{
	std::cout << "Received Echo Response: " << receivedMessage << "\"\n";
	auto result = new ValueCallbackResult<std::string>;
	auto result_base = std::unique_ptr<CallbackResult>(result);
	std::swap(result->value, receivedMessage);
	resetState();
	returnCallbackStack(std::move(result_base));
}

/// File ///

void WozekSessionClient::startSegmentedFileSend(const fs::path sourcePath, const size_t fileSize)
{
	auto& state = setState<States::SegmentedFileTransfer>();
	state.bigBuffer.resize(DefaultBigBufferSize);
	state.fileStream.emplace(getContext(), sourcePath, std::ios::in | std::ios::binary);
	state.bytesRemaining = fileSize;
}

void WozekSessionClient::sendSegmentHeader()
{
	auto& state = getState<States::SegmentedFileTransfer>();
	
	
	if(state.bytesRemaining == 0)
	{
		
		return;
	}
	
	if(state.fileSegmentLengthLeft == 0)
	{
		state.fileStream.file.read(state.bigBuffer.data(), state.bigBuffer.size());
		state.fileSegmentLengthLeft = DefaultSegmentsInBuffer;
	}
	
	if(state.bytesRemaining < DefaultSegmentSize)
	{
		
		return;
	}
	
	data::SegmentedFileTransfer::Header header = {}; // TODO
	
	asyncWriteObjects(
		&WozekSessionClient::handleSegmentFileHeaderResponseCode,
		&WozekSessionClient::errorCritical,
		header
	);
	
	// TODO ALL
	
}

void WozekSessionClient::handleSegmentFileHeaderResponseCode(const data::SegmentedFileTransfer::Error error)
{
	
}

void WozekSessionClient::sendSegmentFileData()
{
	
}

void WozekSessionClient::handleSegmentFileDataResponseCode(const data::SegmentedFileTransfer::Error error)
{
	
}

void WozekSessionClient::finalizeSegmentFileSend()
{
	
}



}
