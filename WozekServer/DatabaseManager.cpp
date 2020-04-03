#include "DatabaseManager.hpp"

namespace db
{


bool DatabaseManager::connectControllerToHost(IdType controllerId, IdType hostId)
{
	auto controllerItr = getItr<Database::Table::Controller>(controllerId);
	if(isLast<Database::Table::Controller>(controllerItr))
	{
		return false;
	}
	
	auto hostItr = getItr<Database::Table::Host>(hostId);
	if(isLast<Database::Table::Host>(hostItr))
	{
		return false;
	}
	
	if(controllerItr->second.host != 0)
	{
		if(controllerItr->second.host == hostId)
		{
			return true;
		}
		
		auto oldHostItr = getItr<Database::Table::Host>(controllerItr->second.host);
		if(!isLast<Database::Table::Host>(oldHostItr))
		{
			oldHostItr->second.connectedControllers.erase(oldHostItr->second.connectedControllers.find(controllerId));
		}
	}
	
	controllerItr->second.host = hostId;
	return true;
}

DatabaseManager databaseManager;

}

