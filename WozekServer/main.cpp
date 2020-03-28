#include "Everything.hpp"


int main()
{
	asio::io_context ioContext;
	tcp::WozekServer server(ioContext);
	server.start(8081);
	
	
	ioContext.run();
}
