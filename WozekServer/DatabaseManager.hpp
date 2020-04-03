#pragma once

#include "DatabaseData.hpp"
#include <tuple>
#include <iostream>

namespace db
{

class DatabaseManager
{
	Database* database = nullptr;
	
	template<Database::Table table>
	bool isLast(const typename Database::tableType<table>::iterator itr) const
	{
		return itr == database->get<table>().end();
	}
	
	template<Database::Table table>
	IdType getNextFreeId();
	
public:
	DatabaseManager(Database* database_ = nullptr)
		: database(database_)
	{
	}
	
	bool hasDatabase()
	{
		return database != nullptr;
	}
	
	void setDatabase(Database& database_)
	{
		setDatabase(&database_);
	}
	void setDatabase(Database* database_)
	{
		database = database_;
	}
	
	// Basic functions
	
	template<Database::Table table>
	auto getItr(IdType id) const;
	
	template<Database::Table table>
	Database::recordType<table>* get(IdType id) const;
	
	template<Database::Table table>
	bool removeRecord(IdType id);
	
	template<Database::Table table>
	bool addRecord(const typename Database::recordType<table>::Header& header);
	
	template<Database::Table table>
	bool createAndAddRecord(typename Database::recordType<table>::Header& header);
	template<Database::Table table>
	bool createAndAddRecord(const typename Database::recordType<table>::Header& header);
	
	// Specific functions
	
	bool connectControllerToHost(IdType, IdType);
};

extern DatabaseManager databaseManager;


/// Definitions

template<Database::Table table>
IdType DatabaseManager::getNextFreeId()
{
	static IdType lastId = 0;
	
	while(!isLast<table>(getItr<table>(++lastId)));
	return lastId;
}

template<Database::Table table>
auto DatabaseManager::getItr(IdType id) const
{
	return database->get<table>().find(id);
}

template<Database::Table table>
Database::recordType<table>* DatabaseManager::get(IdType id) const
{
	auto itr = getItr<table>(id);
	if(isLast<table>(itr))
		return nullptr;
		
	return &itr->second;
}

template<Database::Table table>
bool DatabaseManager::addRecord(const typename Database::recordType<table>::Header& header)
{
	auto id = header.id;
	if(!isLast<table>(getItr<table>(id)))
		return false;
		
	database->get<table>()[id].header = header;
	return true;
}

template<Database::Table table>
bool DatabaseManager::createAndAddRecord(typename Database::recordType<table>::Header& header)
{
	header.id = getNextFreeId<table>();
	return addRecord<table>(header);
}
template<Database::Table table>
bool DatabaseManager::createAndAddRecord(const typename Database::recordType<table>::Header& header)
{
	auto newHeader = header;
	return createAndAddRecord<table>(newHeader);
}

template<Database::Table table>
bool DatabaseManager::removeRecord(IdType id)
{
	auto itr = getItr<table>(id);
	if(isLast<table>(itr))
		return false;
	
	if constexpr (table == Database::Table::Host)
	{
		// Disconnect all connected controllers
		for(auto& controllerId : itr->second.connectedControllers)
		{
			auto controllerPtr = get<Database::Table::Controller>(controllerId);
			if(controllerPtr && controllerPtr->host == itr->second.header.id)
			{
				controllerPtr->host = 0;
			}
		}
	}
	else if (table == Database::Table::Controller)
	{
		// TODO
	}
	
	database->get<table>().erase(itr);
	return true;
}



}

#ifndef RELEASE



inline void showDatabase(const db::Database& database)
{
	std::cout << "Hosts: \n";
	for(auto& host : database.hosts)
	{
		std::cout << "  " << host.first << ": " << host.second.header.id << ", " << host.second.header.name << '\n';
	}
	
	std::cout << "Controllers: \n";
	for(auto& controller : database.controllers)
	{
		std::cout << "  " << controller.first << ": " << controller.second.header.id << '\n';
	}
}

#endif // RELEASE
