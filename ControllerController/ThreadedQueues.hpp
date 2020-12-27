#pragma once

#include <atomic>
#include <memory>
#include <mutex>
#include <optional>
#include "utils.hpp"

template <typename T>
class MutexCopyQueue
{
	struct Node
	{
		std::optional<T> value;
		std::shared_ptr<Node> next;		
	};
	
	std::atomic<bool> empty = true;
	
	std::shared_ptr<Node> tail = std::make_shared<Node>();
	std::shared_ptr<Node> head = tail;
	
	std::mutex mutex;
	
public:
	
	void push(T value)
	{
		std::lock_guard<std::mutex> g{mutex};
		tail->value = value;
		tail = tail->next = std::make_shared<Node>();
		empty.store(false, order_relaxed);
	}
	
	void pop()
	{
		std::lock_guard<std::mutex> g{mutex};
		if(!head->value)
		{
			return;
		}
		if((head = head->next) == tail)
		{
			empty.store(true, order_relaxed);
		}
	}
	
	std::optional<T> topAndPop()
	{
		std::lock_guard<std::mutex> g{mutex};
		
		std::optional<T> result = head->value;
		if(result)
		{
			if((head = head->next) == tail)
			{
				empty.store(true, order_relaxed);
			}
		}
		return result;
	}
	
	bool isEmpty()
	{
		return empty.load(order_relaxed);
	}
	
	std::optional<T> top()
	{
		std::lock_guard<std::mutex> g{mutex};
		return head->value;
	}
	
};

