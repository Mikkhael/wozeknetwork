#pragma once

#include "Datagrams.hpp"

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
	std::unordered_set<IdType> connectedControllers;
};

struct Controller
{
	using Header = data::Controller::Header;
	using DesiredState = data::Controller::DesiredState;
	using MeasuredState = data::Controller::MeasuredState;
	
	Header header;
	DesiredState desiredState;
	MeasuredState measuredState;
	IdType host = 0;
};

struct Database
{
	IdMap<Host> hosts;
	IdMap<Controller> controllers;
	
	
	
	enum class Table {Host, Controller};
	
	template <Table table>
	auto& get()
	{
		if constexpr (table == Table::Host)
			return hosts;
		else if 	 (table == Table::Controller)
			return controllers;
	}
	
	template <Table table>
	using tableType = typename std::remove_reference< decltype(((Database*)nullptr)->get<table>()) >::type;
	template <Table table>
	using recordType = typename tableType<table>::mapped_type;
	
};

static_assert(std::is_same< Database::recordType<Database::Table::Host>, Host >::value);

	
}


