#pragma once

#include <Core/Traits/Jsonable.h>
#include <cstdint>

class WalletBalanceDTO : public Traits::IJsonable
{
public:
    WalletBalanceDTO(
        const uint64_t unconfirmed,
        const uint64_t immature,
        const uint64_t locked,
        const uint64_t spendable
    ) : m_unconfirmed(unconfirmed),
        m_immature(immature),
        m_locked(locked),
        m_spendable(spendable) { }

    uint64_t GetTotal() const noexcept { return m_unconfirmed + m_immature + m_spendable; }
    uint64_t GetUnconfirmed() const noexcept { return m_unconfirmed; }
    uint64_t GetImmature() const noexcept { return m_immature; }
    uint64_t GetLocked() const noexcept { return m_locked; }
    uint64_t GetSpendable() const noexcept { return m_spendable; }

    Json::Value ToJSON() const noexcept final
    {
        Json::Value json;
        json["total"] = GetTotal();
        json["unconfirmed"] = m_unconfirmed;
        json["immature"] = m_immature;
        json["locked"] = m_locked;
        json["spendable"] = m_spendable;
        return json;
    }

private:
    uint64_t m_unconfirmed;
    uint64_t m_immature;
    uint64_t m_locked;
    uint64_t m_spendable;
};