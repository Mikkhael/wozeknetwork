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
	
	size_t lastPercent = 0; // for logging
	
	size_t sendBytes = 0;
	while(true)
	{
		data::SegmentedTransfer::SegmentHeader header;
		header.size = std::min(size - sendBytes, maxSegmentLength);
		
		writeToBuffer(header);
		
		size_t leftToSend = header.size;
		size_t trimmedOffset = sizeof(header);
		while(leftToSend > 0)
		{
			size_t bytesToSend = std::min(buffer.size() - trimmedOffset, leftToSend);
			file.read(buffer.data() + trimmedOffset, bytesToSend);
			sendFromBuffer(bytesToSend + trimmedOffset, err);
			if(err)
			{
				std::cout << "Error: " << err << '\n';
				return false;
			}
			leftToSend -= bytesToSend;
			trimmedOffset = 0;
		}
		
		
		sendBytes += header.size;
		
		data::SegmentedTransfer::SegmentAck ackHeader;
		receiveToBuffer(sizeof(ackHeader), err);
		if(err)
		{
			std::cout << "Error: " << err << '\n';
			return false;
		}
		loadFromBuffer(ackHeader);
		
		if(sendBytes == size)
		{
			if(ackHeader.code != data::SegmentedTransfer::FinishedCode)
			{
				std::cout << "Connection finished, but wrong response code received.";
				return false;
			}
			std::cout << "Connection finished, send all " << sendBytes << " bytes successfuly\n";
			return true;
		}
		if(ackHeader.code != data::SegmentedTransfer::ContinueCode)
		{
			std::cout << "Unknown error during file transfer\n";
			return false;
		}
		size_t newPercent = 100.f * float(sendBytes) / float(size);
		if(newPercent > lastPercent)
		{
			lastPercent = newPercent;
			std::cout << "(" << lastPercent << " / 100)" << sendBytes << " bytes in total sent\n";
		}
	}
	
	return false;
}

/// Download map file ///

bool Connection::downloadMapFile(data::IdType id, const fs::path& mapFilePath)
{
	std::ofstream file(mapFilePath, std::ios::binary | std::ios::trunc);
	
	if(!file.is_open())
	{
		std::cout << "Cannot open file\n";
		return false;
	}
	
	data::DownloadMap::RequestHeader header;
	header.hostId = id;
	writeToBuffer(data::DownloadMap::Code);
	writeToBuffer(header, sizeof(data::DownloadMap::Code));
	
	Error err;
	
	sendFromBuffer(sizeof(header) + sizeof(data::DownloadMap::Code), err);
	if(err)
	{
		std::cout << "Error: " << err << '\n';
		return false;
	}
	
	data::DownloadMap::ResponseHeader resHeader;
	receiveToBuffer(sizeof(resHeader), err);
	if(err)
	{
		std::cout << "Error: " << err << '\n';
		return false;
	}
	loadFromBuffer(resHeader);
	
	if(resHeader.code != data::DownloadMap::ResponseHeader::AcceptCode)
	{
		std::cout << "Access to the map file denied\n";
		return false;
	}
	
	size_t bytesReceived = 0;
	
	std::cout << "Strating transfer\n";
	
	std::vector<char> bigBuffer;
	bigBuffer.resize(std::min(resHeader.totalMapSize, size_t(4 * 1024 * 1024)));
	
	size_t lastPercent = 0; // for logging
	
	while(true)
	{
		data::SegmentedTransfer::SegmentHeader header;
		receiveToBuffer(sizeof(header), err);
		if(err)
		{
			std::cout << "Error: " << err << '\n';
			return false;
		}
		
		loadFromBuffer(header);
		
		if(header.size + bytesReceived > resHeader.totalMapSize)
		{
			std::cout << "Sum of segments lengths exceeded total file size. Closing connection.";
			socket.close();
			return false;
		}
		
		size_t leftToRead = header.size;
		while(leftToRead > 0)
		{
			size_t bytesRead = std::min(bigBuffer.size(), leftToRead);
			asio::read(socket, asio::buffer(bigBuffer.data(), bytesRead));
			file.write(bigBuffer.data(), bytesRead);
			leftToRead -= bytesRead;
		}
		
		bytesReceived += header.size;
		
		data::SegmentedTransfer::SegmentAck ackHeader;
		if(bytesReceived == resHeader.totalMapSize)
		{
			ackHeader.code = data::SegmentedTransfer::FinishedCode;
			asio::write(socket, asio::buffer(&ackHeader, sizeof(ackHeader)), err);
			if(err)
			{
				std::cout << "Error: " << err << '\n';
				return false;
			}
			std::cout << "File transfer finished. Received total of " << bytesReceived << " bytes\n";
			return true;
		}
				
		ackHeader.code = data::SegmentedTransfer::ContinueCode;
		asio::write(socket, asio::buffer(&ackHeader, sizeof(ackHeader)), err);
		if(err)
		{
			std::cout << "Error: " << err << '\n';
			return false;
		}
		
		size_t newPercent = 100.f * float(bytesReceived) / float(resHeader.totalMapSize);
		if(newPercent > lastPercent)
		{
			lastPercent = newPercent;
			std::cout << "(" << lastPercent << "/100) " << bytesReceived << " bytes received\n";
		}
		
	}
	
}


}
