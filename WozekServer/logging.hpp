#pragma once
#include "asioWrapper.hpp"
#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
#include <string>

#include "enum.hpp"

namespace fs = std::filesystem;

class Logger
{
public:
	
	SMARTENUM( Error, 
			UnknownError, TcpTimeout, TcpInvalidRequests, TcpForbidden, FileSystemError, TcpSegFileTransferError, TcpUnexpectedConnectionClosed,
			TcpConnectionBroken, UdpUnknownCode, UdpInvalidRequest, UdpUnknownError)
	
	SMARTENUM( Log, 
			TcpActiveConnections, TcpTotalConnections)
	
private:
	
	auto getTimestamp() { return std::chrono::duration_cast<std::chrono::seconds>( std::chrono::system_clock::now().time_since_epoch() ).count(); }
	
	std::ofstream outputFile;
	std::ofstream errorFile;
	std::ofstream logFile;
	
	
	std::array< unsigned long long, ErrorSize> errorArr;
	std::array< unsigned long long, LogSize> logArr;
	
	std::optional<asio::io_context::strand> strand;
	
	std::chrono::seconds saveLogsTimerDuration;
	std::optional<asio::steady_timer> saveLogsTimer;
	
	template <typename ...Args>
	bool writeToFile(std::ofstream& os, Args&& ...args)
	{
		if(!os.is_open() || os.fail())
		{
			return false;
		}
		asio::post(getStrand(), [&os, args...]{ 
			(os << ... << args);
			os.flush(); // Possibly delete
		});
		return true;
	}
	
	void saveLogsRecord()
	{
		auto now = getTimestamp();
		if(errorFile.is_open() && !errorFile.fail())
		{
			errorFile << now;
			for(size_t i=0; i<ErrorSize; i++)
			{
				errorFile << '\t' << errorArr[i];
			}
			errorFile << '\n';
			errorFile.flush();
		}
		if(logFile.is_open() && !logFile.fail())
		{
			logFile << now;
			for(size_t i=0; i<LogSize; i++)
			{
				logFile << '\t' << logArr[i];
			}
			logFile << '\n';
			logFile.flush();
		}
	}
	
	void saveLogsTimerStart(const std::chrono::seconds& duration)
	{
		saveLogsTimerDuration = duration;
		if(duration == std::chrono::seconds(0))
			return;
		saveLogsTimerStart();
	}
	void saveLogsTimerStart()
	{
		saveLogsTimer.value().expires_after(saveLogsTimerDuration);
		saveLogsTimer.value().async_wait([=](const ::Error& err){
			if(err)
				return;
			saveLogsRecord();
			saveLogsTimerStart();
		});
	}
	
public:
	
	Logger()
		: errorArr{}, logArr{}
	{
	}
	
	bool init(const fs::path& outputPath, const fs::path& errorPath, const fs::path& logPath, const std::chrono::seconds& saveLogsTimerDuration )
	{
		if(!strand.has_value())
			return false;
		
		outputFile.open(outputPath, std::ios::app);
		errorFile.open(errorPath, std::ios::app);
		logFile.open(logPath, std::ios::app);
		
		std::string header = "# New Session: ";
		header += std::to_string(getTimestamp());
		header += '\n';
		
		if(outputFile.is_open())
		{
			outputFile << header;
		}
		if(errorFile.is_open())
		{
			errorFile << header << "#Timestamp";
			for(size_t i=0; i<ErrorSize; i++)
			{
				errorFile << '\t' << ErrorGetName(i);
			}
			errorFile << '\n';
		}
		if(logFile.is_open())
		{
			logFile << header << "#Timestamp";
			for(size_t i=0; i<LogSize; i++)
			{
				logFile << '\t' << LogGetName(i);
			}
			logFile << '\n';
		}
		
		saveLogsTimerStart(saveLogsTimerDuration);
		
		return true;
	}
	
	bool isOutputFileOpen() { return outputFile.is_open(); }
	bool isErrorFileOpen() { return errorFile.is_open(); }
	bool isLogFileOpen() { return logFile.is_open(); }
	auto& getStrand() { return strand.value(); }
	
	void setStrand(asio::io_context& ioContext)
	{
		strand.emplace(ioContext);
		saveLogsTimer.emplace(ioContext);
	}
	
	
	bool writeOutputToStdout = true;
	
	template <typename ...Args>
	bool output(Args&& ...args)
	{
		if(writeOutputToStdout)
		{
			(std::cout << ... << args);
		}
		
		return writeToFile(outputFile, std::forward<Args>(args)... );
	}
	
	void log(const Log name, const long long n = 1)
	{
		asio::post(getStrand(), [=]{ 
			logArr[static_cast<int>(name)] += n;
		});
	}
	
	void error(const Error name, const long long n = 1)
	{
		asio::post(getStrand(), [=]{ 
			errorArr[static_cast<int>(name)] += n;
		});
	}
	
	void showErrors(std::ostream& os)
	{
		os << "Errors as of " << getTimestamp() << ":\n";
		for(size_t i=0; i<ErrorSize; i++)
		{
			os << ErrorGetName(i) << ": " << errorArr[i] << '\n';
		}
	}
	
	
	void showLogs(std::ostream& os)
	{
		os << "Logs as of " << getTimestamp() << ":\n";
		for(size_t i=0; i<LogSize; i++)
		{
			os << LogGetName(i) << ": " << logArr[i] << '\n';
		}
	}
	
};





extern Logger logger;
