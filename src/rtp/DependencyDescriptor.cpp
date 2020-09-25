#include "rtp/DependencyDescriptor.h"
#include "bitstream.h"
#include "video.h"

#define CHECK(r) if(r.Error()) return {};

enum NextLayerIdc 
{
	SameLayer	 = 0,
	NextTemporal	 = 1,
	NewSpatial	 = 2,
	NoMoreLayers	 = 3,
	Invalid		 = 4
};

std::optional<TemplateDependencyStructure> TemplateDependencyStructure::Parse(BitReader& reader)
{
	auto tds = std::make_optional<TemplateDependencyStructure>({});
	
	tds->templateIdOffset	= reader.Get(6);	CHECK(reader);
	tds->dtisCount		= reader.Get(5) + 1;	CHECK(reader);
	
	uint32_t temporalId		= 0;
	uint8_t  spatialId		= 0;
	uint8_t  maxTemporalId		= 0;
	NextLayerIdc nextLayerIdc	= Invalid;	
	
	//template_layers()
	while (nextLayerIdc!=NoMoreLayers)
	{
		//Add template layer if info
		tds->frameDependencyTemplates.emplace_back(FrameDependencyTemplate{
			{temporalId,spatialId},
		});
		//Get next layer id
		nextLayerIdc = (NextLayerIdc)reader.Get(2); CHECK(reader);
		
		//Check next layer type
		switch(nextLayerIdc)
		{
			case SameLayer:
				break;
			case NextTemporal:
				temporalId++;
				if (temporalId > maxTemporalId) 
					maxTemporalId = temporalId;
				break;
			case  NewSpatial:
				temporalId = 0;
				spatialId++;
				break;
			case NoMoreLayers:
				break;
		}
	} 
	
	//template_dtis()
	for (auto& fdt : tds->frameDependencyTemplates)
	{
		//Read all dtis up to count num
		while(fdt.decodeTargetIndications.size()< tds->dtisCount)
		{
			//template_dti[templateIndex][dtiIndex] = f(2)
			fdt.decodeTargetIndications.push_back((DecodeTargetIndication)reader.Get(2)); CHECK(reader);
		}
	}
	
	//Store max temporal layer id
	uint8_t maxSpatialId	= spatialId;
	
	//template_fdiffs()
	for (auto& fdp : tds->frameDependencyTemplates)
	{
		//read fdiff_follows_flag
		while (reader.Get(1))
		{
			CHECK(reader);
			//Read fdiff_minus_one 
			fdp.frameDiffs.push_back(reader.Get(4) + 1); CHECK(reader);
		}
	}

	//template_chains()
	tds->chainsCount = reader.GetNonSymmetric(tds->dtisCount + 1);
	if (tds->chainsCount == 0) 
		return tds;
  
	//Read all dtis
	while (tds->decodeTargetProtectedByChain.size() < tds->dtisCount)
	{
		//decode_target_protected_by[dtiIndex]
		tds->decodeTargetProtectedByChain.push_back(reader.GetNonSymmetric(tds->dtisCount)); CHECK(reader);
	}
	
	//For all templates
	for (auto& fdt : tds->frameDependencyTemplates)
	{
		//Read all chains
		while (fdt.frameDiffsChains.size()<tds->chainsCount)
		{
			//template_chain_fdiff
			fdt.frameDiffsChains.push_back(reader.Get(4)); CHECK(reader);
		}
	}
	
	//Read resolutions_present_flag 
	if (reader.Get(1)) 
	{
		//render_resolutions()
		//For all spatiail layers
		while(tds->resolutions.size()<maxSpatialId) 
		{
			//Read resolution
			uint32_t maxRenderWidthMinusOne  = reader.Get(16); CHECK(reader);
			uint32_t maxRenderHeightMinusOne = reader.Get(16); CHECK(reader);
			//Add render resolution for spatial layer
			tds->resolutions.emplace_back(RenderResolution{maxRenderWidthMinusOne + 1, maxRenderHeightMinusOne + 1});
		}
  	}
	
	return tds;
}
	
