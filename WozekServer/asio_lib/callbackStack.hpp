#pragma once
#include <functional>
#include <stack>


struct CallbackResult
{
	enum class Status {Good, Error, CriticalError};
	using Ptr = std::unique_ptr<CallbackResult>;
	
	Status status = Status::Good;
	
	CallbackResult(Status status_ = Status::Good)
		: status(status_)
	{}
	
	bool isCritical()
	{
		return status == Status::CriticalError;
	}
	
	virtual ~CallbackResult() {};
};

using CallbackType = std::function< void(CallbackResult::Ptr) >;
using CallbackStack = std::stack<CallbackType>;




template <typename T>
struct ValueCallbackResult : public CallbackResult
{
	using Ptr = std::unique_ptr<ValueCallbackResult>;
	T value;
	
	ValueCallbackResult(Status status_ = Status::Good)
		: CallbackResult(status_)
	{}
};
