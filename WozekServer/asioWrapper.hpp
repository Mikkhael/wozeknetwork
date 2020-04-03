#pragma once

#include <iostream>
#include <boost/asio.hpp>

namespace asio = boost::asio;
using Error = boost::system::error_code;

using asiotcp = asio::ip::tcp;
using asioudp = asio::ip::udp;


inline std::ostream& operator<<(std::ostream& os, const Error& error)
{
	return os << '(' << error.value() << ") " << error.message();
}
