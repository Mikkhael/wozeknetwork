#pragma once

#include "asio_lib.hpp"
#include "DatabaseBase.hpp"




namespace db
{



struct ControllerRecord
{
	asioudp::endpoint endpoint;
	struct Rotation
	{
		data::RotationType X = 0, Y = 0, Z = 0;
	} rotation;
	
};

class ControllerTable
	: public TableBase<100, ControllerRecord>
{
public:
	
	ControllerTable(asio::io_context& ioContext)
		: TableBase(ioContext)
	{}
};
















}