std::optional<DependencyDescriptor> DependencyDescriptor::Parse(BitReader& reader)
{
	auto dd = std::make_optional<DependencyDescriptor>({});
	
	//check min size
	if (reader.Left()<24)
		//error
		return {};
	
	//mandatory_descriptor_fields()
	dd->startOfFrame		= reader.Get(1);  CHECK(reader);
	dd->endOfFrame			= reader.Get(1);  CHECK(reader);
	dd->frameDependencyTemplateId	= reader.Get(6);  CHECK(reader);
	dd->frameNumber			= reader.Get(16); CHECK(reader);
	
	if (reader.Left()) 
	{
		//extended_descriptor_fields()	
		bool templateDependencyStructurePresent	= reader.Get(1); CHECK(reader);
		bool activeDecodeTargetsPresent		= reader.Get(1); CHECK(reader);
		bool customDtis				= reader.Get(1); CHECK(reader);
		bool customFrameDiffs			= reader.Get(1); CHECK(reader);
		bool customChains			= reader.Get(1); CHECK(reader);
		uint32_t dtisCount			= 0;
		uint32_t chainsCount			= 0;
		uint32_t activeDecodeTargetsBitmask	= 0;
		
		if (templateDependencyStructurePresent)
		{
			//Parse template dependency
			dd->templateDependencyStructure = TemplateDependencyStructure::Parse(reader); CHECK(reader);
			//If failed
			if (!dd->templateDependencyStructure)
				//Error
				return {};
			//Set counts
			dtisCount	= dd->templateDependencyStructure->dtisCount;
			chainsCount	= dd->templateDependencyStructure->chainsCount;
		}

		if (activeDecodeTargetsPresent) 
		{
			//check dtis are set
			if (!dtisCount)
				//Error
				return {};
			//Get bitmask
			activeDecodeTargetsBitmask = reader.Get(dtisCount); CHECK(reader);
			//Create empty field
			dd->activeDecodeTargets = std::vector<bool>(dtisCount);
		}
		
		//If got active decode targets info
		if (dd->activeDecodeTargets)
			//Parse mask
			for (uint8_t i = 0; i < dtisCount; ++i)
				//Read bits from end to first, so push front in field
				dd->activeDecodeTargets.value()[dtisCount - i - 1] = (activeDecodeTargetsBitmask >> i ) & 1;
		
		if (customDtis)
		{
			//check dtis count is set
			if (!dtisCount)
				//Error
				return {};
			//Create custom dtis
			dd->customDecodeTargetIndications = {};
			//Fill custom dtis up to count
			while (dd->customDecodeTargetIndications->size() < customDtis)
			{
				//frame_dti[dtiIndex] = f(2)
				dd->customDecodeTargetIndications->push_back((DecodeTargetIndication)reader.Get(2)); CHECK(reader);
			}
		}
		
		
		if (customFrameDiffs)
		{
			//Create custom dtis
			dd->customFrameDiffs = {};
			
			//Get next value size
			uint32_t nextFrameDiffSize = reader.Get(2); CHECK(reader);
			//While not last
			while (nextFrameDiffSize) 
			{
				//Get frame diff
				uint32_t frameDiffMinusOne = reader.Get(4 * nextFrameDiffSize); CHECK(reader);
				//Push back
				dd->customFrameDiffs->push_back(frameDiffMinusOne + 1);
				//Get next value size
				nextFrameDiffSize = reader.Get(2); CHECK(reader);
			}
		}
		
		if (customChains)
		{
			//check count is set
			if (!chainsCount)
				//Error
				return {};
				
			//Create custom chains
			dd->customFrameDiffsChains = {};
			//Fill custom dtis up to count
			while (dd->customFrameDiffsChains->size() < chainsCount)
			{
				//frame_chain_fdiff[chainIndex] = f(8)
				dd->customFrameDiffsChains->push_back(reader.Get(8)); CHECK(reader);
			}
		}
	}
	
	//frame_dependency_definition()
	//zero_padding = f(sz * 8 - TotalConsumedBits)
	  
	return dd;
}

