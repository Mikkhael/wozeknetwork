#include "ControllerSessionClient.hpp"

#include <cstring>

namespace tcp
{
	
	

void ControllerSessionClient::registerAsControllerRequest(std::string_view name)
{
	data::RegisterAsController::RequestHeader request;
	if(name.size() >= sizeof(request.name))
	{
		std::memcpy(&request.name, name.data(), sizeof(request.name) - 1);
		request.name[sizeof(request.name) - 1] = '\0';
	}
	else
	{
		std::memcpy(&request.name, name.data(), name.size());
		request.name[name.size()] = '\0';
	}
	
	asyncWriteObjects(
		&ControllerSessionClient::receiveRegisterAsControllerResponse,
		&ControllerSessionClient::errorCritical,
		data::RegisterAsController::request_id,
		request
	);
	
}

void ControllerSessionClient::receiveRegisterAsControllerResponse()
{
	asyncReadObjects<data::RegisterAsController::ResponseHeader>(
		&ControllerSessionClient::handleRegisterAsControllerResponse,
		&ControllerSessionClient::errorCritical
	);
}

void ControllerSessionClient::handleRegisterAsControllerResponse(const data::RegisterAsController::ResponseHeader& response)
{
	auto result = new ValueCallbackResult<data::IdType>();
	auto result_base = std::unique_ptr<CallbackResult>(result);
	if(response.resultCode == data::RegisterAsController::ResponseHeader::ResultCode::Accepted)
	{
		std::cout << "Name Accepted\n";
		
		result->value = response.id;
		returnCallbackStack(std::move(result_base));
		return;
	}
	if(response.resultCode == data::RegisterAsController::ResponseHeader::ResultCode::Invalid)
	{
		std::cout << "Invalid name\n";
		
		result->status = CallbackResult::Status::Error;
		result->value = 1;
		returnCallbackStack(std::move(result_base));
		return;
	}
	if(response.resultCode == data::RegisterAsController::ResponseHeader::ResultCode::InUse)
	{
		std::cout << "Name already in use\n";
		
		result->status = CallbackResult::Status::Error;
		result->value = 2;
		returnCallbackStack(std::move(result_base));
		return;
	}
}



void ControllerSessionClient::startKeepAlive(const std::chrono::milliseconds interval)
{
	// TODO
}

void ControllerSessionClient::stopKeepAlive()
{
	// TODO
}



}
