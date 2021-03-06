#pragma once

#include <Wallet/WalletManager.h>
#include <Models/TxModels.h>
#include <Consensus/Common.h>

class TestWallet
{
public:
    using Ptr = std::shared_ptr<TestWallet>;

    TestWallet(
        const IWalletManagerPtr& pWalletManager,
        const SessionToken& token,
        const uint16_t listenerPort,
        const std::optional<TorAddress>& torAddressOpt
    ) : m_pWalletManager(pWalletManager),
        m_token(token),
        m_listenerPort(listenerPort),
        m_torAddress(torAddressOpt)
    {
    
    }

    ~TestWallet()
    {
        m_pWalletManager->Logout(m_token);
    }

    const SessionToken& GetToken() const noexcept { return m_token; }
    uint16_t GetListenerPort() const noexcept { return m_listenerPort; }
    const std::optional<TorAddress>& GetTorAddress() const noexcept { return m_torAddress; }
    SlatepackAddress GetSlatepackAddress() const { return m_pWalletManager->GetSlatepackAddress(m_token); }

    static TestWallet::Ptr Create(
        const IWalletManagerPtr& pWalletManager,
        const SessionToken& token,
        const uint16_t listenerPort,
        const std::optional<TorAddress>& torAddressOpt)
    {
        return std::make_shared<TestWallet>(pWalletManager, token, listenerPort, torAddressOpt);
    }

    Test::Tx CreateCoinbase(const KeyChainPath& path, const uint64_t fees)
    {
        auto response = m_pWalletManager->BuildCoinbase(
            BuildCoinbaseCriteria(m_token, fees, std::make_optional<KeyChainPath>(path))
        );

        auto pTx = std::make_shared<Transaction>(
            BlindingFactor(Hash::ValueOf(0)),
            TransactionBody({ {}, { response.GetOutput() }, { response.GetKernel() } })
        );

        return Test::Tx{
            pTx,
            { },
            { { path, Consensus::REWARD + fees } }
        };
    }

    void RefreshWallet()
    {
        m_pWalletManager->CheckForOutputs(m_token, true);
    }

    std::vector<WalletTxDTO> GetTransactions()
    {
        return m_pWalletManager->GetTransactions(
            ListTxsCriteria(m_token, std::nullopt, std::nullopt, {})
        );
    }

private:
    SessionToken m_token;
    uint16_t m_listenerPort;
    std::optional<TorAddress> m_torAddress;
    IWalletManagerPtr m_pWalletManager;
};