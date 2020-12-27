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
		data::RotationType X, Y, Z;
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



