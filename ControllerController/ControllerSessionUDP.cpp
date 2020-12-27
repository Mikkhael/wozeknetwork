#include "ControllerSessionUDP.hpp"
#include "State.hpp"

namespace udp
{



void ControllerUDPSender::sendEchoMessage(const std::string& message)
{
	const auto length = message.size() < BufferSize ? message.size() : BufferSize;
	buffer.saveObject(char(data::EchoRequest::request_id));
	buffer.saveBytesAt(1, message.c_str(), length);
	
	std::cout << "Sending echo message : \"" << message << "\" of length " << length << " To endpoint: " << remoteEndpoint << '\n';
	
	asyncWriteTo(
		buffer.get(length + 1),
		remoteEndpoint,
		&ControllerUDPSender::returnCallbackGood,
		&ControllerUDPSender::errorCritical
	);
}

void ControllerUDPSender::sendFetchStateRequest(const data::IdType id)
{
	std::cout << "Sending Fetch State Request for id " << id << " to endpoint: " << remoteEndpoint << '\n';
	
	asyncWriteObjectsTo(
		remoteEndpoint,
		&ControllerUDPSender::returnCallbackGood,
		&ControllerUDPSender::errorCritical,
		char(data::UdpFetchState::request_id),
		id
	);
}


void ControllerUDPReceiver::handleDatagram()
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
		case data::UdpFetchState::response_id:
		{
			handleFetchStateResponse();
			break;
		}
		default:
		{
			std::cout << "Unknown UDP response id: " << static_cast<int>(requestId) << '\n';
		}
	}
}

void ControllerUDPReceiver::handleEchoResponse()
{
	std::cout << "Receiving Response Echo message.\n";
	std::string message;
	message.resize(bytesTransfered - 1);
	buffer.loadBytesAt(1, message.data(), bytesTransfered - 1);
	std::cout << "Received Echo Message: " << message << '\n';
}

void ControllerUDPReceiver::handleFetchStateResponse()
{
	std::cout << "Received State Update\n";
	data::UdpFetchState::Response response;
	buffer.loadObjectAt(1, response);
	
	std::cout << "1: " << (int)response.values[0] << '\n';
	std::cout << "2: " << (int)response.values[1] << '\n';
	std::cout << "3: " << (int)response.values[2] << '\n';
	
	associatedStatePtr->rotationState.updateValue(&response.values);
}


State* ControllerUDPReceiver::associatedStatePtr;


}
