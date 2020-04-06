#include "tcp.hpp"

namespace tcp
{

bool Connection::registerAsHost(data::Host::Header header, Error& err)
{
	buffer[0] = data::RegisterNewHost::Code;
	writeToBuffer(header, 1);
	
	sendFromBuffer(sizeof(header) + 1, err);
	if(err)
	{
		return false;
	}
	
	receiveToBuffer(1, err);
	if(err)
	{
		return false;
	}
	
	if(buffer[0] != data::RegisterNewHost::Success)
	{
		return false;
	}
	
	receiveToBuffer(sizeof(header.id), err);
	
	if(err)
	{
		return false;
	}
	
	
	std::memcpy(&id, buffer.data(), sizeof(id));
	
	return true;
}

/// Upload Map File ///

bool Connection::uploadMapFile(const fs::path& mapFilePath, size_t maxSegmentLength)
{
	if(maxSegmentLength == 0)
	{
		maxSegmentLength = bufferSize - sizeof(data::SegmentedTransfer::SegmentHeader);
	}
	
	std::ifstream file(mapFilePath, std::ios::binary);
	if(!file.is_open())
	{
		std::cout << "Cannot open file\n";
		return false;
	}
	
	size_t size = fs::file_size(mapFilePath);
	
	Error err;
	
	data::UploadMap::RequestHeader header;
	header.hostId = getId();
	header.totalMapSize = size;
	
	writeToBuffer(data::UploadMap::Code);
	writeToBuffer(header, 1);
	
	sendFromBuffer(sizeof(header) + 1, err);
	
	if(err)
	{
		std::cout << "Error: " << err << '\n';
		return false;
	}
	
	data::UploadMap::ResponseHeader resHeader;
	receiveToBuffer(sizeof(resHeader), err);
	
	if(err)
	{
		std::cout << "Error: " << err << '\n';
		return false;
	}
	
	loadFromBuffer(resHeader);
	
	if(resHeader.code == data::UploadMap::ResponseHeader::InvalidSizeCode)
	{
		std::cout << "Operation aborted: invalid size (possibly to big)\n";
		return false;
	}
	if(resHeader.code == data::UploadMap::ResponseHeader::DenyAccessCode)
	{
		std::cout << "Operation aborted: Access Denied\n";
		return false;
	}
	if(resHeader.code != data::UploadMap::ResponseHeader::AcceptCode)
	{
		std::cout << "Operation aborted: Unknown Error\n";
		return false;
	}
	
	std::cout << "Sending " << size << " bytes\n";
	
	size_t sendBytes = 0;
	while(sendBytes < size)
	{
		size_t bytesLeftInSegment = std::min(size - sendBytes, maxSegmentLength);
		
		data::SegmentedTransfer::SegmentHeader header;
		header.size = bytesLeftInSegment;
		
		size_t tempSegmentSize = std::min(bufferSize - sizeof(header), bytesLeftInSegment);
		
		writeToBuffer(header);
		file.read(buffer.data() + sizeof(header), tempSegmentSize);
		sendFromBuffer(sizeof(header) + tempSegmentSize, err);
		if(err)
		{
			std::cout << "Error: " << err << '\n';
			return false;
		}
		
		sendBytes += tempSegmentSize;
		bytesLeftInSegment -= tempSegmentSize;
		while(bytesLeftInSegment > 0)
		{
			tempSegmentSize = std::min(bufferSize, bytesLeftInSegment);
			file.read(buffer.data(), tempSegmentSize);
			sendFromBuffer(tempSegmentSize, err);
			if(err)
			{
				std::cout << "Error: " << err << '\n';
				return false;
			}
			sendBytes += tempSegmentSize;
			bytesLeftInSegment -= tempSegmentSize;
		}
		
		
		data::SegmentedTransfer::SegmentAck ackHeader;
		receiveToBuffer(sizeof(ackHeader), err);
		if(err)
		{
			std::cout << "Error: " << err << '\n';
			return false;
		}
		loadFromBuffer(ackHeader);
		
		if(ackHeader.code == data::SegmentedTransfer::ErrorCode)
		{
			std::cout << "Unknown error during file transfer\n";
			return false;
		}
		if(ackHeader.code == data::SegmentedTransfer::FinishedCode)
		{
			if(sendBytes != size)
			{
				std::cout << "Connection finished, but " << (size - sendBytes) << " bytes are still remaining to be transfered\n";
				return false;
			}
			std::cout << "Connection finished, send all " << sendBytes << " bytes successfuly\n";
			return true;
		}
		std::cout << header.size << " bytes sent ("<< sendBytes << " bytes in total)\n";
	}
	
	return false;
}



}
