#ifndef TCP_HPP_INCLUDED
#define TCP_HPP_INCLUDED

#include "asioWrapper.hpp"
#include "asioBufferUtils.hpp"
#include "asyncUtils.hpp"
#include "callbackStack.hpp"

#include <functional>
#include <memory>
#include <iostream>
#include <type_traits>
#include <atomic>

namespace tcp
{

template <typename Session>
class BasicServer
{
protected:
	asio::io_context& ioContext;
	asiotcp::acceptor acceptor;
	bool running = false;
	
	using SessionPointer = std::shared_ptr<Session>;
	
	virtual bool connectionErrorHandler_impl(const Error& err) = 0;
	virtual bool authorisationChecker_impl(const asio::ip::tcp::endpoint remote) = 0;
	
public:
	
	BasicServer(asio::io_context& ioContext_)
		: ioContext(ioContext_), acceptor(ioContext_)
	{
	}
	
	bool start(uint16_t port)
	{
		if(running)
		{
			return false;
		}
		
		acceptor = asiotcp::acceptor(ioContext, asiotcp::endpoint(asiotcp::v4(), port));
		
		awaitNewConnection();
		return true;
	}
	
private:
	
	void awaitNewConnection()
	{
		//logger.output("Awaiting connection on endpoint: ", acceptor.local_endpoint());
		auto newSession = std::make_shared<Session>(ioContext);
		
		acceptor.async_accept( newSession->getSocket(), [=](const auto& err){
			handleNewSession(newSession, err);
		});
	}
	
	void handleNewSession(SessionPointer session, const Error& err)
	{
		//logger.output("Handleing new connection.");
		if(err && connectionErrorHandler_impl(err))
		{
			// If error handler returned true, stop server
			return;
		}
		
		Error ignore;
		const auto remote = session->getSocket().remote_endpoint(ignore);
		if(authorisationChecker_impl(remote))
		{
			session->start();
		}
		
		awaitNewConnection();
	}
	
};


template <typename SessionImpl, typename BufferImpl, bool DisableSafe> 
class BasicSession : public AsyncSessionUtils<SessionImpl, DisableSafe>
{
public:
	using Socket = asiotcp::socket;
	using Endpoint = asiotcp::endpoint;
	
protected:
	asio::io_context& ioContext;
	
	asiotcp::resolver resolver;
	
	Socket socket;
	Endpoint remoteEndpoint;
	
	asio::steady_timer timeoutTimer;
	asio::steady_timer::duration defaultTimeoutTimerDuration = std::chrono::seconds(35);
	BufferImpl buffer;
	
	CallbackStack callbackStack;
	
	
	virtual void timeoutHandler_impl() = 0;
	virtual bool start_impl() = 0;
	virtual void shutdown_impl() = 0;
	virtual void startError_impl(const Error& err) = 0;
	
	bool isShutdown = true;
	
public:
	
	bool checkIsShutdown() {return isShutdown;}
	
	void shutdownSession()
	{
		if(isShutdown)
			return;
		isShutdown = true;
		
		stopTimeoutTimer();
		
		Error ignored;
		socket.shutdown(Socket::shutdown_type::shutdown_both, ignored);
		socket.close(ignored);
		
		shutdown_impl();
	}
	
	/// Timer
	
	void startTimeoutTimer(const asio::steady_timer::duration& duration)
	{
		timeoutTimer.expires_after(duration);
		timeoutTimer.async_wait([=, me = this->sharedFromThis()](const Error& err){
				if(err || isShutdown)
				{
					return;
				}
				
				timeoutHandler_impl();
				shutdownSession();
			});
	}
	
	void stopTimeoutTimer()
	{
		timeoutTimer.cancel();
	}
	
protected:
	
	/// Async
	
