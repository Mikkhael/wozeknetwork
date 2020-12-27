#include "ClientUDP.hpp"

namespace udp
{



void WozekUDPSender::sendEchoMessage(const std::string& message)
{
	const auto length = message.size() < BufferSize ? message.size() : BufferSize;
	buffer.saveObject(char(data::EchoRequest::request_id));
	buffer.saveBytesAt(1, message.c_str(), length);
	
	std::cout << "Sending echo message : \"" << message << "\" of length " << length << " To endpoint: " << remoteEndpoint << '\n';
	
	asyncWriteTo(
		buffer.get(length + 1),
		remoteEndpoint,
		&WozekUDPSender::returnCallbackGood,
		&WozekUDPSender::errorCritical
	);
}

void WozekUDPSender::sendUdpStateUpdate(const data::RotationType rotation[3], const data::IdType& id)
{
	std::cout << "Sending rotation " << rotation[0] << ' ' << rotation[1] << ' ' << rotation[2] << " for id " << id << '\n';
	
	int offset = 0;
	offset = buffer.saveObjectAt(offset, char(data::UdpUpdateState::request_id));
	offset = buffer.saveObjectAt(offset, id);
	offset = buffer.saveBytesAt(offset, (char*)rotation, sizeof(data::RotationType) * 3);
	
	//static_assert(sizeof(rotation) == sizeof(data::RotationType) * 3);
	
	asyncWriteTo(
		buffer.get(1 + sizeof(id) + (sizeof(data::RotationType) * 3) ),
		remoteEndpoint,
		&WozekUDPSender::returnCallbackGood,
		&WozekUDPSender::errorCritical
	);
}



void WozekUDPReceiver::handleDatagram()
{
	char requestId;
	buffer.loadBytes(&requestId, 1);
	
	switch(requestId)
	{
		case data::EchoRequest::response_id:
		{
			handleEchoResponse();
			break;
		}
		default:
		{
			std::cout << "Unknown UDP response id: " << static_cast<int>(requestId) << '\n';
		}
	}
}

void WozekUDPReceiver::handleEchoResponse()
{
	std::cout << "Receiving Response Echo message.\n";
	std::string message;
	message.resize(bytesTransfered - 1);
	buffer.loadBytesAt(1, message.data(), bytesTransfered - 1);
	std::cout << "Received Echo Message: " << message << '\n';
}



}
