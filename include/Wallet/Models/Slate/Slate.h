#pragma once

#include <Wallet/Models/Slate/SlateCommitment.h>
#include <Wallet/Models/Slate/SlatePaymentProof.h>
#include <Wallet/Models/Slate/SlateFeatureArgs.h>
#include <Wallet/Models/Slate/SlateSignature.h>
#include <Wallet/Models/Slate/SlateStage.h>
#include <Core/Models/Transaction.h>
#include <Core/Util/JsonUtil.h>
#include <Common/Util/BitUtil.h>
#include <json/json.h>
#include <stdint.h>

#ifdef NOMINMAX
#define SLATE_NOMINMAX_DEFINED
#undef NOMINMAX
#endif
#include <uuid.h>
#ifdef SLATE_NOMINMAX_DEFINED
#undef SLATE_NOMINMAX_DEFINED
#define NOMINMAX
#endif

static uint16_t MIN_SLATE_VERSION = 4;
static uint16_t MAX_SLATE_VERSION = 4;

// A 'Slate' is passed around to all parties to build up all of the public transaction data needed to create a finalized transaction. 
// Callers can pass the slate around by whatever means they choose, (but we can provide some binary or JSON serialization helpers here).
class Slate
{
	// Bit     4      3      2      1      0
	// field  ttl   feat    fee    Amt  num_parts
	struct OptionalFieldStatus
	{
		bool include_num_parts;
		bool include_amt;
		bool include_fee;
		bool include_feat;
		bool include_ttl;

		uint8_t ToByte() const noexcept
		{
			uint8_t status = 0;
			status |= include_num_parts ? 1 : 0;
			status |= include_amt ? 1 << 1 : 0;
			status |= include_fee ? 1 << 2 : 0;
			status |= include_feat ? 1 << 3 : 0;
			status |= include_ttl ? 1 << 4 : 0;
			return status;
		}

		static OptionalFieldStatus FromByte(const uint8_t byte) noexcept
		{
			OptionalFieldStatus status;
			status.include_num_parts = BitUtil::IsSet(byte, 0);
			status.include_amt = BitUtil::IsSet(byte, 1);
			status.include_fee = BitUtil::IsSet(byte, 2);
			status.include_feat = BitUtil::IsSet(byte, 3);
			status.include_ttl = BitUtil::IsSet(byte, 4);
			return status;
		}
	};

	// Bit      1     0
	// field  proof  coms
	struct OptionalStructStatus
	{
		bool include_coms;
		bool include_proof;

		uint8_t ToByte() const noexcept
		{
			uint8_t status = 0;
			status |= include_coms ? 1 : 0;
			status |= include_proof ? 1 << 1 : 0;
			return status;
		}

		static OptionalStructStatus FromByte(const uint8_t byte) noexcept
		{
			OptionalStructStatus status;
			status.include_coms = BitUtil::IsSet(byte, 0);
			status.include_proof = BitUtil::IsSet(byte, 1);
			return status;
		}
	};

public:
	uuids::uuid slateId;
	SlateStage stage;
	uint16_t version;
	uint16_t blockVersion;
	uint8_t numParticipants;

	// Time to Live, or block height beyond which wallets should refuse to further process the transaction.
	// Assumed 0 (no ttl) if omitted from the slate. To be used when delayed transaction posting is desired.
	uint64_t ttl;

	EKernelFeatures kernelFeatures;
	std::optional<SlateFeatureArgs> featureArgsOpt;
	BlindingFactor offset;
	uint64_t amount;
	uint64_t fee;
	std::vector<SlateSignature> sigs;
	std::vector<SlateCommitment> commitments;
	std::optional<SlatePaymentProof> proofOpt;

	Slate(const uuids::uuid& uuid = uuids::uuid_system_generator()())
		: slateId{ uuid },
		stage{ ESlateStage::STANDARD_SENT },
		version{ 4 },
		blockVersion{ 4 },
		numParticipants{ 2 },
		ttl{ 0 },
		kernelFeatures{ EKernelFeatures::DEFAULT_KERNEL },
		featureArgsOpt{ std::nullopt },
		offset{},
		amount{ 0 },
		fee{ 0 },
		sigs{},
		commitments{},
		proofOpt{ std::nullopt }
	{

	}

