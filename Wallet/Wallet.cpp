#include "Wallet.h"
#include "WalletRefresher.h"
#include "Keychain/KeyChain.h"

Wallet::Wallet(const Config& config, const INodeClient& nodeClient, IWalletDB& walletDB, const std::string& username, KeyChainPath&& userPath)
	: m_config(config), m_nodeClient(nodeClient), m_walletDB(walletDB), m_keyChain(config), m_username(username), m_userPath(std::move(userPath))
{

}

Wallet* Wallet::LoadWallet(const Config& config, const INodeClient& nodeClient, IWalletDB& walletDB, const std::string& username)
{
	KeyChainPath userPath = KeyChainPath::FromString("m/0/0"); // TODO: Need to lookup actual account
	return new Wallet(config, nodeClient, walletDB, username, std::move(userPath));
}

WalletSummary Wallet::GetWalletSummary(const CBigInteger<32>& masterSeed)
{
	uint64_t awaitingConfirmation = 0;
	uint64_t immature = 0;
	uint64_t locked = 0;
	uint64_t spendable = 0;

	const uint64_t lastConfirmedHeight = m_nodeClient.GetChainHeight();
	const std::vector<OutputData> outputs = WalletRefresher(m_config, m_nodeClient, m_walletDB).RefreshOutputs(m_username, masterSeed);
	for (const OutputData& outputData : outputs)
	{
		const EOutputStatus status = outputData.GetStatus();
		if (status == EOutputStatus::LOCKED)
		{
			locked += outputData.GetAmount();
		}
		else if (status == EOutputStatus::SPENDABLE)
		{
			spendable += outputData.GetAmount();
		}
		else if (status == EOutputStatus::IMMATURE)
		{
			immature += outputData.GetAmount();
		}
		else if (status == EOutputStatus::NO_CONFIRMATIONS)
		{
			awaitingConfirmation += outputData.GetAmount();
		}
	}

	const uint64_t total = awaitingConfirmation + immature + spendable;
	return WalletSummary(lastConfirmedHeight, m_config.GetWalletConfig().GetMinimumConfirmations(), total, awaitingConfirmation, immature, locked, spendable);
}

bool Wallet::AddOutputs(const CBigInteger<32>& masterSeed, const std::vector<OutputData>& outputs)
{
	return m_walletDB.AddOutputs(m_username, masterSeed, outputs);
}

std::vector<WalletCoin> Wallet::GetAllAvailableCoins(const CBigInteger<32>& masterSeed) const
{
	std::vector<WalletCoin> coins;

	std::vector<Commitment> commitments;
	const std::vector<OutputData> outputs = WalletRefresher(m_config, m_nodeClient, m_walletDB).RefreshOutputs(m_username, masterSeed);
	for (const OutputData& output : outputs)
	{
		if (output.GetStatus() == EOutputStatus::SPENDABLE)
		{
			BlindingFactor blindingFactor = m_keyChain.DerivePrivateKey(masterSeed, output.GetKeyChainPath())->ToBlindingFactor();
			coins.emplace_back(WalletCoin(std::move(blindingFactor), OutputData(output)));
		}
	}

	return coins;
}

std::unique_ptr<WalletCoin> Wallet::CreateBlindedOutput(const CBigInteger<32>& masterSeed, const uint64_t amount)
{
	KeyChainPath keyChainPath = m_walletDB.GetNextChildPath(m_username, m_userPath);
	BlindingFactor blindingFactor = m_keyChain.DerivePrivateKey(masterSeed, keyChainPath)->ToBlindingFactor();
	std::unique_ptr<Commitment> pCommitment = Crypto::CommitBlinded(amount, blindingFactor);
	if (pCommitment != nullptr)
	{
		std::unique_ptr<RangeProof> pRangeProof = m_keyChain.GenerateRangeProof(masterSeed, keyChainPath, amount, *pCommitment, blindingFactor);
		if (pRangeProof != nullptr)
		{
			TransactionOutput transactionOutput(EOutputFeatures::DEFAULT_OUTPUT, Commitment(*pCommitment), RangeProof(*pRangeProof));
			OutputData outputData(std::move(keyChainPath), std::move(transactionOutput), amount, EOutputStatus::NO_CONFIRMATIONS);

			if (m_walletDB.AddOutputs(m_username, masterSeed, std::vector<OutputData>({ outputData })))
			{
				return std::make_unique<WalletCoin>(WalletCoin(std::move(blindingFactor), std::move(outputData)));
			}
		}
	}

	return std::unique_ptr<WalletCoin>(nullptr);
}

std::unique_ptr<SlateContext> Wallet::GetSlateContext(const uuids::uuid& slateId, const CBigInteger<32>& masterSeed) const
{
	return m_walletDB.LoadSlateContext(m_username, masterSeed, slateId);
}

bool Wallet::SaveSlateContext(const uuids::uuid& slateId, const CBigInteger<32>& masterSeed, const SlateContext& slateContext)
{
	return m_walletDB.SaveSlateContext(m_username, masterSeed, slateId, slateContext);
}

bool Wallet::LockCoins(const CBigInteger<32>& masterSeed, std::vector<WalletCoin>& coins)
{
	std::vector<OutputData> outputs;

	for (WalletCoin& coin : coins)
	{
		coin.SetStatus(EOutputStatus::LOCKED);
		outputs.push_back(coin.GetOutputData());
	}

	return m_walletDB.AddOutputs(m_username, masterSeed, outputs);
}