#include "TCPWozekServer.hpp"

namespace tcp
{
	
void WozekConnectionHandler::receiveSomeMessage(size_t)
{
	std::cout << "Receiving message from " << getRemote() << "...\n";
	socket.async_read_some(asio::buffer(messageBuffer), asyncBranch(sendMessage));
}

void WozekConnectionHandler::sendMessage(size_t length )
{
	refreshTimeout();
	std::cout << "Received message from " << getRemote() << " | length: " << length << '\n';
	std::cout << "Sending message to " << getRemote() << "...\n";
	asio::async_write(socket, asio::buffer(messageBuffer, length), asyncBranch(
		[=](size_t length = 0)
		{
			refreshTimeout();
			std::cout << "Sent message to " << getRemote() << '\n';
			receiveSomeMessage(length);
		}, logAndAbortErrorHandler)
	);
}






void WozekConnectionHandler::timeoutHandler()
{
	std::cout << "Socket " << getRemote() << " timed out\n";
}

void WozekConnectionHandler::shutdownHandler()
{
	std::cout << "Socket " << getRemote() << " shut down\n";
}
	
}