	template <typename Buffer, typename SuccessHandler, typename ErrorHandler>
	auto asyncRead( Buffer&& buffer,
					SuccessHandler&& successHandler,
					ErrorHandler&& errorHandler)
	{
		return asyncRead(
				std::forward<Buffer>(buffer),
				std::forward<SuccessHandler>(successHandler),
				std::forward<ErrorHandler>(errorHandler),
				defaultTimeoutTimerDuration
		);
	}
	template <typename Buffer, typename SuccessHandler, typename ErrorHandler>
	auto asyncRead( Buffer&& buffer,
					SuccessHandler&& successHandler,
					ErrorHandler&& errorHandler,
					asio::steady_timer::duration timeoutDuration)
	{
		startTimeoutTimer(timeoutDuration);
		return asio::async_read(
						socket,
						buffer,
						this->errorBranch(
							&SessionImpl::stopTimeoutTimer,
							std::forward<SuccessHandler>(successHandler),
							std::forward<ErrorHandler>(errorHandler)
							)
					);
	}
	template <typename ...Ts, typename SuccessHandler, typename ErrorHandler>
	auto asyncReadObjects(  SuccessHandler&& successHandler,
							ErrorHandler&& errorHandler)
	{
		return asyncReadObjects<Ts...>(
			std::forward<SuccessHandler>(successHandler),
			std::forward<ErrorHandler>(errorHandler),
			defaultTimeoutTimerDuration
		);
	}
	template <typename ...Ts, typename SuccessHandler, typename ErrorHandler>
	auto asyncReadObjects(  SuccessHandler&& successHandler,
							ErrorHandler&& errorHandler,
							asio::steady_timer::duration timeoutDuration)
	{
		return asyncRead(
						buffer.get(PACKSIZEOF<Ts...>),
						[=]{
							auto t = std::tuple<Ts...>();
							std::apply([=](auto&... args){buffer.loadMultiple(args...);}, t);
							std::apply([this, me = this->sharedFromThis(), successHandler](auto&... args)
											{
												this->execute(successHandler, args...);
											}, t);
						},
						std::forward<ErrorHandler>(errorHandler),
						timeoutDuration
					);
	}
   	
	template <typename Buffer, typename SuccessHandler, typename ErrorHandler>
	auto asyncWrite(Buffer&& buffer,
					SuccessHandler&& successHandler,
					ErrorHandler&& errorHandler)
	{
		return asio::async_write(
						socket,
						buffer,
						this->errorBranch(
							std::forward<SuccessHandler>(successHandler),
							std::forward<ErrorHandler>(errorHandler)
							)
					);
	}
	template <typename ...Ts, typename SuccessHandler, typename ErrorHandler>
	auto asyncWriteObjects(  SuccessHandler&& successHandler,
							 ErrorHandler&& errorHandler,
							 const Ts& ... ts)
	{
		buffer.saveMultiple(ts...);
		return asyncWrite(
						buffer.get(PACKSIZEOF<Ts...>),
						std::forward<SuccessHandler>(successHandler),
						std::forward<ErrorHandler>(errorHandler)
					);
	}
public:
	
	BasicSession(asio::io_context& ioContext_)
		: ioContext(ioContext_), resolver(ioContext_), socket(ioContext_), timeoutTimer(ioContext_)
	{
		pushCallbackStack( [=](CallbackResult::Ptr result){
			shutdownSession();
		} );
	}
	
	virtual ~BasicSession(){
	}
	
	auto getExecutor() {return socket.get_executor();}
	auto& getContext() {return ioContext;}
	
	auto& getSocket() {return socket;}
	auto getRemote() {return remoteEndpoint;}
	
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
	
	bool start()
	{
		shutdownSession();
		
		Error err;
		remoteEndpoint = socket.remote_endpoint(err);
		if(err)
		{
			/*
			logger.output("Unknown error while connecting to the remote endpoint: ", err);
			logger.error(Logger::Error::TcpUnknownError);
			*/
			startError_impl(err);
			return false;
		}
		
		isShutdown = false;
		return start_impl();
	}
	
	bool connect(const Endpoint& endpoint)
	{
		shutdownSession();
		
		Error err;
		socket.connect(endpoint, err);
		if(err)
		{
			startError_impl(err);
			return false;
		}
		remoteEndpoint = endpoint;
		isShutdown = false;
		return start_impl();
	}
	
	bool resolveAndConnect(const std::string_view hostname, const std::string_view port)
	{
		shutdownSession();
		
		Error err;
		
		auto remotes = this->resolver.resolve(hostname, port, err);
		if(err)
		{
			startError_impl(err);
			return false;
		}
		
		asio::connect(socket, remotes, err);
		if(err)
		{
			startError_impl(err);
			return false;
		}
		
		remoteEndpoint = socket.remote_endpoint(err);
		if(err)
		{
			startError_impl(err);
			return false;
		}
		
		isShutdown = false;
		return start_impl();
	}
};


}

#endif // TCP_HPP_INCLUDED