	bool operator==(const Slate& rhs) const noexcept
	{
		return slateId == rhs.slateId &&
			stage == rhs.stage &&
			version == rhs.version &&
			blockVersion && rhs.blockVersion &&
			amount == rhs.amount &&
			fee == rhs.fee &&
			offset == rhs.offset &&
			numParticipants == rhs.numParticipants &&
			kernelFeatures == rhs.kernelFeatures &&
			ttl == rhs.ttl &&
			sigs == rhs.sigs &&
			commitments == rhs.commitments &&
			proofOpt == rhs.proofOpt;
	}

	const uuids::uuid& GetId() const noexcept { return slateId; }
	uint64_t GetAmount() const noexcept { return amount; }
	uint64_t GetFee() const { return fee; }
	uint64_t GetLockHeight() const noexcept { return featureArgsOpt.has_value() ? featureArgsOpt.value().lockHeightOpt.value_or(0) : 0; }
	std::optional<SlatePaymentProof>& GetPaymentProof() noexcept { return proofOpt; }
	const std::optional<SlatePaymentProof>& GetPaymentProof() const noexcept { return proofOpt; }

	std::vector<TransactionInput> GetInputs() const noexcept
	{
		std::vector<TransactionInput> inputs;
		for (const SlateCommitment& com : commitments)
		{
			if (!com.proofOpt.has_value()) {
				inputs.push_back(TransactionInput{ com.features, com.commitment });
			}
		}

		return inputs;
	}

	std::vector<TransactionOutput> GetOutputs() const noexcept
	{
		std::vector<TransactionOutput> outputs;
		for (const SlateCommitment& com : commitments)
		{
			if (com.proofOpt.has_value()) {
				outputs.push_back(TransactionOutput{ com.features, com.commitment, com.proofOpt.value() });
			}
		}

		return outputs;
	}

	void AddInput(const EOutputFeatures features, const Commitment& commitment)
	{
		for (const auto& com : commitments)
		{
			if (com.commitment == commitment) {
				return;
			}
		}

		commitments.push_back(SlateCommitment{ features, commitment, std::nullopt });
	}

	void AddOutput(const EOutputFeatures features, const Commitment& commitment, const RangeProof& proof)
	{
		for (auto& com : commitments)
		{
			if (com.commitment == commitment) {
				if (!com.proofOpt.has_value()) {
					com.proofOpt = std::make_optional(RangeProof(proof));
				}

				return;
			}
		}

		commitments.push_back(SlateCommitment{ features, commitment, std::make_optional(RangeProof(proof)) });
	}

