#pragma once

#include "asio_lib.hpp"
#include "fileManager.hpp"

#include <variant>
#include <fstream>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

namespace States
{
	struct Empty {};
	
	struct EchoMessageBuffer
	{
		std::string buffer;
	};
	
	
	struct SegmentedFileTransfer
	{
		std::vector<char> bigBuffer;
		size_t bufferFilled = 0;
		std::optional<FileStream> fileStream;
		size_t bytesRemaining = 0;
		size_t fileSegmentLengthLeft = 0;
		
		bool internalFileError = false;
	};
	
	/*
	
	struct FileTransferReceive
	{
		std::ofstream file;
		std::vector<char> bigBuffer;
		size_t bigBufferTop = 0;
		size_t totalBytesReceived = 0;
		unsigned int progress = 0;
		
		size_t getRemainingBigBufferSize() {return bigBuffer.size() - bigBufferTop;}
		char* getBigBufferEnd() {return bigBuffer.data() + bigBufferTop;}
		void advance(const size_t bytes) {bigBufferTop += bytes; totalBytesReceived += bytes;}
	};
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
	struct FileTransferSendThreadsafe
	{
		fs::path path;
		std::vector<char> bigBuffer;
		std::array<asio::const_buffer, 2> bufferSequence;
		
		size_t bigBufferTop = 0;
		size_t totalBytesSent = 0;
		unsigned int progress = 0;
		
		size_t getRemainingBigBufferSize() {return bigBuffer.size() - bigBufferTop;}
		char* getBigBufferEnd() {return bigBuffer.data() + bigBufferTop;}
		void advance(const size_t bytes) {bigBufferTop += bytes; totalBytesSent += bytes;}
	};
	
	*/
	
	
	using Type = std::variant<Empty, EchoMessageBuffer, SegmentedFileTransfer>;
}


class Statefull
{
	States::Type state;
	
public:
	
	template <typename T>
	T& setState()
	{
		//std::cout << "TUTAJ\n";
		return state.emplace<T>();
	}
	template <typename T>
	T& getState()
	{
		return std::get<T>(state);
	}
	
	void resetState()
	{
		//std::cout << "TEZTUTAJ\n";
		state.emplace<States::Empty>();
	}
};
