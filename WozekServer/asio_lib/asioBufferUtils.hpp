#pragma once

#include "asioWrapper.hpp"
#include <algorithm>



template <typename Head, typename ...Tail>
constexpr static size_t PACKSIZEOF_impl()
{
	if constexpr ( sizeof...(Tail) == 0)
		return sizeof(Head);
	else
		return sizeof(Head) + PACKSIZEOF_impl<Tail...>();
}

template <typename ...Tail>
constexpr size_t PACKSIZEOF = PACKSIZEOF_impl<Tail...>();


template <size_t BufferSize>
class BasicBuffer
{
	template<size_t TotalSizeAcc, typename T, typename ...Ts>
	size_t loadMultiple_impl(const size_t offset, T& object, Ts&... rest)
	{
		return loadMultiple_impl<TotalSizeAcc + sizeof(T)>( loadObjectAt(offset, object), rest... );
	}
	template<size_t TotalSizeAcc, typename T>
	size_t loadMultiple_impl(const size_t offset, T& object)
	{
		static_assert(TotalSizeAcc <= BufferSize);
		return loadObjectAt(offset, object);
	}
	
	template<size_t TotalSizeAcc, typename T, typename ...Ts>
	size_t saveMultiple_impl(const size_t offset, T& object, Ts&... rest)
	{
		return saveMultiple_impl<TotalSizeAcc + sizeof(T)>( saveObjectAt(offset, object), rest... );
	}
	template<size_t TotalSizeAcc, typename T>
	size_t saveMultiple_impl(const size_t offset, T& object)
	{
		static_assert(TotalSizeAcc <= BufferSize);
		return saveObjectAt(offset, object);
	}
	
protected:
	
public:
	virtual void loadBytes(char* to, size_t length) = 0;
	virtual size_t loadBytesAt(const size_t offset, char* to, size_t length) = 0;
	virtual void saveBytes(char* from, size_t length) = 0;
	virtual size_t saveBytesAt(const size_t offset, char* from, size_t length) = 0;
	virtual asio::mutable_buffer get(const size_t length) = 0;
	virtual asio::mutable_buffer getAt(const size_t offset, const size_t length) = 0;
	
	
	template<typename T>
	void loadObject(T& object)
	{
		static_assert( sizeof(T) <= BufferSize );
		loadBytes((char*)&object, sizeof(T));
	}
	template<typename T>
	size_t loadObjectAt(const size_t offset, T& object)
	{
		static_assert( sizeof(T) <= BufferSize );
		return loadBytesAt(offset, (char*)&object, sizeof(T));
	}
	template <typename ...Ts>
	size_t loadMultiple(Ts&... ts)
	{
		return loadMultiple_impl<0>(0, ts...);
	}
	template <typename ...Ts>
	size_t loadMultipleAt(const size_t offset, Ts&... ts)
	{
		return loadMultiple_impl<0>(offset, ts...);
	}
	
	
	template<typename T>
	void saveObject(const T& object)
	{
		static_assert( sizeof(T) <= BufferSize );
		return saveBytes((char*)&object, sizeof(T));
	}
	template<typename T>
	size_t saveObjectAt(const size_t offset, const T& object)
	{
		static_assert( sizeof(T) <= BufferSize );
		return saveBytesAt(offset, (char*)&object, sizeof(T));
	}
	template <typename ...Ts>
	size_t saveMultiple(Ts&... ts)
	{
		return saveMultiple_impl<0>(0, ts...);
	}
	template <typename ...Ts>
	size_t saveMultipleAt(const size_t offset, Ts&... ts)
	{
		return saveMultiple_impl<0>(offset, ts...);
	}
	
};


template <size_t BufferSize>
class ArrayBuffer : public BasicBuffer<BufferSize>
{
	std::array<char, BufferSize> buffer = {0};
	
public:
	
	void loadBytes(char* to, size_t length)
	{
		assert( length <= BufferSize );
		std::memcpy(to, buffer.data(), length);
	}
	size_t loadBytesAt(const size_t offset, char* to, size_t length)
	{
		assert(length + offset <= BufferSize);
		std::memcpy(to, buffer.data() + offset, length);
		return offset + length;
	}
	
	void saveBytes(char* from, size_t length)
	{
		assert( length <= BufferSize );
		std::memcpy(buffer.data(), from, length);
	}
	size_t saveBytesAt(const size_t offset, char* from, size_t length)
	{
		assert(length + offset <= BufferSize);
		std::memcpy(buffer.data() + offset, from, length);
		return offset + length;
	}
	
	asio::mutable_buffer get(const size_t length)
	{
		return asio::buffer(buffer, length);
	}
	asio::mutable_buffer getAt(const size_t offset, const size_t length)
	{
		return asio::buffer(buffer.data() + offset, length);
	}
};

