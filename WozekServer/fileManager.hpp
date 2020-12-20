#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <atomic>
#include "Datagrams.hpp"
#include "asio_lib.hpp"

namespace fs = std::filesystem;

class FileStream
{
	asio::io_context& ioContext;
	asio::io_context::strand strand;
	
	std::atomic<int> enqueuedWrites = 0;
	
	bool isFlushed = true;
	
public:
	
	std::fstream stream;
	
	FileStream(asio::io_context& ioContext_, const fs::path& path, const std::ios::openmode mode)
		: ioContext(ioContext_), strand(ioContext_), stream(path, mode)
	{
	}
	
	template <typename T>
	bool writeBufferAsync(const char* source, const size_t length, T&& callback)
	{
		if(!stream.is_open())
		{
			return false;
		}
		isFlushed = false;
		enqueuedWrites++;
		asio::post(strand, [=](){
			stream.write(source, length);
			callback(!stream.fail());
			enqueuedWrites--;
		});
		return true;
	}
	
	template <typename T>
	bool readFileAsync(char* dest, const size_t length, T&& callback)
	{
		if(!stream.is_open())
		{
			return false;
		}
		asio::post(strand, [=](){
			stream.read(dest, length);
			callback(!stream.fail());
		});
		return true;
	}
	
	void flushSync()
	{
		stream.flush();
		isFlushed = true;
	}
	
	void closeSync()
	{
		stream.close();
	}
	
};

class FileManager
{
	
	fs::path workingDirectory;
	fs::path mapFilesFolder = "maps";
	fs::path otherFilesFolder = "otherFiles";
	
	asio::io_context* ioContextPtr;
	
	void initDirectories()
	{
		fs::create_directory(workingDirectory / mapFilesFolder);
		fs::create_directory(workingDirectory / otherFilesFolder);
	}
	
public:	
	FileManager() {}
	
	bool hasContext() {return ioContextPtr != nullptr;}
	void setContext(asio::io_context& ioContext) {ioContextPtr = &ioContext;}
	auto& getContext() {return *ioContextPtr;}
	
	bool hasWorkingDirectory() {return workingDirectory.empty();}
	bool setWorkingDirectory(const fs::path& path) {
		if(!fs::is_directory(path)) 
			return false;
		workingDirectory = path;
		workingDirectory.make_preferred();
		initDirectories();
		return true;
	}
	
	auto getFileStream(const fs::path& path, std::ios::openmode mode)
	{
		return FileStream(getContext(), path, mode);
	}
		
	bool writeBufferToFile(const fs::path& path, std::ios::openmode mode, const char* buffer, size_t length)
	{
		std::ofstream file(path, mode | std::ios::binary);
		if(!file.is_open())
		{
			return false;
		}
		
		file.write(buffer, length);
		if(file.fail())
		{
			return false;
		}
		file.close();
		return true;
	}
	
	bool appendBufferToFile(const fs::path& path, const char* buffer, size_t length)
	{
		return writeBufferToFile(path, std::ios::app, buffer, length);
	}
	
	bool writeFileToBuffer(const fs::path& path, size_t offset, char* buffer, size_t length)
	{
		std::ifstream file(path, std::ios::binary);
		if(!file.is_open())
		{
			return false;
		}
		file.seekg(0, std::ios::end);
		size_t end = file.tellg();
		file.seekg(offset, std::ios::beg);
		
		if((size_t)file.tellg() > end || end - (size_t)file.tellg() < length)
		{
			return false;
		}
		
		file.read(buffer, length);
		if(file.fail())
		{
			return false;
		}
		file.close();
		return true;
	}
	
	// Any Files
	
	fs::path getOtherFilesPath(fs::path path)
	{
		return workingDirectory / otherFilesFolder / path;
	}
	
	size_t getFileSize(const fs::path& path)
	{
		if(!fs::exists(path) || !fs::is_regular_file(path))
		{
			return 0;
		}
		return fs::file_size(path);
	}
	
	void deleteFile(const fs::path& path)
	{
		fs::remove(path);
	}
	
	// MapFiles
	
	fs::path getPathToMapFile(data::IdType id)
	{
		fs::path res = workingDirectory;
		res /= mapFilesFolder;
		res /= "map_";
		res += std::to_string(id);
		return res;
	}
	
	void deleteMapFile(data::IdType id)
	{
		auto path = getPathToMapFile(id);
		fs::remove(path);
	}
	
};

extern FileManager fileManager;
