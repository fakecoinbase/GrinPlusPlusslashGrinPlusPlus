#pragma once

#include <json/json.h>
#include <Core/BlockHeader.h>
#include <Core/FullBlock.h>
#include <Core/Transaction.h>
#include <P2P/Peer.h>
#include <P2P/ConnectedPeer.h>

class JSONFactory
{
public:
	static Json::Value BuildBlockJSON(const FullBlock& block);
	static Json::Value BuildHeaderJSON(const BlockHeader& header);

	static Json::Value BuildTransactionBodyJSON(const TransactionBody& body);
	static Json::Value BuildTransactionInputJSON(const TransactionInput& input);
	static Json::Value BuildTransactionOutputJSON(const TransactionOutput& output);
	static Json::Value BuildTransactionKernelJSON(const TransactionKernel& kernel);

	static Json::Value BuildPeerJSON(const Peer& peer);
	static Json::Value BuildConnectedPeerJSON(const ConnectedPeer& connectedPeer);
};