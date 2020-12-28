#pragma once

#include "asio_lib.hpp"
#include "DatabaseBase.hpp"




namespace db
{



struct ControllerRecord
{
	std::string name = "";
	
	asioudp::endpoint endpoint;
	
	struct Rotation
	{
		data::RotationType X = 0, Y = 0, Z = 0;
	} rotation;
	
};

using ControllerTable = TableBase<100, ControllerRecord>;
using ControllerNameIndex = IndexBase<std::string>;

/*


class ControllerTable
	: public TableBase<100, ControllerRecord>
{
public:
	
	ControllerTable(asio::io_context& ioContext)
		: TableBase(ioContext)
	{}
};

*/














}



