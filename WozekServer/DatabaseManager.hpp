#pragma once

#include "asio_lib.hpp"
#include <tuple>
#include <iostream>
#include <optional>
#include <typeinfo>

#include "DatabaseData.hpp"

namespace db
{

class Database
{
public:
	
	asio::io_context& ioContext;
	
	ControllerTable controllerTable;
	
	Database(asio::io_context& ioContext_)
		: ioContext(ioContext_), controllerTable(ioContext_)
	{
	}
};

class DatabaseManager
{
	std::optional<Database> database;
	
public:
	
	void setContext(asio::io_context& ioContext) {database.emplace(ioContext);}
	bool hasContext() {return database.has_value();}
	auto& getContext() {assert(hasContext()); return database.value().ioContext;}
	
	auto& getDatabase() {
		assert( hasContext() );
		return database.value();
	}
	
	DatabaseManager()
	{
	}
	
};

extern DatabaseManager databaseManager;



}

#ifndef RELEASE


#endif // RELEASE
