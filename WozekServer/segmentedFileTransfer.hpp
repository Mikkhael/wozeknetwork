#include "asioWrapper.hpp"
#include "Datagrams.hpp"



namespace segFileTransfer
{
	template<typename HeaderConsumer, typename Sender, typename AckHandler>
	void sendFile(const size_t remainingFileSize, const size_t maxSegmentLength, HeaderConsumer headerConsumer, Sender sender, AckHandler ackHandler)
	{
		data::SegmentedTransfer::SegmentHeader header;
		header.size = std::min(remainingFileSize, maxSegmentLength);
		headerConsumer(header, [=]
		{
			sender(true, header.size, [=](const size_t sentBytes){
				assert( header.size >= sentBytes );
				sendSegmentData(header.size - sentBytes, sender, [=]{
					ackHandler([=]{
						assert( remainingFileSize > header.size );
						sendFile(remainingFileSize - header.size, maxSegmentLength, headerConsumer, sender, ackHandler);
					});
				});
			});
		});
	}
	
	template<typename Sender, typename Callback>
	void sendSegmentData(const size_t remainingSegmentSize, Sender sender, Callback callback)
	{
		if(remainingSegmentSize == 0)
		{
			callback();
			return;
		}
		
		sender(false, remainingSegmentSize, [=](const size_t sentBytes)
		{ 
			assert( remainingSegmentSize >= sentBytes );
			sendSegmentData(remainingSegmentSize - sentBytes, sender, callback);
		});
	}
	
	
	
	template<typename HeaderReceiver, typename DataReceiver, typename HeaderVerifier, typename AckSender>
	void readFile(const size_t remainingFileSize, HeaderReceiver headerReceiver, DataReceiver dataReceiver, HeaderVerifier headerVerifier, AckSender ackSender)
	{
		static constexpr size_t headerSize = sizeof(data::SegmentedTransfer::SegmentHeader);
		
		headerReceiver([=](const char* receivedBytes)
		{
			data::SegmentedTransfer::SegmentHeader header;
			std::memcpy(&header, receivedBytes, headerSize);
			
			headerVerifier(header, [=]
			{
				readSegmentData(header.size, dataReceiver, [=]
				{
					ackSender([=]
						{ readFile(remainingFileSize - header.size, headerReceiver, dataReceiver, headerVerifier, ackSender); } );
				});
			});
		});
	}
	
	template<typename DataReceiver, typename Callback>
	void readSegmentData(const size_t remainingSegmentSize, DataReceiver dataReceiver, Callback callback)
	{
		if(remainingSegmentSize == 0)
		{
			callback();
			return;
		}
		dataReceiver(remainingSegmentSize, [=](const size_t receivedDataLength){
			readSegmentData(remainingSegmentSize - receivedDataLength, dataReceiver, callback);
		});
	}
	
}
