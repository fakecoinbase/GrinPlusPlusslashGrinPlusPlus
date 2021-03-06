#pragma once

#include <Crypto/Crypto.h>
#include <Crypto/CryptoException.h>

class CryptoUtil
{
public:
	static BlindingFactor AddBlindingFactors(const BlindingFactor* pPositive, const BlindingFactor* pNegative)
	{
		std::vector<BlindingFactor> positive;
		if (pPositive != nullptr)
		{
			positive.push_back(*pPositive);
		}

		std::vector<BlindingFactor> negative;
		if (pNegative != nullptr)
		{
			negative.push_back(*pNegative);
		}

		return Crypto::AddBlindingFactors(positive, negative);
	}
};