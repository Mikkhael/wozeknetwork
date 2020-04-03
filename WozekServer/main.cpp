#ifndef TESTRUN

#include "Everything.hpp"



int main()
{
	try
	{
		
		asio::io_context ioContext;
		
		db::Database database;
		db::databaseManager.setDatabase(database);
		
		tcp::WozekServer server(ioContext);
		server.start(8081);



		ioContext.run();
	}
	catch(std::exception& e)
	{
		std::cerr << "Cought exception\n " << e.what();
		std::cin.get();
	}
}

#endif // TESTRUN
