#pragma once

#include "../UserMetadata.h"
#include "SqliteTransaction.h"

#include <Wallet/WalletDB/WalletDB.h>
#include <Wallet/WalletDB/Models/SlateContextEntity.h>
#include <libsqlite3/sqlite3.h>
#include <unordered_map>

class WalletSqlite : public IWalletDB
{
public:
	explicit WalletSqlite(const fs::path& walletDirectory, const std::string& username, sqlite3* pDatabase)
		: m_walletDirectory(walletDirectory), m_username(username), m_pDatabase(pDatabase), m_pTransaction(nullptr)
	{
	
	}

	virtual ~WalletSqlite() { sqlite3_close(m_pDatabase); }

	void Commit() final;
	void Rollback() noexcept final;
	void OnInitWrite() final;
	void OnEndWrite() final;

	KeyChainPath GetNextChildPath(const KeyChainPath& parentPath) final;

	std::unique_ptr<Slate> LoadSlate(
		const SecureVector& masterSeed,
		const uuids::uuid& slateId,
		const SlateStage& stage
	) const final;

	void SaveSlate(
		const SecureVector& masterSeed,
		const Slate& slate
	) final;

	std::unique_ptr<SlateContextEntity> LoadSlateContext(
		const SecureVector& masterSeed,
		const uuids::uuid& slateId
	) const final;
	void SaveSlateContext(
		const SecureVector& masterSeed,
		const uuids::uuid& slateId,
		const SlateContextEntity& slateContext
	) final;

	void AddOutputs(const SecureVector& masterSeed, const std::vector<OutputDataEntity>& outputs) final;
	std::vector<OutputDataEntity> GetOutputs(const SecureVector& masterSeed) const final;

	void AddTransaction(const SecureVector& masterSeed, const WalletTx& walletTx) final;
	std::vector<WalletTx> GetTransactions(const SecureVector& masterSeed) const final;
	std::unique_ptr<WalletTx> GetTransactionById(const SecureVector& masterSeed, const uint32_t walletTxId) const final;

	uint32_t GetNextTransactionId() final;
	uint64_t GetRefreshBlockHeight() const final;
	void UpdateRefreshBlockHeight(const uint64_t refreshBlockHeight) final;
	uint64_t GetRestoreLeafIndex() const final;
	void UpdateRestoreLeafIndex(const uint64_t lastLeafIndex) final;

private:
	UserMetadata GetMetadata() const;
	void SaveMetadata(const UserMetadata& userMetadata);

	fs::path m_walletDirectory;
	std::string m_username;
	sqlite3* m_pDatabase;
	std::unique_ptr<SqliteTransaction> m_pTransaction;
};