	/*
		ver.slate_version	u16	2	
		ver.block_header_version	u16	2	
		id	Uuid	16	binary Uuid representation
		sta	u8	1	See Status Byte
		offset	BlindingFactor	32
		Optional field status	u8	1	See Optional Field Status
		num_parts	u8	(1)	If present
		amt	u64	(4)	If present
		fee	u64	(4)	If present
		feat	u8	(1)	If present
		ttl	u64	(4)	If present
		sigs length	u8	1	Number of entries in the sigs struct
		sigs entries	struct	varies	See Sigs Entries
		Optional struct status	u8	1	See Optional Struct Status
		coms length	u16	(2)	If present
		coms entries	struct	(varies)	If present. See Coms Entries
		proof	struct	(varies)	If present. See Proof
		feat_args entries	struct	(varies)	If present. See Feature Args
	*/
	void Serialize(Serializer& serializer) const
	{
		serializer.Append<uint16_t>(version);
		serializer.Append<uint16_t>(blockVersion);
		serializer.AppendBigInteger<16>(CBigInteger<16>{ (uint8_t*)slateId.as_bytes().data() });
		serializer.Append<uint8_t>(stage.ToByte());
		serializer.AppendBigInteger<32>(offset.GetBytes());

		OptionalFieldStatus fieldStatus;
		fieldStatus.include_num_parts = (numParticipants != 2);
		fieldStatus.include_amt = (amount != 0);
		fieldStatus.include_fee = (fee != 0);
		fieldStatus.include_feat = (kernelFeatures != EKernelFeatures::DEFAULT_KERNEL);
		fieldStatus.include_ttl = (ttl != 0);
		serializer.Append<uint8_t>(fieldStatus.ToByte());

		if (fieldStatus.include_num_parts) {
			serializer.Append<uint8_t>(numParticipants);
		}

		if (fieldStatus.include_amt) {
			serializer.Append<uint64_t>(amount);
		}

		if (fieldStatus.include_fee) {
			serializer.Append<uint64_t>(fee);
		}

		if (fieldStatus.include_feat) {
			serializer.Append<uint8_t>((uint8_t)kernelFeatures);
		}

		if (fieldStatus.include_ttl) {
			serializer.Append<uint64_t>(ttl);
		}

		serializer.Append<uint8_t>((uint8_t)sigs.size());
		for (const auto& sig : sigs)
		{
			sig.Serialize(serializer);
		}

		OptionalStructStatus structStatus;
		structStatus.include_coms = !commitments.empty();
		structStatus.include_proof = proofOpt.has_value();
		serializer.Append<uint8_t>(structStatus.ToByte());

		if (structStatus.include_coms) {
			serializer.Append<uint16_t>((uint16_t)commitments.size());

			for (const auto& commitment : commitments)
			{
				commitment.Serialize(serializer);
			}
		}

		if (structStatus.include_proof) {
			proofOpt.value().Serialize(serializer);
		}

		if (kernelFeatures == EKernelFeatures::HEIGHT_LOCKED) {
			serializer.Append<uint64_t>(GetLockHeight());
		}
	}

	static Slate Deserialize(ByteBuffer& byteBuffer)
	{
		Slate slate;
		slate.version = byteBuffer.ReadU16();
		slate.blockVersion = byteBuffer.ReadU16();

		std::vector<unsigned char> slateIdVec = byteBuffer.ReadBigInteger<16>().GetData();
		std::array<unsigned char, 16> slateIdArr;
		std::copy_n(slateIdVec.begin(), 16, slateIdArr.begin());
		slate.slateId = uuids::uuid(slateIdArr);

		slate.stage = SlateStage::FromByte(byteBuffer.ReadU8());
		slate.offset = byteBuffer.ReadBigInteger<32>();

		OptionalFieldStatus fieldStatus = OptionalFieldStatus::FromByte(byteBuffer.ReadU8());

		if (fieldStatus.include_num_parts) {
			slate.numParticipants = byteBuffer.ReadU8();
		}

		if (fieldStatus.include_amt) {
			slate.amount = byteBuffer.ReadU64();
		}

		if (fieldStatus.include_fee) {
			slate.fee = byteBuffer.ReadU64();
		}

		if (fieldStatus.include_feat) {
			slate.kernelFeatures = (EKernelFeatures)byteBuffer.ReadU8();
		}

		if (fieldStatus.include_ttl) {
			slate.ttl = byteBuffer.ReadU64();
		}

		const uint8_t numSigs = byteBuffer.ReadU8();
		for (uint8_t s = 0; s < numSigs; s++)
		{
			slate.sigs.push_back(SlateSignature::Deserialize(byteBuffer));
		}

		OptionalStructStatus structStatus = OptionalStructStatus::FromByte(byteBuffer.ReadU8());

		if (structStatus.include_coms) {
			const uint16_t numComs = byteBuffer.ReadU16();

			for (uint16_t c = 0; c < numComs; c++)
			{
				slate.commitments.push_back(SlateCommitment::Deserialize(byteBuffer));
			}
		}

		if (structStatus.include_proof) {
			slate.proofOpt = std::make_optional<SlatePaymentProof>(SlatePaymentProof::Deserialize(byteBuffer));
		}

		if (slate.kernelFeatures != EKernelFeatures::DEFAULT_KERNEL) {
			SlateFeatureArgs featureArgs;
			if (slate.kernelFeatures == EKernelFeatures::HEIGHT_LOCKED) {
				featureArgs.lockHeightOpt = std::make_optional<uint64_t>(byteBuffer.ReadU64());
			}
			slate.featureArgsOpt = std::make_optional<SlateFeatureArgs>(std::move(featureArgs));
		}

		return slate;
	}

