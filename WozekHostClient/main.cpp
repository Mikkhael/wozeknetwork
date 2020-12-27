#ifdef DLL
static_assert(false);
#endif // DLL

#include "Everything.hpp"

#include <iostream>
#include <thread>
#include "commander.hpp"


int main()
{
	
	asio::io_context ioContext;
	AsioAsync::setGlobalAsioContext(ioContext);
	
	Commander commander(ioContext);
	commander.start();
	
	
	std::thread t([&]{ioContext.run(); std::cout << "Additional thread ended\n";});
	try
	{
		ioContext.run();
	}
	catch(std::exception& e)
	{
		std::cout << "!!!!! Exception: !!!!!!\n" << e.what() << '\n';
	}
	
	
	
	std::cout << "Main thread ended\n";
}
