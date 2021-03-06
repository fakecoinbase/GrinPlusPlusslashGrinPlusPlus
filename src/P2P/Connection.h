#pragma once

#include "Messages/Message.h"

#include <Common/ConcurrentQueue.h>
#include <BlockChain/BlockChainServer.h>
#include <Net/Socket.h>
#include <P2P/ConnectedPeer.h>
#include <Config/Config.h>
#include <atomic>
#include <queue>

// Forward Declarations
class IMessage;
class ConnectionManager;
class Pipeline;
class HandShake;
class MessageProcessor;
class MessageRetriever;
class MessageSender;

//
// A Connection will be created for each ConnectedPeer.
// Each Connection will run on its own thread, and will watch the socket for messages,
// and will ping the peer when it hasn't been heard from in a while.
//
class Connection
{
public:
	Connection(
		SocketPtr pSocket,
		const uint64_t connectionId,
		ConnectionManager& connectionManager,
		const ConnectedPeer& connectedPeer,
		SyncStatusConstPtr pSyncStatus,
		std::shared_ptr<HandShake> pHandShake,
		const std::weak_ptr<MessageProcessor>& pMessageProcessor,
		std::shared_ptr<MessageRetriever> pMessageRetriever,
		std::shared_ptr<MessageSender> pMessageSender
	);
	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;
	Connection(Connection&&) = delete;
	~Connection();

	static std::shared_ptr<Connection> Create(
		SocketPtr pSocket,
		const uint64_t connectionId,
		const Config& config,
		ConnectionManager& connectionManager,
		IBlockChainServerPtr pBlockChainServer,
		const ConnectedPeer& connectedPeer,
		const std::weak_ptr<MessageProcessor>& pMessageProcessor,
		SyncStatusConstPtr pSyncStatus
	);

	void Disconnect();

	uint64_t GetId() const { return m_connectionId; }
	bool IsConnectionActive() const;

	void Send(const IMessage& message);

	SocketPtr GetSocket() const { return m_pSocket; }
	PeerPtr GetPeer() { return m_connectedPeer.GetPeer(); }
	PeerConstPtr GetPeer() const { return m_connectedPeer.GetPeer(); }
	const ConnectedPeer& GetConnectedPeer() const { return m_connectedPeer; }
	const IPAddress& GetIPAddress() const { return GetPeer()->GetIPAddress(); }
	uint64_t GetTotalDifficulty() const { return m_connectedPeer.GetTotalDifficulty(); }
	uint64_t GetHeight() const { return m_connectedPeer.GetHeight(); }
	Capabilities GetCapabilities() const { return m_connectedPeer.GetPeer()->GetCapabilities(); }

	bool ExceedsRateLimit() const;

private:
	static void Thread_ProcessConnection(std::shared_ptr<Connection> pConnection);

	ConnectionManager& m_connectionManager;
	SyncStatusConstPtr m_pSyncStatus;

	std::shared_ptr<HandShake> m_pHandShake;
	std::weak_ptr<MessageProcessor> m_pMessageProcessor;
	std::shared_ptr<MessageRetriever> m_pMessageRetriever;
	std::shared_ptr<MessageSender> m_pMessageSender;

	std::atomic<bool> m_terminate = true;
	std::thread m_connectionThread;
	const uint64_t m_connectionId;

	ConnectedPeer m_connectedPeer;

	std::shared_ptr<asio::io_context> m_pContext;
	mutable SocketPtr m_pSocket;

	ConcurrentQueue<IMessagePtr> m_sendQueue;
};

typedef std::shared_ptr<Connection> ConnectionPtr;