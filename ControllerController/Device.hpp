#pragma once

#include "utils.hpp"

#include <bitset>
#include "seriallib.hpp"

using byte = unsigned char;


inline constexpr void lookupBytes(const byte* buffor, const int length)
{
	for(int i=0; i<length; i++)
	{
		if(buffor[i] < 16)
			std::clog << '0';
		std::clog << std::hex << static_cast<unsigned int>(buffor[i]) << ' ';
	}
}

inline constexpr void lookupBits(const byte* data, const int length)
{
	for(int i=0; i<length; i++)
	{
		std::clog << std::bitset<8>(data[i]) << ' ';
	}
}

inline const char *comports[33]   =       {"", "\\\\.\\COM1",  "\\\\.\\COM2",  "\\\\.\\COM3",  "\\\\.\\COM4",
										"\\\\.\\COM5",  "\\\\.\\COM6",  "\\\\.\\COM7",  "\\\\.\\COM8",
										"\\\\.\\COM9",  "\\\\.\\COM10", "\\\\.\\COM11", "\\\\.\\COM12",
										"\\\\.\\COM13", "\\\\.\\COM14", "\\\\.\\COM15", "\\\\.\\COM16",
										"\\\\.\\COM17", "\\\\.\\COM18", "\\\\.\\COM19", "\\\\.\\COM20",
										"\\\\.\\COM21", "\\\\.\\COM22", "\\\\.\\COM23", "\\\\.\\COM24",
										"\\\\.\\COM25", "\\\\.\\COM26", "\\\\.\\COM27", "\\\\.\\COM28",
										"\\\\.\\COM29", "\\\\.\\COM30", "\\\\.\\COM31", "\\\\.\\COM32"
};

inline const char* getPortName(const int portNumber) 
{ 
	return comports[portNumber];
}

inline constexpr int getEncodedLength(const int dataLength, const int opcodeSize = 3) { return (8*dataLength + opcodeSize - 1)/7 + 1; }

template <int dataLength, int byteIndex, int currentOffset> 
void encodeStep(const byte* data, byte* resBuffer)
{
	if constexpr (currentOffset == 0)
	{
		*(resBuffer+1) = 0;
		encodeStep<dataLength, byteIndex, 7>(data, resBuffer+1);
	}
	else
	{
		*(resBuffer) |= (data[byteIndex] >> (8 - currentOffset));
		*(resBuffer+1) = byte(0) | ((data[byteIndex] << (currentOffset - 1)) & byte(0x7F) );
		if constexpr (byteIndex < dataLength - 1) {
			encodeStep<dataLength, byteIndex+1, currentOffset - 1>(data, resBuffer+1);
		}
	}
}

template <byte opcode, int dataLength, int opcodeSize = 3>
void encode(const byte* data, byte* resBuffer)
{
	constexpr byte shiftedOpcode = (opcode << (7 - opcodeSize) ) | 0x80;
	resBuffer[0] = shiftedOpcode;
	if constexpr (dataLength > 0) {
		encodeStep< dataLength, 0, 7 - opcodeSize >(data, resBuffer);
	}
}

template<int opcodeSize = 3>
inline void encode(byte opcode, const int dataLength, const byte* data, byte* resBuffer)
{
	resBuffer[0] = (opcode << (7 - opcodeSize) ) | 0x80;
	if(dataLength <= 0) {
		return;
	}
	int currentOffset = 7 - opcodeSize;
	int resIndex = 0;
	int byteIndex = 0;
	do{
		if (currentOffset == 0){
			resBuffer[++resIndex] = (data[byteIndex] >> 1);
			currentOffset = 7;
		}
		else{
			resBuffer[resIndex] |= (data[byteIndex] >> (8 - currentOffset));
		}
		resBuffer[++resIndex] = byte(0) | ((data[byteIndex] << (--currentOffset)) & byte(0x7F) );
	}while(++byteIndex < dataLength);
}

template <int opcodeSize = 3>
void decode(const int resLength, const byte* data, byte* resBuffer)
{
	int currentOffset = 7 - opcodeSize;
	int dataIndex = 0;
	int resIndex = 0;
	do{
	if (currentOffset == 0)
	{
	  ++dataIndex;
	  currentOffset = 7;
	}
	resBuffer[resIndex]  = (data[dataIndex++] << (8 - currentOffset));
	resBuffer[resIndex] |= (data[dataIndex] >> (--currentOffset) );
	}while(++resIndex < resLength);
}

class Device
{
	
	int port = 0;
	int bdrate = 9600;
	const char* portName;
	
public:
	
	serialib serialPort;
	
	int lastError = 0;
	
	Device(int port = 0, int bdrate_ = 9600)
		: bdrate(bdrate_)
	{
		setPort(port);
	}
	
	auto getPort() {return port;}
	
	bool setPort(int port)
	{
		if(port < 0 || port > 32)
		{
			this->port = 0;
			this->portName = getPortName(0);
			return false;
		}
		this->port = port;
		this->portName = getPortName(port);
		return true;
	}
	void setBdRate(const int bdrate_)
	{
		bdrate = bdrate_;
	}
	
	bool connect()
	{
		return (lastError = serialPort.openDevice(portName, bdrate)) > 0;
	}
	
	void disconnect()
	{
		serialPort.closeDevice();
	}
	
	void flush()
	{
		serialPort.flushReceiver();
	}
	
	constexpr static int DefaultTimeout  = 30;
	static constexpr int ReadBufferSize  = 512;
	static constexpr int WriteBufferSize = 512;
	byte readBuffer[ReadBufferSize] {};
	byte writeBuffer[WriteBufferSize] {};
	int readBytes = 0;
	
	bool postRawData(const byte* data, const int length)
	{
		return serialPort.writeBytes(data, length) > 0;
	}
	int postRawDataAndAwaitResponse(const byte* data, const int length, const int expectedResponseLength, const int timeout = DefaultTimeout)
	{
		flush();
		lookupBytes(data, length);
		int res = serialPort.writeBytes(data,length);
		if(res < 0)
		{
			return -1;
		}
		res = serialPort.readBytes(readBuffer, expectedResponseLength, timeout);
		if(res < 0)
		{
			return -2;
		}
		return res;
	}
	
	template <int opcode, int length> 
	bool postEncodedRequest(const byte* data)
	{
		encode<opcode, length>(data, writeBuffer);
		return serialPort.writeBytes(writeBuffer, getEncodedLength(length)) > 0;
	}
	
	template <int opcode, int length>
	int postEncodedRequestAndAwaitResponse(const byte* data, const int expectedResponseLength = ReadBufferSize)
	{
		encode<opcode, length>(data, writeBuffer);
		flush();
		int res = serialPort.writeBytes(writeBuffer, getEncodedLength(length));
		if(res < 0)
		{
			return -1;
		}
		res = serialPort.readBytes(readBuffer, expectedResponseLength, DefaultTimeout);
		if(res < 0)
		{
			return -2;
		}
		return res;
	}
	
	int postEncodedRequestAndAwaitResponse(const int opcode, const byte* data, const int length, const int expectedResponseLength = ReadBufferSize)
	{
		encode(opcode, length, data, writeBuffer);
		flush();
		lookupBytes(writeBuffer, getEncodedLength(length));
		int res = serialPort.writeBytes(writeBuffer, getEncodedLength(length));
		if(res < 0)
		{
			return -1;
		}
		res = serialPort.readBytes(readBuffer, expectedResponseLength, DefaultTimeout);
		if(res < 0)
		{
			return -2;
		}
		return res;
	}
};
