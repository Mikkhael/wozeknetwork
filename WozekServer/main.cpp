#ifndef TESTRUN

#include "Everything.hpp"


int main()
{
	const size_t numberOfAdditionalThreads = 1;
	
	asio::io_context ioContext;
	
	db::Database database;
	db::databaseManager.setDatabase(database);
	db::databaseManager.createStrand(ioContext);
	
	assert( fileManager.setWorkingDirectory("C:/Users/bondg/Desktop/Dev/WozekNetwork/ServerFiles") );
	fileManager.createStrand(ioContext);
	
	tcp::WozekServer server(ioContext);
	server.start(8081);

	std::vector<std::thread> threadPool;
	threadPool.reserve(numberOfAdditionalThreads);
	for(size_t i=0; i<numberOfAdditionalThreads; i++)
	{
		threadPool.emplace_back( [&ioContext]
		{
			try
			{
				ioContext.run();
			}
			catch(std::exception& e)
			{
				std::cerr << "Cought exception in thread " << std::this_thread::get_id() << "\n " << e.what() << '\n';
			}
			catch(int e)
			{
				std::cerr << "Cought (int) exception in thread " << std::this_thread::get_id() << "\n " << e << '\n';
			}
			std::cout << "Thread " << std::this_thread::get_id() << " ended.\n";
		});
	}
		
	try
	{
			
		ioContext.run();
		std::cout << "Main thread ended.\n";
		
		for(auto& t : threadPool)
		{
			t.join();
		}
		
	}
	catch(std::exception& e)
	{
		std::cerr << "Cought exception in main thread\n " << e.what();
	}
	catch(int e)
	{
		std::cerr << "Cought (int) exception in main thread\n " << e;
	}
	
	std::cout << "Main thread exited\n";
	
	#ifndef RELEASE
	
	std::cin.get();
	
	#endif // RELEASE
}

#endif // TESTRUN
