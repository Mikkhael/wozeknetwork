#pragma once

#include "asioWrapper.hpp"
#include "DatabaseData.hpp"
#include <tuple>
#include <iostream>
#include <optional>

namespace db
{

class DatabaseManager
{
	std::optional<asio::io_context::strand> strand;
	
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
	
	void createStrand(asio::io_context& ioContext)
	{
		strand.emplace(ioContext);
	}
	
	bool hasStrand() {return strand.has_value();}
	asio::io_context::strand& getStrand() {return strand.value();}
	
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
	IdType createAndAddRecord(const typename Database::recordType<table>::Header& header);
	
	// Specific functions
	
	bool connectControllerToHost(IdType, IdType);
	
	// Async
	
	template <typename Handler>
	auto post(Handler handler)
	{
		return asio::post(getStrand(), handler);
	}
	
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
IdType DatabaseManager::createAndAddRecord(const typename Database::recordType<table>::Header& header)
{
	auto id = getNextFreeId<table>();
	auto& h = database->get<table>()[id].header;
	h = header;
	return h.id = id;
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
