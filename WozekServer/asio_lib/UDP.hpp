#pragma once

#include "asioWrapper.hpp"
#include "asioBufferUtils.hpp"
#include "asyncUtils.hpp"
#include "callbackStack.hpp"

#include <functional>
#include <memory>
#include <iostream>
#include <type_traits>
#include <atomic>

namespace udp
{

template <typename Handler>
class BasicServer
{
protected:
	asio::io_context& ioContext;
	bool running = false;
	
	using HandlerPointer = std::shared_ptr<Handler>;
	
	using Socket = asioudp::socket;
	using Endpoint = asioudp::endpoint;
	
	Socket receiveSocket;
	Endpoint localEndpoint;
	
	virtual bool connectionErrorHandler_impl(const Error& err) = 0;
	virtual bool bindingErrorHandler_impl(const Error& err) = 0;
	//virtual bool authorisationChecker_impl(const asio::ip::tcp::endpoint remote) = 0;
	
public:
	
	BasicServer(asio::io_context& ioContext_)
		: ioContext(ioContext_), receiveSocket(ioContext_)
	{
	}
	
	bool start(uint16_t port)
	{
		if(running)
		{
			return false;
		}
		
		Error err;
		
		receiveSocket.open(asioudp::v4(), err);
		if(err)
		{
			if(bindingErrorHandler_impl(err))
			{
				return false;
			}
		}
		
		receiveSocket.bind(asioudp::endpoint(asioudp::v4(), port), err);
		
		if(err)
		{
			if(bindingErrorHandler_impl(err))
			{
				return false;
			}
		}
		
		std::cout << "SADASD: " << receiveSocket.local_endpoint(err) << '\n';
		
		awaitNewDatagram();
		return true;
	}
	
	auto& getSocket()
	{
		return receiveSocket;
	}
	
private:
	
	void awaitNewDatagram()
	{
		//logger.output("Awaiting connection on endpoint: ", acceptor.local_endpoint());
		auto newHandler = std::make_shared<Handler>(ioContext, receiveSocket);
		receiveSocket.async_receive_from(newHandler->getBuffer(), newHandler->getEndpoint(), [this, newHandler](const Error& err, const size_t bytesTransfered){
			if(err)
			{
				if(connectionErrorHandler_impl(err))
				{
					return;
				}
			}
			else
			{
				newHandler->handle(bytesTransfered);
			}
			awaitNewDatagram();
		});
	}
};


template <typename HandlerImpl, typename BufferImpl, bool DisableSafe> 
class BasicHandler : public AsyncSessionUtils<HandlerImpl, DisableSafe>
{
public:
	using Socket = asioudp::socket;
	using Endpoint = asioudp::endpoint;
	
protected:
	asio::io_context& ioContext;
	
	asioudp::resolver resolver;
	
	Socket& socket;
	Endpoint remoteEndpoint;
	
	BufferImpl buffer;
	size_t bytesTransfered;
	
	CallbackStack callbackStack;
	
	virtual void connectionErrorHandler_impl(const Error& err) = 0;
	virtual void resolutionErrorHandler_impl(const Error& err) = 0;
	virtual void handle_impl() = 0;
	
/// Writes ///
	
	template <typename Buffer, typename SuccessHandler, typename ErrorHandler>
	auto asyncWriteTo(Buffer&& buffer,
					const Endpoint& endpoint,
					SuccessHandler&& successHandler,
					ErrorHandler&& errorHandler)
	{
		return socket.async_send_to(
						buffer,
						endpoint,
						this->errorBranch(
							std::forward<SuccessHandler>(successHandler),
							std::forward<ErrorHandler>(errorHandler)
							)
					);
	}
	template <typename ...Ts, typename SuccessHandler, typename ErrorHandler>
	auto asyncWriteObjectsTo(  const Endpoint& endpoint,
							 SuccessHandler&& successHandler,
							 ErrorHandler&& errorHandler,
							 const Ts& ... ts)
	{
		buffer.saveMultiple(ts...);
		return asyncWriteTo(
						buffer.get(PACKSIZEOF<Ts...>),
						endpoint,
						std::forward<SuccessHandler>(successHandler),
						std::forward<ErrorHandler>(errorHandler)
					);
	}
public:
	
	BasicHandler(asio::io_context& ioContext_, Socket& socket_)
		: ioContext(ioContext_), resolver(ioContext_), socket(socket_)
	{
		pushCallbackStack( [=](CallbackResult::Ptr result){
			return;
		} );
	}
	
	virtual ~BasicHandler(){}
	
	auto getExecutor() {return socket.get_executor();}
	auto& getContext() {return ioContext;}
	
	auto& getSocket() {return socket;}
	
	auto& getEndpoint() { return remoteEndpoint; }
	auto  getBuffer() { return buffer.get(); }
	
	template <typename T>
	void pushCallbackStack(T&& callback) { callbackStack.push( CallbackType(callback) ); }
	
	void returnCallbackStack(std::unique_ptr<CallbackResult> resultPtr)
	{
		if(callbackStack.size() <= 0)
			return;
		
		auto callback = callbackStack.top();
		callbackStack.pop();
		callback( std::move(resultPtr) );
	}
	
	template <typename Result, typename ...Args>
	void returnConstructCallbackStack(Args&& ... args)
	{		
		if(callbackStack.size() <= 0)
			return;
		
		auto callback = callbackStack.top();
		callbackStack.pop();
		callback( std::make_unique<Result>( std::forward<Args>(args)... ));
	}
	
	void returnCallbackDefault(CallbackResult::Status status) { returnConstructCallbackStack<CallbackResult>(status); }
	void returnCallbackGood() { returnCallbackDefault(CallbackResult::Status::Good); }
	void returnCallbackError() { returnCallbackDefault(CallbackResult::Status::Error); }
	void returnCallbackCriticalError() { returnCallbackDefault(CallbackResult::Status::CriticalError); }
	
	void handle(const size_t bytesTransfered) {
		this->bytesTransfered = bytesTransfered;
		asio::post(ioContext, [this, me = this->sharedFromThis()]{ 
			handle_impl();
		});
	};
	
	auto resolve(const std::string_view hostname, const std::string_view port)
	{
		Error ignored;
		return resolver.resolve(hostname, port, ignored);
	}
	
	bool resolveAndConnect(const std::string_view hostname, const std::string_view port)
	{
		Error err;
		auto endpoints = resolve(hostname, port);		
		if(std::size(endpoints) <= 0)
		{
			resolutionErrorHandler_impl(err);
			return false;
		}
		
		asio::connect(socket, endpoints, err);
		if(err)
		{
			connectionErrorHandler_impl(err);
			return false;
		}
		
		remoteEndpoint = socket.remote_endpoint(err);
		return true;
	}
	
	bool connect(const Endpoint& endpoint)
	{
		Error err;
		remoteEndpoint = endpoint;
		return true;
	}
};


}