	Json::Value ToJSON() const
	{
		Json::Value json;
		json["ver"] = StringUtil::Format("{}:{}", version, blockVersion);
		json["id"] = uuids::to_string(slateId);
		json["sta"] = stage.ToString();

		if (!offset.IsNull()) {
			json["off"] = offset.ToHex();
		}

		if (numParticipants != 2) {
			json["num_parts"] = std::to_string(numParticipants);
		}

		if (fee != 0) {
			json["fee"] = std::to_string(fee);
		}

		if (amount != 0) {
			json["amt"] = std::to_string(amount);
		}

		if (kernelFeatures != EKernelFeatures::DEFAULT_KERNEL) {
			json["feat"] = (uint8_t)kernelFeatures;
			assert(featureArgsOpt.has_value());
			json["feat_args"] = featureArgsOpt.value().ToJSON();
		}

		if (ttl != 0) {
			json["ttl"] = std::to_string(ttl);
		}

		Json::Value sigsJson(Json::arrayValue);
		for (const auto& sig : sigs)
		{
			sigsJson.append(sig.ToJSON());
		}
		json["sigs"] = sigsJson;

		Json::Value comsJson(Json::arrayValue);
		for (const auto& commitment : commitments)
		{
			comsJson.append(commitment.ToJSON());
		}
		json["coms"] = comsJson;

		if (proofOpt.has_value()) {
			json["proof"] = proofOpt.value().ToJSON();
		}

		return json;
	}

	static Slate FromJSON(const Json::Value& json)
	{
		std::vector<std::string> versionParts = StringUtil::Split(JsonUtil::GetRequiredString(json, "ver"), ":");
		if (versionParts.size() != 2)
		{
			throw std::exception();
		}

		std::optional<uuids::uuid> slateIdOpt = uuids::uuid::from_string(JsonUtil::GetRequiredString(json, "id"));
		if (!slateIdOpt.has_value())
		{
			throw DESERIALIZE_FIELD_EXCEPTION("id");
		}

		auto featuresOpt = JsonUtil::GetOptionalField(json, "feat");

		Slate slate;
		slate.version = (uint16_t)std::stoul(versionParts[0]);
		slate.blockVersion = (uint16_t)std::stoul(versionParts[1]);
		slate.slateId = slateIdOpt.value();
		slate.stage = SlateStage::FromString(JsonUtil::GetRequiredString(json, "sta"));
		slate.offset = JsonUtil::GetBlindingFactorOpt(json, "off").value_or(BlindingFactor{});

		slate.numParticipants = JsonUtil::GetUInt8Opt(json, "num_parts").value_or(2);
		slate.fee = JsonUtil::GetUInt64Opt(json, "fee").value_or(0);
		slate.amount = JsonUtil::GetUInt64Opt(json, "amt").value_or(0);
		slate.kernelFeatures = (EKernelFeatures)JsonUtil::GetUInt8Opt(json, "feat").value_or(0);
		// TODO: Feat args
		slate.ttl = JsonUtil::GetUInt64Opt(json, "ttl").value_or(0);

		std::vector<Json::Value> sigs = JsonUtil::GetRequiredArray(json, "sigs");
		for (const Json::Value& sig : sigs)
		{
			slate.sigs.push_back(SlateSignature::FromJSON(sig));
		}

		std::vector<Json::Value> commitmentsJson = JsonUtil::GetArray(json, "coms");
		for (const Json::Value& commitmentJson : commitmentsJson)
		{
			slate.commitments.push_back(SlateCommitment::FromJSON(commitmentJson));
		}

		std::optional<Json::Value> proofJsonOpt = JsonUtil::GetOptionalField(json, "proof");
		if (proofJsonOpt.has_value() && !proofJsonOpt.value().isNull()) {
			slate.proofOpt = std::make_optional(SlatePaymentProof::FromJSON(proofJsonOpt.value()));
		}

		return slate;
	}
};