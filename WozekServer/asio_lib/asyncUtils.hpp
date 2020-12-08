#pragma once

#include "asioWrapper.hpp"


class AsioAsync
{
	static inline asio::io_context* globalContext;
public:
	static void setGlobalAsioContext(asio::io_context& ioContext)
	{
		globalContext = &ioContext;
	}
	
	template <typename T>
	static auto post(T&& token)
	{
		assert( globalContext != nullptr );
		return asio::post( *globalContext, std::forward<T>(token) );
	}
};


template <typename SessionImpl, bool DisableSafe>
class AsyncSessionUtils;

template <typename SessionImpl>
class AsyncSessionUtils<SessionImpl, true>
{
protected:
	
	auto sharedFromThis() { return nullptr; };
	auto getThis() { return static_cast<SessionImpl*>(this); };
	
	template<typename Handler, typename ... Args>
	auto execute(Handler& handler, Args&&... args)
	{
		if constexpr (std::is_member_function_pointer<Handler>::value)
		{
			(getThis()->*handler)( std::forward<Args>(args)... );
		}
		else
		{
			handler( std::forward<Args>(args)... );
		}
	}

	template <typename SuccessHandler, typename ErrorHandler>
	auto errorBranch( SuccessHandler&& successHandler,
					  ErrorHandler&& errorHandler)
	{
		return [=](const Error& err, const size_t length){
			if(err)
				this->execute(errorHandler, err);
			else
				this->execute(successHandler);
		};
	}
	template <typename ContinueHandler, typename SuccessHandler, typename ErrorHandler>
	auto errorBranch( ContinueHandler&& continueHandler, 
					  SuccessHandler&& successHandler,
					  ErrorHandler&& errorHandler)
	{
		return [=](const Error& err, const size_t length){
			this->execute(continueHandler);
			if(err)
				this->execute(errorHandler, err);
			else
				this->execute(successHandler);
		};
	}
	
	template <typename Handler, typename ... Args>
	auto asyncContinue(Handler&& handler, const Args&... args)
	{
		return AsioAsync::post([=]{
							this->execute(handler, args...);
						});
	}
	template <typename Executor, typename Handler, typename ... Args>
	auto asyncContinue(Executor&& executor, Handler&& handler, const Args&... args)
	{
		return asio::post(executor, [=]{
							this->execute(handler, args...);
						});
	}
};


template <typename SessionImpl>
class AsyncSessionUtils<SessionImpl, false> : public std::enable_shared_from_this<SessionImpl>, public AsyncSessionUtils<SessionImpl, true>
{
	
protected:
	
	using Unsafe = AsyncSessionUtils<SessionImpl, true>;
	
	auto sharedFromThis() { 
		#ifndef RELEASE
		std::cout << '{' << this->weak_from_this().use_count() << '}';
		#endif // RELEASE
	return this->shared_from_this();
	};
	
	template <typename SuccessHandler, typename ErrorHandler>
	auto errorBranch( SuccessHandler&& successHandler,
					  ErrorHandler&& errorHandler)
	{
		return [=, me = sharedFromThis()](const Error& err, const size_t length){
			if(err)
				this->execute(errorHandler, err);
			else
				this->execute(successHandler);
		};
	}
	
	template <typename ContinueHandler, typename SuccessHandler, typename ErrorHandler>
	auto errorBranch( ContinueHandler&& continueHandler, 
					  SuccessHandler&& successHandler,
					  ErrorHandler&& errorHandler)
	{
		return [=, me = sharedFromThis()](const Error& err, const size_t length){
			this->execute(continueHandler);
			if(err)
				this->execute(errorHandler, err);
			else
				this->execute(successHandler);
		};
	}
	
	template <typename Handler, typename ... Args>
	auto asyncContinue(Handler&& handler, const Args&... args)
	{
		return AsioAsync::post([=, me = sharedFromThis()]{
							this->execute(handler, args...);
						});
	}
	template <typename Executor, typename Handler, typename ... Args>
	auto asyncContinue(Executor&& executor, Handler&& handler, const Args&... args)
	{
		return asio::post(executor, [=, me = sharedFromThis()]{
							this->execute(handler, args...);
						});
	}
};
