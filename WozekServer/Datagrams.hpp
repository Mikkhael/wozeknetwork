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





/// Data Structures ///

namespace Host
{
	struct Header
	{
		IdType id;
		StaticString<32> name;
	};
	
	static_assert(sizeof(Header) == sizeof(Header::id) + sizeof(Header::name));
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



/// Protocols

// TCP

constexpr char HeartbeatCode = 0x00;


static_assert(sizeof(size_t) == 8);

struct EchoRequest
{
	constexpr static IdType request_id = 1;
	// char* - null terminated string of any length up to some MAX length
};

namespace SegmentedFileTransfer
{	
	struct Header
	{
		constexpr static size_t correctStartHeader = 0xAAAAAAAAAAAAAAAA;
		size_t startHeader;
		size_t segmentLength;
	};
	static_assert(sizeof(Header) == sizeof(Header::startHeader) + sizeof(Header::segmentLength) );
	
	enum Error {
		Good = 0,
		FileSystem = 1,
		SegmentTooLong = 2
	};
};


namespace RegisterNewHost
{
	constexpr char Code = 0x01;
	
	using Request = Host::Header;
	
	struct Response
	{
		static constexpr char Failure = 0x00;
		static constexpr char Success = 0x01;
		
		char code;
		IdType id;
	};
	
}

namespace SegmentedTransfer
{
	constexpr char ErrorCode = 0x00;
	constexpr char ContinueCode = 0x01;
	constexpr char FinishedCode = 0x02;
	
	
	struct SegmentAck
	{
		char code = ErrorCode;
	};
	
	struct SegmentHeader
	{
		size_t size;
	};
	
	static_assert(sizeof(SegmentAck)    == 1);
	static_assert(sizeof(SegmentHeader) == 8);
	
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

namespace StartTheWorld
{
	static constexpr char Code = 0x04;
	struct RequestHeader
	{
		unsigned short port;
	};
	
	struct ResponseHeader
	{
		constexpr static char Failure = 0x00;
		constexpr static char Success = 0x01;
		
		char code;
		IdType worldId;
	};
	
};

namespace FileTransfer
{
	namespace Upload
	{
		constexpr static char Code = 0x05;
		struct Request
		{
			StaticString<128> fileName;
			size_t fileSize;
		};
		
		struct Response
		{
			constexpr static char InvalidSizeCode = 0x01;
			constexpr static char InvalidNameCode = 0x02;
			constexpr static char AcceptCode = 0x03;
			
			char code;
		};
		
		static_assert(sizeof(Request)  == 128 + 8);
		static_assert(sizeof(Response) == 1);
	}
	
	namespace Download
	{
		constexpr static char Code = 0x06;
		struct Request
		{
			StaticString<128> fileName;
		};
		
		struct Response
		{
			constexpr static char FileNotFoundCode = 0x01;
			constexpr static char AcceptCode = 0x02;
			
			size_t fileSize;
			char code;
		};
		
		
		static_assert(sizeof(Request)  == 128);
		static_assert(sizeof(Response) == 16);
	}
}


// UDP

struct UpdateHostState
{
	constexpr static char Code = 0x01;
	
	struct Header
	{
		IdType worldId;
		uint64_t timepoint;
		uint64_t controllersCount;
	
		size_t getSizeof() {return sizeof(UpdateHostState) + sizeof(ControllerData) * controllersCount;}
	};
	
	struct ControllerData
	{
		IdType id;
		Controller::DesiredState desiredState;
	};
	
	Header header;
	// ContollerData[]	
};

}
