#ifndef TESTRUN

#include "Everything.hpp"


int main(int argc, char** argv)
{
	
	#ifdef RELEASE
	
	if(argc < 4)
	{
		std::cout << "Use the parameters [port] [additional_threads] [directory]";
		return 0;
	}
	
	short port = std::atoi(argv[1]);
	short threads = std::atoi(argv[2]);
	std::string dir = argv[3];
	
	#else
	
	short port = 8081;
	short threads = 1;
	std::string dir = "ServerFiles";
	
	#endif // RELEASE
	
	const size_t numberOfAdditionalThreads = threads;
	
	asio::io_context ioContext;
	
	const fs::path logsPath = fs::path(dir) / "logs";
	const fs::path configPath = fs::path(dir) / "config";
	
	if(!fs::exists(logsPath))
	{
		if(!fs::create_directory(logsPath))
		{
			std::cout << "Cannot create directory for logs\n";
			return 0;
		}
	}
	
	if(!fs::exists(configPath))
	{
		if(!fs::create_directory(configPath))
		{
			std::cout << "Cannot create directory for config\n";
			return 0;
		}
	}
	
	logger.setStrand(ioContext);
	if(!logger.init(logsPath / "output.txt", logsPath / "error.txt", logsPath / "log.txt", std::chrono::seconds(30)))
	{
		std::cout << "Cannot initialize logger\n";
		return 0;
	}
	
	if(!logger.isErrorFileOpen() || !logger.isLogFileOpen() || !logger.isOutputFileOpen())
	{
		std::cout << "Cannot create/open files required for logging\n";
		return 0;
	}
	
	config.setStrand(ioContext);
	if(!config.init(configPath / "allowedips4.txt", std::chrono::seconds(3600)))
	{
		std::cout << "Cannot inicialize config\n";
		return 0;
	}
	
	db::Database database;
	db::databaseManager.setDatabase(database);
	db::databaseManager.createStrand(ioContext);
	
	if( !fileManager.setWorkingDirectory(dir) )
	{
		std::cout << "Working directory of \"" << dir << "\" (" << fs::absolute(fs::path(dir)) << ") dosent exist\n";
		return 0;
	}
	fileManager.createStrand(ioContext);
	
	tcp::WozekServer server(ioContext);
	if(server.start(port))
	{
		std::cout << "TCP Server started on port " << port << '\n';
	}
	
	udp::UdpWozekStateServer udpStateServer(ioContext);
	if(udpStateServer.start(port))
	{
		std::cout << "UDP State Server started on port " << port << '\n';
	}

	std::vector<std::thread> threadPool;
	threadPool.reserve(numberOfAdditionalThreads);
	for(size_t i=0; i<numberOfAdditionalThreads; i++)
	{
		threadPool.emplace_back( [&ioContext]
		{
			try
			{
				std::cout << "Running on thread " << std::this_thread::get_id() << '\n';
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
			
		std::cout << "Rinning on main thread\n";
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
