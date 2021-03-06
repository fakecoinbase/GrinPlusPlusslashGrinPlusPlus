#include "ReceiveSlateBuilder.h"
#include "SignatureUtil.h"
#include "TransactionBuilder.h"
#include "SlateUtil.h"

#include <Core/Exceptions/WalletException.h>
#include <Wallet/WalletUtil.h>
#include <Infrastructure/Logger.h>

Slate ReceiveSlateBuilder::AddReceiverData(
	Locked<Wallet> wallet,
	const SecureVector& masterSeed,
	const Slate& slate,
	const std::optional<SlatepackAddress>& addressOpt) const
{
	WALLET_INFO_F(
		"Receiving {} from {}",
		slate.GetAmount(),
		addressOpt.has_value() ? addressOpt.value().ToString() : "UNKNOWN"
	);

	auto pWallet = wallet.Write();
	Slate receiveSlate = slate;
	receiveSlate.stage = ESlateStage::STANDARD_RECEIVED;

	// Verify slate was never received, exactly one kernel exists, and fee is valid.
	if (!VerifySlateStatus(pWallet.GetShared(), masterSeed, receiveSlate))
	{
		throw WALLET_EXCEPTION("Failed to verify received slate.");
	}

	auto pBatch = pWallet->GetDatabase().BatchWrite();

	// Generate output
	KeyChainPath keyChainPath = pBatch->GetNextChildPath(pWallet->GetUserPath());
	const uint32_t walletTxId = pBatch->GetNextTransactionId();
	OutputDataEntity outputData = pWallet->CreateBlindedOutput(
		masterSeed,
		receiveSlate.GetAmount(),
		keyChainPath,
		walletTxId,
		EBulletproofType::ENHANCED
	);
	const SecretKey& secretKey = outputData.GetBlindingFactor();
	SecretKey secretNonce = Crypto::GenerateSecureNonce();

	// TODO: Adjust offset?

	// Add receiver ParticipantData
	SlateSignature signature = BuildSignature(receiveSlate, secretKey, secretNonce);

	// Add output to Transaction
	SlateCommitment outputCommit{
		outputData.GetFeatures(),
		outputData.GetCommitment(),
		std::make_optional<RangeProof>(outputData.GetRangeProof())
	};
	receiveSlate.commitments.push_back(outputCommit);

	UpdatePaymentProof(
		pWallet.GetShared(),
		masterSeed,
		receiveSlate
	);

	UpdateDatabase(
		pBatch.GetShared(),
		masterSeed,
		receiveSlate,
		signature,
		outputData,
		walletTxId,
		addressOpt
	);

	pBatch->Commit();

	return receiveSlate;
}

bool ReceiveSlateBuilder::VerifySlateStatus(
	std::shared_ptr<Wallet> pWallet,
	const SecureVector& masterSeed,
	const Slate& slate) const
{
	// Slate was already received.
	std::unique_ptr<WalletTx> pWalletTx = pWallet->GetTxBySlateId(masterSeed, slate.GetId());
	if (pWalletTx != nullptr && pWalletTx->GetType() != EWalletTxType::RECEIVED_CANCELED)
	{
		WALLET_ERROR_F("Already received slate {}", uuids::to_string(slate.GetId()));
		return false;
	}

	// TODO: Verify fees

	return true;
}

SlateSignature ReceiveSlateBuilder::BuildSignature(
	Slate& slate,
	const SecretKey& secretKey,
	const SecretKey& secretNonce) const
{
	const Hash kernelMessage = TransactionKernel::GetSignatureMessage(
		slate.kernelFeatures,
		slate.fee,
		slate.GetLockHeight()
	);

	PublicKey excess = Crypto::CalculatePublicKey(secretKey);
	PublicKey nonce = Crypto::CalculatePublicKey(secretNonce);

	// Generate signature
	slate.sigs.push_back(SlateSignature{ excess, nonce, std::nullopt });

	std::unique_ptr<CompactSignature> pPartialSignature = SignatureUtil::GeneratePartialSignature(
		secretKey,
		secretNonce,
		slate.sigs,
		kernelMessage
	);
	if (pPartialSignature == nullptr)
	{
		WALLET_ERROR_F("Failed to generate signature for slate {}", uuids::to_string(slate.GetId()));
		throw WALLET_EXCEPTION("Failed to generate signature.");
	}

	slate.sigs.back().partialOpt = std::make_optional<CompactSignature>(*pPartialSignature);

	if (!SignatureUtil::VerifyPartialSignatures(slate.sigs, kernelMessage))
	{
		WALLET_ERROR_F("Failed to verify signature for slate {}", uuids::to_string(slate.GetId()));
		throw WALLET_EXCEPTION("Failed to verify signatures.");
	}

	// Add receiver's ParticipantData to Slate
	return slate.sigs.back();
}

