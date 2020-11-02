#ifndef DEPENDENCYDESCRIPTOR_H
#define DEPENDENCYDESCRIPTOR_H

#include "config.h"
#include <optional>
#include <utility>
#include <vector>

#include "bitstream.h"
#include "WrapExtender.h"
#include "rtp/LayerInfo.h"

enum DecodeTargetIndication
{
	NotPresent	= 0,	// DecodeTargetInfo symbol '-'
	Discardable	= 1,	// DecodeTargetInfo symbol 'D'
	Switch		= 2,	// DecodeTargetInfo symbol 'S'
	Required	= 3	// DecodeTargetInfo symbol 'R'
};

struct RenderResolution
{
	uint32_t width  = 0;
	uint32_t height = 0;
	
	friend bool operator==(const RenderResolution& lhs, const RenderResolution& rhs)
	{
		return  lhs.width == rhs.width &&
			lhs.height == rhs.height;
	}
};

struct FrameDependencyTemplate : public LayerInfo
{
	std::vector<DecodeTargetIndication> decodeTargetIndications;
	std::vector<uint32_t> frameDiffs;
	std::vector<uint32_t> frameDiffsChains;
	
	void Dump() const;
	
	friend bool operator==(const FrameDependencyTemplate& lhs, const FrameDependencyTemplate& rhs)
	{
		return  lhs.temporalLayerId == rhs.temporalLayerId &&
			lhs.spatialLayerId == rhs.spatialLayerId &&
			lhs.decodeTargetIndications == rhs.decodeTargetIndications &&
			lhs.frameDiffs == rhs.frameDiffs &&
			lhs.frameDiffsChains == rhs.frameDiffsChains;
	}
};

struct TemplateDependencyStructure
{
	uint32_t templateIdOffset = 0;
	uint32_t dtsCount	  = 0;
	uint32_t chainsCount	  = 0;
	
	std::vector<FrameDependencyTemplate> frameDependencyTemplates;
	// The index of the Chain that protects the Decode target
	// When chains count > 0, each Decode Target MUST be protected by exactly one Chain.
	// decodeTargetProtectedByChain[dtIndex] = chainIndex
	std::vector<uint32_t> decodeTargetProtectedByChain;
	std::vector<RenderResolution> resolutions;
	
	//Mapping between decode targets and layers, oredered in descending spataila and layer order
	std::vector<std::pair<uint32_t,LayerInfo>> decodeTargetLayerMapping;
	
	bool ContainsFrameDependencyTemplate(uint32_t frameDependencyTemplateId) const
	{
		return frameDependencyTemplateId >= templateIdOffset
			&& (frameDependencyTemplateId - templateIdOffset) < frameDependencyTemplates.size();
	}

	const FrameDependencyTemplate& GetFrameDependencyTemplate(uint32_t frameDependencyTemplateId) const
	{
		return frameDependencyTemplates[frameDependencyTemplateId - templateIdOffset];
	}
	
	void CalculateLayerMapping();
	
	static std::optional<TemplateDependencyStructure> Parse(BitReader& reader);
	bool Serialize(BitWritter& writter) const;
	void Dump() const;
	
	friend bool operator==(const TemplateDependencyStructure& lhs, const TemplateDependencyStructure& rhs)
	{
		return  lhs.templateIdOffset == rhs.templateIdOffset &&
			lhs.dtsCount == rhs.dtsCount &&
			lhs.chainsCount == rhs.chainsCount &&
			lhs.frameDependencyTemplates == rhs.frameDependencyTemplates &&
			lhs.decodeTargetProtectedByChain == rhs.decodeTargetProtectedByChain &&
			lhs.resolutions == rhs.resolutions;
	}
};

struct DependencyDescriptor
{
	static constexpr int MaxSpatialIds	= 4;
	static constexpr int MaxTemporalIds	= 8;
	static constexpr int MaxDecodeTargets	= 32;
	static constexpr int MaxTemplates	= 64;

	
	
	//mandatory_descriptor_fields
	bool startOfFrame			= true; 
	bool endOfFrame				= true;
	uint32_t  frameDependencyTemplateId	= 0;
	uint16_t  frameNumber			= 0;
	
	std::optional<TemplateDependencyStructure> templateDependencyStructure;
	std::optional<std::vector<bool>> activeDecodeTargets;			
	
	std::optional<std::vector<DecodeTargetIndication>> customDecodeTargetIndications;
	std::optional<std::vector<uint32_t>> customFrameDiffs;
	std::optional<std::vector<uint32_t>> customFrameDiffsChains;
	
	bool Serialize(BitWritter& writter) const;
	void Dump() const;
	
	static std::optional<DependencyDescriptor> Parse(BitReader& reader, const std::optional<TemplateDependencyStructure>& templateDependencyStructure = std::nullopt);

	friend bool operator==(const DependencyDescriptor& lhs, const DependencyDescriptor& rhs)
	{
		return  lhs.startOfFrame == rhs.startOfFrame &&
			lhs.endOfFrame == rhs.endOfFrame &&
			lhs.frameDependencyTemplateId == rhs.frameDependencyTemplateId &&
			lhs.frameNumber == rhs.frameNumber &&
			lhs.templateDependencyStructure == rhs.templateDependencyStructure &&
			lhs.activeDecodeTargets == rhs.activeDecodeTargets &&
			lhs.customDecodeTargetIndications == rhs.customDecodeTargetIndications &&
			lhs.customFrameDiffs == rhs.customFrameDiffs &&
			lhs.customFrameDiffsChains == rhs.customFrameDiffsChains;
	}
};

#endif /* DEPENDENCYDESCRIPTOR_H */

