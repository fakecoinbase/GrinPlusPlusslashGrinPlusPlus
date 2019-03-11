#pragma once

#include <Core/Serialization/Serializer.h>
#include <Core/Serialization/ByteBuffer.h>
#include <stdint.h>

static const uint8_t USER_METADATA_FORMAT = 0;

class UserMetadata
{
public:
	UserMetadata(const uint32_t nextTxId, const uint64_t refreshBlockHeight)
		: m_nextTxId(nextTxId), m_refreshBlockHeight(refreshBlockHeight)
	{

	}

	inline uint32_t GetNextTxId() const { return m_nextTxId; }
	inline uint64_t GetRefreshBlockHeight() const { return m_refreshBlockHeight; }

	void Serialize(Serializer& serializer) const
	{
		serializer.Append<uint8_t>(USER_METADATA_FORMAT);
		serializer.Append<uint32_t>(m_nextTxId);
		serializer.Append<uint64_t>(m_refreshBlockHeight);
	}

	static UserMetadata Deserialize(ByteBuffer& byteBuffer)
	{
		if (byteBuffer.ReadU8() != USER_METADATA_FORMAT)
		{
			throw DeserializationException();
		}

		const uint32_t nextTxId = byteBuffer.ReadU32();
		const uint64_t refreshBlockHeight = byteBuffer.ReadU64();
		return UserMetadata(nextTxId, refreshBlockHeight);
	}

private:
	uint32_t m_nextTxId;
	uint64_t m_refreshBlockHeight;
};