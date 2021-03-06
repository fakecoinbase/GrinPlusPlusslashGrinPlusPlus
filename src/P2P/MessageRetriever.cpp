#include "MessageRetriever.h"
#include "ConnectionManager.h"
#include "Messages/MessageHeader.h"

#include <iostream>
#include <P2P/ConnectedPeer.h>
#include <Net/Socket.h>
#include <Net/SocketException.h>
#include <Core/Serialization/ByteBuffer.h>
#include <Core/Serialization/Serializer.h>
#include <Infrastructure/Logger.h>
#include <Infrastructure/ShutdownManager.h>
#include <Common/Util/ThreadUtil.h>
#include <chrono>

MessageRetriever::MessageRetriever(const Config& config, const ConnectionManager& connectionManager)
	: m_config(config), m_connectionManager(connectionManager)
{

}

std::unique_ptr<RawMessage> MessageRetriever::RetrieveMessage(
	Socket& socket,
	const ConnectedPeer& connectedPeer,
	const ERetrievalMode retrievalMode) const
{
	bool hasReceivedData = socket.HasReceivedData();
	if (retrievalMode == BLOCKING)
	{
		std::chrono::time_point timeout = std::chrono::system_clock::now() + std::chrono::seconds(8);
		while (!hasReceivedData)
		{
			if (std::chrono::system_clock::now() >= timeout || ShutdownManagerAPI::WasShutdownRequested())
			{
				return std::unique_ptr<RawMessage>(nullptr);
			}

			ThreadUtil::SleepFor(std::chrono::milliseconds(5), false);
			hasReceivedData = socket.HasReceivedData();
		}
	}

	if (hasReceivedData)
	{
		socket.SetReceiveTimeout(5 * 1000);

		std::vector<unsigned char> headerBuffer(11, 0);
		const bool received = socket.Receive(11, true, headerBuffer);
		if (received)
		{
			ByteBuffer byteBuffer(std::move(headerBuffer));
			MessageHeader messageHeader = MessageHeader::Deserialize(byteBuffer);

			if (!messageHeader.IsValid(m_config))
			{
				throw DESERIALIZATION_EXCEPTION("Message header is invalid");
			}
			else
			{
				if (messageHeader.GetMessageType() != MessageTypes::Ping &&
					messageHeader.GetMessageType() != MessageTypes::Pong)
				{
					LOG_TRACE_F(
						"Retrieved message ({}) from ({})",
						MessageTypes::ToString(messageHeader.GetMessageType()),
						connectedPeer
					);
				}

				std::vector<unsigned char> payload(messageHeader.GetMessageLength());
				const bool bPayloadRetrieved = socket.Receive(messageHeader.GetMessageLength(), false, payload);
				if (bPayloadRetrieved)
				{
					connectedPeer.GetPeer()->UpdateLastContactTime();
					return std::make_unique<RawMessage>(RawMessage(std::move(messageHeader), std::move(payload)));
				}
				else
				{
					throw DESERIALIZATION_EXCEPTION("Expected payload not received");
				}
			}
		}
		else
		{
			LOG_TRACE_F("Failed to receive message from ({})", connectedPeer);
		}
	}

	return std::unique_ptr<RawMessage>(nullptr);
}