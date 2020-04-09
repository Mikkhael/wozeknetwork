#pragma once

#include "asioWrapper.hpp"

#include <variant>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace States
{
	struct Empty {};
	
	struct FileTransferSend
	{
		std::ifstream file;
		std::vector<char> bigBuffer;
		std::array<asio::const_buffer, 2> bufferSequence;
		size_t bigBufferTop = 0;
		size_t totalBytesSent = 0;
		unsigned int progress = 0;
		
		size_t getRemainingBigBufferSize() {return bigBuffer.size() - bigBufferTop;}
		char* getBigBufferEnd() {return bigBuffer.data() + bigBufferTop;}
		void advance(const size_t bytes) {bigBufferTop += bytes; totalBytesSent += bytes;}
	};
	
	struct FileTransferReceiveThreadsafe
	{
		fs::path path;
		std::vector<char> bigBuffer;
		size_t bigBufferTop = 0;
		size_t totalBytesRead = 0;
		unsigned int progress = 0;
		
		size_t getRemainingBigBufferSize() {return bigBuffer.size() - bigBufferTop;}
		char* getBigBufferEnd() {return bigBuffer.data() + bigBufferTop;}
		void advance(const size_t bytes) {bigBufferTop += bytes; totalBytesRead += bytes;}
	};
	
	
	
	
	using Type = std::variant<Empty, FileTransferSend, FileTransferReceiveThreadsafe>;
}
