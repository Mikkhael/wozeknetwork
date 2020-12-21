#pragma once

#include "asio_lib.hpp"
#include "logging.hpp"

#include "segmentedFileTransfer.hpp"

#include <type_traits>
#include <chrono>
#include <array>
#include <sstream>

#include "states.hpp"
#include "config.hpp"
#include "ipAuthorization.hpp"
#include "DatabaseManager.hpp"


namespace tcp
{

// prev WozekConnectionHandler
class WozekSession
	: public BasicSession<WozekSession, ArrayBuffer< Config::SessionBufferSize >, false > , public Statefull
{
public:
	
	enum class Type {None, Host, Controller};
	
private:
	
#ifndef RELEASE
	asio::steady_timer debugTimer;
	void debugHandler() { 
		std::cout << weak_from_this().use_count() << std::endl;
		debugTimer.expires_after(std::chrono::seconds(1));
		debugTimer.async_wait([=](const Error& err){debugHandler();});
	}
#endif
	
public:
	WozekSession(asio::io_context& ioContext)
		: BasicSession(ioContext), debugTimer(ioContext)
	{
		//assert(sharedFromThis().get() == this);
	}
	
	~WozekSession()
	{
		log("Connection Terminated");
	}
	
	/// Type and Database
	
	Type type = Type::None;
	db::IdType id = 0;
	
private:
	
	/// Logging
	
	std::string getPrefix() // TODO: add timestamp
	{
		std::stringstream ss;
		ss << "[ " << remoteEndpoint << ' ';
		if(type != Type::None)
			ss << (type == Type::Host ? 'H' : 'C') << id << ' ';
		ss << "] ";
		return ss.str();
	}
	
	template <typename ...Ts>
	void log(Ts&& ...args)
	{
		logger.output(getPrefix(), std::forward<Ts>(args)...);
	}
	template <typename ...Ts>
	void logError(Logger::Error name, Ts&& ...args)
	{
		if constexpr (sizeof...(args) > 0)
		{
			logger.output(getPrefix(), "Error {code: ", static_cast<int>(name), "} ", std::forward<Ts>(args)...);
		}
		logger.error(name);
	}
	template <typename ...Ts>
	void logError(Ts&& ...args)
	{
		logger.output(getPrefix(), "Error ", std::forward<Ts>(args)...);
		logger.error(Logger::Error::UnknownError);
	}
	
	/// Functionality ///
	
	void awaitRequest();
	void awaitRequestSilent();
	void finilizeRequest() { awaitRequest(); };
	void handleReceivedRequestId(char id);
	
		/// Echo ///
	
	static constexpr size_t MaxEchoRequestMessageLength = 100;
	void receiveEchoRequest();
	void receiveEchoRequestMessagePart();
	void handleEchoRequestMessagePart(const char c);
	void abortEchoRequest();
	void sendEchoResponse();
	
	
		/// File ///
	
	static constexpr size_t BigBUfferDefaultSize = 1024 * 1024 * 16;
	static constexpr size_t BigBufferSubdivisions = 2;
	void startSegmentedFileReceive(const fs::path path, const size_t totalSize);
	void receiveSegmentFileHeader();
	void handleSegmentFileHeader(const data::SegmentedFileTransfer::Header header);
	void sendSegmentFileError(const data::SegmentedFileTransfer::Error error);
	void receiveSegmentFileData(const size_t length);
	void handleSegmentFileData(const size_t length);
	void finalizeSegmentFileReceive();
	
	
		/// Controller ///
		
	void receiveRegisterAsControllerRequest();
	void handleRegisterAsControllerRequest(const data::RegisterAsController::RequestHeader& request);
	void finalizeRegisterAsControllerRequest(const data::RegisterAsController::ResponseHeader& response);
	
	/*
		
	// SegFmented ile Transfer
	
	void initiateFileTransferReceive(const size_t totalSize, fs::path path, std::function<void(bool)> callback, bool silent = false);
	void initiateFileTransferSend(fs::path path, std::function<void(bool)> callback, bool silent = false);
	
	// Register host
	
	void handleRegisterHostRequest();
	void tryRegisteringNewHost(db::Host::Header& header);
	void registerNewHostRequestSuccess();
	void registerNewHostRequestFailed();
	
	// Upload Map
	
	void handleUploadMapRequest();
	void finalizeUploadMap(data::UploadMap::RequestHeader header);
	
	// Download Map
	
	void handleDownloadMapRequest();
	void finalizeDownloadMap(data::DownloadMap::RequestHeader header);
	
	// Any Files Transfer
	
	void handleUploadFile();
	void handleDownloadFile();
	
	// World
	
	void handleStartTheWorld();
	
	/// Handlers ///
	
*/
	bool errorDisconnect(const Error& err) {
		if(checkIsShutdown())
		{
			return true;
		}
		
		if(err == asio::error::eof)
		{
			log("Disconnected");
		}
		else
		{
			logError(Logger::Error::TcpConnectionBroken, err);
		}
		return true;
	}
	
	bool errorAbort(const Error& err) {
		if(checkIsShutdown())
		{
			return true;
		}
		
		if(err == asio::error::eof)
		{
			logError(Logger::Error::TcpUnexpectedConnectionClosed, "Connection unexpectedly closed");
		}
		logError(Logger::Error::TcpConnectionBroken, err);
		return true;
	}

	bool errorCritical(const Error& err)
	{
		if(checkIsShutdown())
		{
			return true;
		}
		
		if(err == asio::error::eof)
		{
			logError(Logger::Error::TcpUnexpectedConnectionClosed, "Connection unexpectedly closed");
		}
		logError(Logger::Error::TcpConnectionBroken, err);
		returnCallbackCriticalError();
		return true;
	}
	
protected:
	virtual void timeoutHandler_impl()
	{
		logError(Logger::Error::TcpTimeout, "Socket timed out");
	}
	virtual bool start_impl()
	{
		log("New connection");
		logger.log(Logger::Log::TcpActiveConnections);
		logger.log(Logger::Log::TcpTotalConnections);
		
		awaitRequest();
		return true;
	}
	virtual void shutdown_impl()
	{
		log("Shutting down...");
		logger.log(Logger::Log::TcpActiveConnections, -1);
	}
	virtual void startError_impl(const Error& err)
	{
		logger.output("Unknown error while connecting to the remote endpoint: ", err);
		logger.error(Logger::Error::TcpUnknownError);
	}
};

class WozekServer : public BasicServer<WozekSession>
{
public:
	WozekServer(asio::io_context& ioContext_)
		: BasicServer(ioContext_)
	{
	}
	
protected:
	bool connectionErrorHandler_impl(const Error& err)
	{
		logger.output("Error occured while connecting:\n\t", err);
		return false;
	}
	
	bool authorisationChecker_impl(const asio::ip::tcp::endpoint remote) 
	{
		const auto address = remote.address();
		if(!address.is_v4())
		{
			logger.output("Connected from forbidden (v6) address: ", remote);
			logger.error(Logger::Error::TcpForbidden);
			return false;
		}
		
		if(!ipAuthorizer.checkIfIpv4IsAllowed(address.to_v4().to_uint()))
		{
			logger.output("Connected from forbidden address: ", remote);
			logger.error(Logger::Error::TcpForbidden);
			return false;
		}
		
		return true;
	}
};


}

