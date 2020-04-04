#ifndef TESTRUN

#include "Everything.hpp"


int main()
{
	try
	{
		
		const size_t numberOfAdditionalThreads = 4;
		
		asio::io_context ioContext;
		
		db::Database database;
		db::databaseManager.setDatabase(database);
		db::databaseManager.createStrand(ioContext);
		
		tcp::WozekServer server(ioContext);
		server.start(8081);

		
		std::vector<std::thread> threadPool;
		threadPool.reserve(numberOfThreads);
		for(size_t i=0; i<numberOfAdditionalThreads; i++)
		{
			threadPool.emplace_back( [&ioContext]
			{
				try
				{
					context.run();
				}
				catch(std::exception& e)
				{
					std::cerr << "Cought exception in thread " << std::thread::get_id() << "\n " << e.what() << '\n';
				}
			});
		}
		
		ioContext.run();
		
		for(auto& t : threadPool)
		{
			t.join();
		}
		
	}
	catch(std::exception& e)
	{
		std::cerr << "Cought exception in main thread\n " << e.what();
	}
	
	#ifndef RELEASE
	
	std::cin.get();
	
	#endif // RELEASE
}

#endif // TESTRUN
