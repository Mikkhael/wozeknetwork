#pragma once

/*

#include "logging.hpp"
#include "Datagrams.hpp"
#include "DatabaseManager.hpp"
#include "UDPServerHandlerBase.hpp"

namespace udp
{

class WozekServerStateHandler : public ServerHandlerBase
{
	
	std::optional< std::function<void( const asioudp::endpoint )> > parseDatagramAndGetHandler(const char code, bufferType& buffer, const size_t length)
	{
		
		switch (code)
		{
			
		case data::UpdateHostState::Code:
			{
				size_t off = 1;
				data::UpdateHostState::Header header;
				if(!loadObjectFromBufferSafe(header, buffer, length, off))
				{
					break;
				}
				if(length != header.getSizeof() + 1)
				{
					break;
				}
				std::vector<data::UpdateHostState::ControllerData> controllerDatas(header.controllersCount);
				for(auto& x : controllerDatas)
				{
					loadObjectFromBuffer(x, buffer, off);
				}
				return [=, controllerDatas_ = std::move(controllerDatas)](const asioudp::endpoint senderEndpoint){ 
					handleUpdateHostState(senderEndpoint, header, controllerDatas_);
					};
			}
		default:
			{
				logError(Logger::Error::UdpUnknownCode, "Unknown datagram code: ", static_cast<int>(code));
				return {};
			}
			
		}
		
		logError(Logger::Error::UdpInvalidRequest, "Invalid datagram structure. Code: ", static_cast<int>(code));
		return {};
	}
	
	
	void handleUpdateHostState( const asioudp::endpoint& senderEndpoint,
								const data::UpdateHostState::Header& header,
								const std::vector<data::UpdateHostState::ControllerData>& controllerDatas)
	{
		asio::post(db::databaseManager.getStrand(), [=]{
			auto worldPtr = db::databaseManager.get<db::Database::Table::World>(header.worldId);
			if(!worldPtr)
			{
				logError(Logger::Error::UdpInvalidRequest, "World with id ", header.worldId, " dosent exist (sender: ", senderEndpoint, ")");
				return;
			}
			auto& world = *worldPtr;
			if(!world.validateMainHostEndpoint(senderEndpoint))
			{
				logError(Logger::Error::UdpInvalidRequest, "Sender is not the main host of the world. (WorldId: ", header.worldId,", received: ", senderEndpoint, ", expected: ", world.getMainHostRemoteEndpoint(),")\n");
				return;
			}
			log("Successful request received. WorldId: ", header.worldId, ", Timepoint: ", header.timepoint, ", ControllersSize: ", header.controllersCount);
		});
	}
	
public:
	
	void operator()( const asioudp::endpoint& senderEndpoint, bufferType& buffer, const size_t length )
	{
		const char code = buffer[0];
		auto handler = parseDatagramAndGetHandler(code, buffer, length);
		
		if(handler)
		{
			(*handler)(senderEndpoint);
		}
	}
	
};


struct WozekStateServerErrorHandler
{
	bool operator()(const asioudp::endpoint& endpoint, const Error& err)
	{
		logger.output("[", endpoint, "] Error occured while receiving UDP datagram:\n", err);
		logger.error(Logger::Error::UdpUnknownError);
		return true;
	}
};

struct WozekStateServerShutdownHandler
{
	void operator()()
	{
		logger.output("UDP State Server shut down.");
	}
};

using UdpWozekStateServer = udp::GenericServer<WozekServerStateHandler, WozekStateServerErrorHandler, WozekStateServerShutdownHandler>;


}

*/
