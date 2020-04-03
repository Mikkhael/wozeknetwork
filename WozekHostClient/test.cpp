#ifdef TESTRUN

#include "Everything.hpp"
#include <string>


void runRegisterTest(std::string host, std::string port, std::string name, data::IdType id)
{
	Error error;
	asio::io_context ioContext;
	tcp::Connection connection(ioContext);
	
	std::cout << "Connectcting...\n";
	
	if(!connection.resolveAndConnect(host, port, error))
	{
		std::cout << error;
		return;
	}
	
	data::Host::Header header;
	header.id = id;
	std::memcpy(&header.name, name.data(), sizeof(header.name));
	
	if(!connection.registerAsHost(header, error))
	{
		std::cout << error << '\n';
		return;
	}
	
	std::cout << "Registered as host. Id: " << connection.getId() << '\n';
}

void runRegisterTests()
{
	while(true)
	{
		runRegisterTest("localhost", "8081", "Arkadiusz", 0);
		std::cout << '\n';
	}
}


int main()
{
	runRegisterTests();
	return 0;
}

#endif // TESTRUN