bool TemplateDependencyStructure::Serialize(BitWritter& writter) const
{
	//Check before write
	if (writter.Left()<11)
		return 0;
	
	writter.Put(6, templateIdOffset);
	writter.Put(5, dtisCount - 1);
	
	//Check before write
	if (writter.Left()<frameDependencyTemplates.size()*2+2)
		return 0;
	
	//template_layers()
	for (size_t i = 1; i < frameDependencyTemplates.size() - 1; ++i)
	{
		//Get next and current layer
		auto& current	= frameDependencyTemplates[i];
		auto& next	= frameDependencyTemplates[i+1];
	
		//Compare spatial and temporal layer ids
		if (next.spatialLayerId == current.spatialLayerId && next.temporalLayerId == current.temporalLayerId) 
			//Same layer
			writter.Put(2, NextLayerIdc::SameLayer);
		else if (next.spatialLayerId == current.spatialLayerId && next.temporalLayerId == current.temporalLayerId + 1)
			//Next temporal
			writter.Put(2, NextLayerIdc::NextTemporal);
		else if (next.spatialLayerId == current.spatialLayerId + 1 && next.temporalLayerId == 0) 
			//Next spatial
			writter.Put(2, NextLayerIdc::NewSpatial);
		else 
			//Error
			return 0;
	}
	
	//No more layers
  	writter.Put(2, NextLayerIdc::NoMoreLayers);
	
	//template_dtis()
	for (auto& fdt : frameDependencyTemplates)
	{
		//Check before write
		if (writter.Left()<fdt.decodeTargetIndications.size()*2)
			return 0;
	
		//Write all dtis
		for (auto& dti : fdt.decodeTargetIndications)
			//template_dti[templateIndex][dtiIndex] = f(2)
			writter.Put(2, dti);
	}
	
	//template_fdiffs()
	for (auto& fdt : frameDependencyTemplates)
	{
		//Check before write
		if (writter.Left()<fdt.frameDiffs.size()*5+1)
			return 0;
		
		//Write all fdiffs
		for (auto& fdiff : fdt.frameDiffs)
		{
			//fdiff_follows_flag
			writter.Put(1, 1);
			//fdiff_minus_one
			writter.Put(4, fdiff - 1);
		}
		//fdiff_follows_flag
		writter.Put(1, 0);
	}

	//template_chains()
	if (!writter.WriteNonSymmetric(dtisCount+1, chainsCount ))
		return 0;
  
	//Read all dtis
	for (auto& dti : decodeTargetProtectedByChain)
		//template_chains()
		if (!writter.WriteNonSymmetric(dtisCount, dti))
			return 0;
	
	//For all templates
	for (auto& fdt : frameDependencyTemplates)
	{
		//Check before write
		if (writter.Left()<fdt.frameDiffsChains.size()*4)
			return 0;
		
		//For all chain diffs
		for (auto diff : fdt.frameDiffsChains)
			//template_chain_fdiff
			writter.Put(4, diff);
	}
	
	//Read 
	if (resolutions.size()) 
	{
		//Check before write
		if (writter.Left()<resolutions.size()*32+1)
			return 0;
		
		//resolutions_present_flag 
		writter.Put(1, 1);
		//render_resolutions()
		for (auto& resolution : resolutions)
		{
			//Write resolution of spatial layer
			writter.Put(16, resolution.width - 1);
			writter.Put(16, resolution.height - 1);
		}
  	} else {
		//resolutions_present_flag 
		writter.Put(1, 0);
	}
	
	return true;
}

bool DependencyDescriptor::Serialize(BitWritter& writter) const
{

	//Check before write
	if (writter.Left()<24)
		return 0;
	
	//mandatory_descriptor_fields()
	writter.Put(1, startOfFrame); 
	writter.Put(1, endOfFrame);
	writter.Put(6, frameDependencyTemplateId);
	writter.Put(16,frameNumber);
	
	if (templateDependencyStructure || activeDecodeTargets || customDecodeTargetIndications || customFrameDiffs || customFrameDiffsChains ) 
	{
		//Check before write
		if (writter.Left()<5)
			return 0;
		
		//extended_descriptor_fields()	
		writter.Put(1, templateDependencyStructure.has_value());
		writter.Put(1, activeDecodeTargets.has_value());
		writter.Put(1, customDecodeTargetIndications.has_value());
		writter.Put(1, customFrameDiffs.has_value());
		writter.Put(1, customFrameDiffsChains.has_value());
		
		if (templateDependencyStructure)
		{
			//Serialize template dependency
			if (!templateDependencyStructure->Serialize(writter))
				//Error
				return 0;
		} else if (activeDecodeTargets || customDecodeTargetIndications || customFrameDiffsChains) {
			uint32_t dtisCount = customDecodeTargetIndications->size();
			writter.Put(5, dtisCount - 1);
			if (customFrameDiffsChains)
				if (!writter.WriteNonSymmetric(dtisCount + 1, customFrameDiffsChains->size() ))
				return 0;
		}

		if (activeDecodeTargets) 
		{
			//Check before write
			if (writter.Left()<activeDecodeTargets->size())
				return 0;
			
			//For each dti
			for (bool active : *activeDecodeTargets)
				//Write if it is active
				writter.Put(1, active);
		}
		
		if (customDecodeTargetIndications)
		{
			//Check before write
			if (writter.Left()<customDecodeTargetIndications->size()*2)
				return 0;
			//For each dti
			for (auto dti : *customDecodeTargetIndications)
				//Write it
				writter.Put(2, dti);
		}
		
		
		if (customFrameDiffs)
		{
			//For each difference
			for (auto fdiff : *customFrameDiffs)
			{
				//It must be non cero
				if (!fdiff || fdiff > 1<<12)
					return 0;
				//Get next value size
				uint32_t nextFrameDiffSize = 0;
				
				if (fdiff <= (1 << 4))
					nextFrameDiffSize = 1;
				else if (fdiff <= (1 << 8))
					nextFrameDiffSize = 2;
				else  //fdiff <= (1 << 12)
					nextFrameDiffSize = 3;
				
				//Check before write
				if (writter.Left() < 2 + nextFrameDiffSize * 4)
					return 0;
				//Write size
				writter.Put(2, nextFrameDiffSize);
				//Write fdif minus one
				writter.Put(nextFrameDiffSize * 4, fdiff -1);
			}
			
			//Check before write
			if (writter.Left() < 2)
				return 0;
			//Last one
			writter.Put(2, 0);
			
		}
		
		if (customFrameDiffsChains)
		{
			
			//Check before write
			if (writter.Left()<customFrameDiffsChains->size()*8)
				return 0;
			
			//For each dti
			for (auto fdiff : *customFrameDiffsChains)
				//Write if it is active
				writter.Put(18, fdiff);
		}
	}
	
	//frame_dependency_definition()
	//zero_padding = f(sz * 8 - TotalConsumedBits)
	  
	return true;
}

