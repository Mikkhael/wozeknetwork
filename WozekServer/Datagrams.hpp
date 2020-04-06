#pragma once

#include <cstdint>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>

namespace data
{


/// Typedefs ///

using IdType = uint32_t;

template <size_t Size>
using StaticString = char[Size];

template <typename T, size_t Size>
using Vector = T[Size];

template <typename T>
using Vector2 = Vector<T, 2>;

template <typename T>
using Vector3 = Vector<T, 3>;

using Vector2f = Vector2<float>;
using Vector3f = Vector3<float>;

/// Protocols

constexpr char HeartbeatCode = 0x00;
namespace RegisterNewHost
{
	constexpr char Code = 0x01;
	
	constexpr char Failure = 0x00;
	constexpr char Success = 0x01;
}

namespace SegmentedTransfer
{
	constexpr char ErrorCode = 0x00;
	constexpr char ContinueCode = 0x01;
	constexpr char FinishedCode = 0x02;
	
	
	struct SegmentAck
	{
		char code;
	};
	
	struct SegmentHeader
	{
		size_t size;
	};
	
}


namespace UploadMap
{
	constexpr char Code = 0x02;
	
	struct RequestHeader
	{
		IdType hostId;
		size_t totalMapSize;
	};
	struct ResponseHeader 
	{
		constexpr static char DenyAccessCode = 0x00;
		constexpr static char InvalidSizeCode = 0x01;
		constexpr static char AcceptCode = 0x02;
		
		char code;
	};
	
}

namespace DownloadMap
{
	constexpr char Code = 0x03;
	
	struct RequestHeader
	{
		IdType hostId;
	};
	struct ResponseHeader 
	{
		constexpr static char DenyAccessCode = 0x00;
		constexpr static char InvalidSizeCode = 0x01;
		constexpr static char AcceptCode = 0x02;
		
		char code;
		size_t totalMapSize;
	};
	
}


/// Data Structures ///

namespace Host
{
	struct Header
	{
		IdType id;
		StaticString<32> name;
	};
}

namespace Controller
{
	struct Header {
		IdType id;
	};
	struct DesiredState {
		Vector3f position;
		Vector2f orientation;
		Vector2f wheelSpeed;
	};
	struct MeasuredState {
		Vector2f wheelSpeed;
	};
}


}