void ReceiveSlateBuilder::UpdatePaymentProof(
	const std::shared_ptr<Wallet>& pWallet,
	const SecureVector& masterSeed,
	Slate& receiveSlate) const
{
	if (receiveSlate.GetPaymentProof().has_value())
	{
		if (!pWallet->GetTorAddress().has_value())
		{
			throw WALLET_EXCEPTION("");
		}

		auto& proof = receiveSlate.GetPaymentProof().value();
		if (proof.GetReceiverAddress() != pWallet->GetTorAddress().value().GetPublicKey())
		{
			throw WALLET_EXCEPTION_F("Payment proof receiver address ({} - {}) does not match wallet's (TOR: [{} - {}], Slatepack: [{} - {}])",
				proof.GetReceiverAddress().Format(),
				SlatepackAddress(proof.GetReceiverAddress()).ToString(),
				pWallet->GetTorAddress().value().GetPublicKey().Format(),
				SlatepackAddress(pWallet->GetTorAddress().value().GetPublicKey()).ToString(),
				pWallet->GetSlatepackAddress().GetEdwardsPubKey().Format(),
				pWallet->GetSlatepackAddress().ToString()
			);
		}

		Commitment kernel_commitment = SlateUtil::CalculateFinalExcess(receiveSlate);
		WALLET_INFO_F("Calculated commitment: {}", kernel_commitment.ToHex());

		// Message: (amount | kernel commitment | sender address)
		Serializer messageSerializer;
		messageSerializer.Append<uint64_t>(receiveSlate.GetAmount());
		kernel_commitment.Serialize(messageSerializer);
		messageSerializer.AppendBigInteger(proof.GetSenderAddress().bytes);

		KeyChain keyChain = KeyChain::FromSeed(m_config, masterSeed);
		ed25519_keypair_t torKey = keyChain.DeriveED25519Key(KeyChainPath::FromString("m/0/1/0"));

		ed25519_signature_t signature = ED25519::Sign(
			torKey.secret_key,
			messageSerializer.GetBytes()
		);
		proof.AddSignature(std::move(signature));
	}
}

void ReceiveSlateBuilder::UpdateDatabase(
	std::shared_ptr<IWalletDB> pBatch,
	const SecureVector& masterSeed,
	Slate& receiveSlate,
	const SlateSignature& signature,
	const OutputDataEntity& outputData,
	const uint32_t walletTxId,
	const std::optional<SlatepackAddress>& addressOpt) const
{
	// Save OutputDataEntity
	pBatch->AddOutputs(masterSeed, std::vector<OutputDataEntity>{ outputData });

	// Save WalletTx
	WalletTx walletTx(
		walletTxId,
		EWalletTxType::RECEIVING_IN_PROGRESS,
		std::make_optional(receiveSlate.GetId()),
		addressOpt.has_value() ? std::make_optional(addressOpt.value().ToString()) : std::nullopt,
		std::nullopt,
		std::chrono::system_clock::now(),
		std::nullopt,
		std::nullopt,
		receiveSlate.amount,
		0,
		std::make_optional<uint64_t>(receiveSlate.fee),
		std::nullopt,
		receiveSlate.GetPaymentProof()
	);

	pBatch->AddTransaction(masterSeed, walletTx);

	receiveSlate.amount = 0;
	receiveSlate.fee = 0;
	receiveSlate.sigs = std::vector<SlateSignature>{ signature };
	pBatch->SaveSlate(masterSeed, receiveSlate);
}