void FrameDependencyTemplate::Dump() const
{
	Log("\t\t[FrameDependencyTemplate]\n");;
	
	{
		std::string str; 
		//For each dti
		for (auto dti : decodeTargetIndications)
		{
			if (!str.empty()) str += ",";
			str += std::to_string(dti);
		}
		Log("\t\t\t[decodeTargetIndications]%s[/decodeTargetIndications/]\n", str.c_str());
	}
	
	{
		std::string str;
		//For each difference
		for (auto fdiff : frameDiffs)
		{
			if (!str.empty()) str += ",";
			str += std::to_string(fdiff);
		}
		Log("\t\t\t[frameDiffs]%s[/frameDiffs/]\n", str.c_str());
	}
		
	{
		std::string str;
		//For each difference
		for (auto fdiff : frameDiffsChains)
		{
			if (!str.empty()) str += ",";
			str += std::to_string(fdiff);
		}
		Log("\t\t\t[frameDiffsChains]%s[/frameDiffsChains/]\n", str.c_str());
	}
	  
	Log("\t\t[FrameDependencyTemplate/]\n");;
}

void TemplateDependencyStructure::Dump() const
{
	Log("\t[TemplateDependencyStructure templateIdOffset=%d dtisCount=%d]\n",
		templateIdOffset,
		dtisCount);
	
	for (auto& fdt : frameDependencyTemplates)
		fdt.Dump();
	
	std::string str; 
	for (auto& dti : decodeTargetProtectedByChain)
	{
		if (!str.empty()) str += ",";
		str += dti;
	}
	Log("\t\t[decodeTargetProtectedByChain]%s[/decodeTargetProtectedByChain/]\n", str.c_str());
	
	
	//Read 
	if (resolutions.size()) 
	{
		Log("\t\t[RenderResolutions]\n");;
		//render_resolutions()
		for (auto& resolution : resolutions)
			Log("\t\t\t%dx%x\n",resolution.width,resolution.height);
		
		Log("\t[RenderResolutions/]\n");;
  	}
	
	Log("\t[TemplateDependencyStructure/]\n");;
}

void DependencyDescriptor::Dump() const
{
	Log("[DependencyDescriptor startOfFrame=%d endOfFrame=%d endOfFrame=%u frameNumber=%u]\n",
		startOfFrame,
		endOfFrame,
		frameDependencyTemplateId,
		frameNumber);
	
	if (templateDependencyStructure)
		templateDependencyStructure->Dump();

	if (activeDecodeTargets) 
	{
		std::string str; 
		//For each dti
		for (bool active : *activeDecodeTargets)
		{
			if (!str.empty()) str += ",";
			str += active ? "1" : "0";
		}
		Log("\t[activeDecodeTargets]%s[/activeDecodeTargets/]\n", str.c_str());
	}
		
	if (customDecodeTargetIndications)
	{
		std::string str; 
		//For each dti
		for (auto dti : *customDecodeTargetIndications)
		{
			if (!str.empty()) str += ",";
			str += std::to_string(dti);
		}
		Log("\t[customDecodeTargetIndications]%s[/customDecodeTargetIndications/]\n", str.c_str());
	}
	
	if (customFrameDiffs)
	{
		std::string str;
		//For each difference
		for (auto fdiff : *customFrameDiffs)
		{
			if (!str.empty()) str += ",";
			str += std::to_string(fdiff);
		}
		Log("\t[customDecodeTargetIndications]%s[/customDecodeTargetIndications/]\n", str.c_str());
	}
	
	if (customFrameDiffsChains)
	{
		std::string str;
		//For each difference
		for (auto fdiff : *customFrameDiffsChains)
		{
			if (!str.empty()) str += ",";
			str += std::to_string(fdiff);
		}
		Log("\t[customFrameDiffsChains]%s[/customFrameDiffsChains/]\n", str.c_str());
	}
	  
	Log("[DependencyDescriptor/]\n");;
}
