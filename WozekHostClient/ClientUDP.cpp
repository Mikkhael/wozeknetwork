#include "ClientUDP.hpp"


namespace udp
{

void Connection::updateState(Callback requestCallback)
{
	auto requestCode = data::UpdateHostState::Code;
	data::UpdateHostState::Header header = gameManager.getUpdateStateHeader();
	
	auto headerLength = writeObjectsToBufferFromOffset(0, requestCode, header);
	
	// Controllers data
	
	Error err;
	send(headerLength, err);
	
	if(err)
	{
		logError("Error while sending udp UpdateState: ", err);
	}
	
	requestCallback(err ? CallbackCode::Error : CallbackCode::Success);
}

}
