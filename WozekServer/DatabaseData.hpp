#pragma once

#include "Datagrams.hpp"
#include "World.hpp"

#include <unordered_map>
#include <unordered_set>

#include <memory>

namespace tcp {
	class WozekConnectionHandler;
}

namespace db
{

using IdType = data::IdType;

template <typename T>
using IdMap = std::unordered_map<IdType, T>;

struct Host
{
	using Header = data::Host::Header;
	
	struct Network
	{
		std::weak_ptr<tcp::WozekConnectionHandler> tcpConnection;
	};
	
	Header header;
	Network network;
};

struct Controller
{
	using Header = data::Controller::Header;
	struct Network
	{
		std::weak_ptr<tcp::WozekConnectionHandler> tcpConnection;
	};
	
	Header header;
	Network network;
};


struct World : public ::World
{	
	struct Header
	{
		IdType id;
	};
	
	Header header;
};

struct Database
{
	IdMap<Host> hosts;
	IdMap<Controller> controllers;
	IdMap<World> worlds;
	
	
	enum class Table {Host, Controller, World};
	
	template <Table table>
	auto& get()
	{
		if constexpr	  (table == Table::Host)
			return hosts;
		else if constexpr (table == Table::Controller)
			return controllers;
		else if constexpr (table == Table::World)
			return worlds;
	}
	
	template <Table table>
	using tableType = typename std::remove_reference< typename std::invoke_result<decltype(&Database::get<table>), Database>::type >::type;
	template <Table table>
	using recordType = typename tableType<table>::mapped_type;
	
};

static_assert(std::is_same< Database::recordType<Database::Table::Host>, Host >::value);

	
}


