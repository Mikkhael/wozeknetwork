#pragma once

// #include "asio_lib.hpp"
#include <tuple>
#include <iostream>
#include <optional>
#include <typeinfo>
#include <atomic>
#include <mutex>
#include <shared_mutex>

#include "Datagrams.hpp"

namespace db
{

using IdType = data::IdType;


template <int IdSlots, typename RecordT, typename IdT = IdType>
class TableBase
{
	//static_assert(std::is_trivially_destructible_v<RecordT>);
	
	asio::io_context::strand strand;
	
	std::atomic<IdT> lastFreeIndex = 1;
	
	auto getNextIndex()
	{
		auto newIndex = lastFreeIndex.fetch_add(1, std::memory_order::memory_order_relaxed);
		assert(newIndex <= IdSlots);
		
		return newIndex;
	}
	
public:
	
	struct MutexedRecord
	{
		std::unique_ptr<RecordT> data;
		std::shared_mutex shared_mutex;
	};
	
	std::array<MutexedRecord, IdSlots + 1 > records;
	
	MutexedRecord& getMutexedRecordById(const IdT id)
	{
		assert(id < records.size() && id != 0);
		return records[id];
	}
	
	auto createNewRecord()
	{
		const auto id = getNextIndex();
		records[id].data.reset(new RecordT);
		return id;
	}
	
	template<typename Callback>
	auto accessSafeRead(const IdT id, Callback&& callback)
	{
		static_assert(std::is_invocable_v<Callback, const RecordT*>);
		
		std::shared_lock lock{ getMutexedRecordById(id).shared_mutex };
		return callback(getMutexedRecordById(id).data.get());
	}
	template<typename Callback>
	auto accessSafeWrite(const IdT id, Callback&& callback)
	{
		static_assert(std::is_invocable_v<Callback, RecordT*>);
		
		std::unique_lock lock{ getMutexedRecordById(id).shared_mutex };
		return callback(getMutexedRecordById(id).data.get());
	}
	
	TableBase(asio::io_context& ioContext)
		: strand(ioContext)
	{}
};

template <typename IndexedValueT, typename IdT = IdType>
class IndexBase
{
	std::optional<asio::io_context::strand> strand;
	
	std::shared_mutex shared_mutex;
	
	std::unordered_map<IndexedValueT, IdT> indexMap;
	
public:
	
	void setContext(asio::io_context& context) { strand.emplace(context); }
	bool hasContext() { return strand.has_value(); }
	auto& getStrand() { return strand.value(); }
	
	template<typename Callback>
	void getSafeCallback(const IndexedValueT& value, Callback&& callback)
	{
		asio::post( strand.value(), [this,&callback,value](){
				auto it = indexMap.find(value);
				if(it != indexMap.end())
				{
					callback(it->second);
				}
				callback(0);
			} );
	}
	
	IdT get(const IndexedValueT& value)
	{
		std::shared_lock lock{shared_mutex};
		auto it = indexMap.find(value);
		if(it != indexMap.end())
		{
			return it->second;
		}
		return 0;
	}
	
	bool has(const IndexedValueT& value)
	{
		std::shared_lock lock{shared_mutex};
		return indexMap.find(value) != indexMap.end();
	}
	
	void set(const IdT id, const IndexedValueT& value)
	{
		std::unique_lock lock{shared_mutex};
		indexMap[value] = id;
	}
	
	IndexBase() {};
	virtual ~IndexBase(){};
};
















}




