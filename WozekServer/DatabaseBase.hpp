#pragma once

// #include "asio_lib.hpp"
#include <tuple>
#include <iostream>
#include <optional>
#include <typeinfo>
#include <atomic>

#include "Datagrams.hpp"

namespace db
{

using IdType = data::IdType;


template <int IdSlots, typename RecordT, typename IdT = IdType>
class TableBase
{
	static_assert(std::is_trivially_destructible_v<RecordT>);
	
	asio::io_context::strand strand;
	
	std::atomic<IdT> lastFreeIndex = 1;
	
	auto getNextIndex()
	{
		auto newIndex = lastFreeIndex.fetch_add(1, std::memory_order::memory_order_relaxed);
		assert(newIndex <= IdSlots);
		
		return newIndex;
	}
	
public:
	
	std::array<std::unique_ptr<RecordT>, IdSlots + 1 > records;
	
	RecordT* getRecordById(const IdT id)
	{
		assert(id < records.size() && id != 0);
		auto& r = records[id];
		return r.get();
	}
	
	auto createNewRecord()
	{
		std::pair< RecordT* , IdT > res;
		res.second = getNextIndex();
		res.first = records[res.second].reset(new RecordT).get();
		return res;
	}
	
	template<typename Callback>
	void postOnStrand(Callback&& callback)
	{
		asio::post( strand, callback );
	}
	
	TableBase(asio::io_context& ioContext)
		: strand(ioContext)
	{}
};

template <typename IndexedValueT, typename IdT = IdType>
class IndexBase
{
	std::optional<asio::io_context::strand> strand;
	
	std::unordered_map<IndexedValueT, IdT> indexMap;
	
public:
	
	void setContext(asio::io_context& context) { strand.emplace(context); }
	bool hasContext() { return strand.has_value(); }
	auto& getStrand() { return strand.value(); }
	
	template<typename Callback>
	void postOnStrand(Callback&& callback)
	{
		asio::post( strand.value(), callback );
	}
	
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
	
	IndexBase() {};
	virtual ~IndexBase(){};
};
















}




