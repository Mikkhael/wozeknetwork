
#include <filesystem>
#include <fstream>
#include <string>
#include "Datagrams.hpp"
#include "asioWrapper.hpp"

namespace fs = std::filesystem;

class FileManager
{
	
	fs::path workingDirectory;
	fs::path mapFilesFolder = "maps";
	fs::path anyFilesFolder = "otherFiles";
	
	std::optional<asio::io_context::strand> strand;
	
	void initDirectories()
	{
		fs::create_directory(workingDirectory / mapFilesFolder);
		fs::create_directory(workingDirectory / anyFilesFolder);
	}
	
public:
	
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
	
	FileManager() {}
	
	bool hasStrand() {return strand.has_value();}
	void createStrand(asio::io_context& ioContext) {strand.emplace(ioContext);}
	auto& getStrand() {return strand.value();}
	
	bool hasWorkingDirectory() {return workingDirectory.empty();}
	bool setWorkingDirectory(const fs::path& path) {
		if(!fs::is_directory(path)) 
			return false;
		workingDirectory = path;
		workingDirectory.make_preferred();
		initDirectories();
		return true;
	}
	
	// Any Files
	
	fs::path getPathToFile(std::string_view name)
	{
		return workingDirectory / anyFilesFolder / name;
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
