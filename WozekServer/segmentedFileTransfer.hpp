#include "asioWrapper.hpp"
#include "Datagrams.hpp"



namespace segFileTransfer
{
	template<typename Sender, typename DataProvider, typename AckHandler>
	void sendFile(const size_t remainingFileSize, const size_t maxSegmentLength, Sender sender, DataProvider dataProvider, AckHandler ackHandler)
	{
		static constexpr size_t headerSize = sizeof(data::SegmentedTransfer::SegmentHeader);
	
		dataProvider(remainingFileSize, [=](const size_t availableDataLength, const char* dataToWrite)
		{
			data::SegmentedTransfer::SegmentHeader header;
			header.size = std::min(availableDataLength, maxSegmentLength);
			sender(header, header.size, dataToWrite, [=](const size_t totalBytesSent)
			{
				ackHandler(totalBytesSent, [=]()
				{
					sendFile(remainingFileSize - header.size, maxSegmentLength, sender, dataProvider, ackHandler);
				});
			});
		});
	}
	
	template<typename Receiver, typename DataConsumer, typename AckSender, typename HeaderVerifier>
	void readFile(const size_t remainingFileSize, Receiver receiver, DataConsumer dataConsumer, AckSender ackSender, HeaderVerifier headerVerifier)
	{
		static constexpr size_t headerSize = sizeof(data::SegmentedTransfer::SegmentHeader);
		
		receiver(true, headerSize, [=](const char* receivedBytes, const size_t receivedBytesLength)
		{
			data::SegmentedTransfer::SegmentHeader header;
			std::memcpy(&header, receivedBytes, headerSize);
			
			headerVerifier(header, remainingFileSize, [=]
			{
				readSingleSegment(header.size, receiver, dataConsumer, [=]
				{
					ackSender(remainingFileSize, header.size, [=, newRemainingFileSize = remainingFileSize - header.size]()
					{
						readFile(newRemainingFileSize, receiver, dataConsumer, ackSender, headerVerifier);
					});
				});
			});
		});
	}
	
	template<typename Receiver, typename DataConsumer, typename Callback>
	static void readSingleSegment(const size_t remainingSegmentSize, Receiver receiver, DataConsumer dataConsumer, Callback callback)
	{
		
		receiver(false, remainingSegmentSize, [=](const char* receivedBytes, const size_t receivedBytesLength)
		{
			dataConsumer(receivedBytes, receivedBytesLength, [=]()
			{
				if(remainingSegmentSize - receivedBytesLength > 0)
				{
					readSingleSegment(remainingSegmentSize - receivedBytesLength, receiver, dataConsumer, callback);
					return;
				}
				
				callback();
			});
		});
	}
	
}
