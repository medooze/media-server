#include <vector>

#include "DependencyDescriptorLayerSelector.h"

constexpr uint32_t NoChain		= std::numeric_limits<uint32_t>::max();
constexpr uint32_t NoDecodeTarget	= std::numeric_limits<uint32_t>::max();
constexpr uint64_t NoFrame		= std::numeric_limits<uint64_t>::max();
	
DependencyDescriptorLayerSelector::DependencyDescriptorLayerSelector(VideoCodec::Type codec)
{
	this->codec = codec;
}

void DependencyDescriptorLayerSelector::SelectTemporalLayer(BYTE id)
{
	//Set next
	temporalLayerId = id;
}

void DependencyDescriptorLayerSelector::SelectSpatialLayer(BYTE id)
{
	//Set next
	spatialLayerId = id;
}
	
bool DependencyDescriptorLayerSelector::Select(const RTPPacket::shared& packet,bool &mark)
{

	//If the dependency descryptor has not been negotiated
	if (!packet->HasDependencyDestriptor())
		//Accept all
		return true;

	//Get dependency description
	auto& dependencyDescriptor = packet->GetDependencyDescriptor();
	auto& templateDependencyStructure = packet->GetTemplateDependencyStructure();
	auto& activeDecodeTargets = packet->GetActiveDecodeTargets();
	
	//No request intra
	waitingForIntra = false;
	
	//Check rtp packet has a frame descriptor
	if (!dependencyDescriptor)
	{
		//Request intra
		waitingForIntra = true;
		//Error
		return Warning("-DependencyDescriptorLayerSelector::Select() | coulnd't retrieve DependencyDestriptor\n");
	}
	
	//Check we already have received a template structure for this rtp stream
	if (!templateDependencyStructure)
	{
		//Request intra
		waitingForIntra = true;
		//Error
		return Warning("-DependencyDescriptorLayerSelector::Select() | coulnd't retrieve current TemplateDependencyStructure\n");
	}
	
	//Get extended frame number
	frameNumberExtender.Extend(dependencyDescriptor->frameNumber);
	auto extFrameNum = frameNumberExtender.GetExtSeqNum();
	
	//Check if we have not received the first frame 
	if (currentFrameNumber==std::numeric_limits<uint32_t>::max())
	{
		//If it is not first packet in frame
		if (!dependencyDescriptor->startOfFrame)
		{
			//Request intra
			waitingForIntra = true;
			//Ignore packet
			return false;
		}
		
		//Debug
		Debug("-DependencyDescriptorLayerSelector::Select() | Got first frame start [number:%llu]\n",extFrameNum);
		
		//Store current frame
		currentFrameNumber = extFrameNum;
	}
	
	//Ensure that we have the packet frame dependency template
	if (!templateDependencyStructure->ContainsFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId))
		//Skip
		return Warning("-DependencyDescriptorLayerSelector::Select() | Current frame dependency templates don't contain reference templateId [id:%d]\n",dependencyDescriptor->frameDependencyTemplateId);
	
	//Get template
	const auto& frameDependencyTemplate = templateDependencyStructure->GetFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId);
	
	//Get dtis for current frame
	const auto& decodeTargetIndications	= dependencyDescriptor->customDecodeTargetIndications	? dependencyDescriptor->customDecodeTargetIndications.value()	: frameDependencyTemplate.decodeTargetIndications; 
	const auto& frameDiffs			= dependencyDescriptor->customFrameDiffs		? dependencyDescriptor->customFrameDiffs.value()		: frameDependencyTemplate.frameDiffs;
	const auto& frameDiffsChains		= dependencyDescriptor->customFrameDiffsChains		? dependencyDescriptor->customFrameDiffsChains.value()		: frameDependencyTemplate.frameDiffsChains;
	
	//Check if frame is decodable
	bool decodable = true;
	
	//We will only forward full frames
	// TODO: check rtp seq num continuity?
	if (extFrameNum>currentFrameNumber && !dependencyDescriptor->startOfFrame)
		//The frame is not complete
		decodable = false;
	
	//Set current frame
	currentFrameNumber = std::max(extFrameNum,currentFrameNumber);
	
	//Get all referenced frames
	for(size_t i=0; i<frameDiffs.size() && decodable; ++i)
	{
		//Calculate frame number from diff
		auto referencedFrame = extFrameNum - frameDiffs[i];
		//If it is not us
		if (referencedFrame!=extFrameNum)
			//Check if we have sent it
			decodable = forwardedFrames.Contains(referencedFrame);
	}
	
	//No chain or decode target yet
	[[maybe_unused]] auto topChain		 = NoChain;
	[[maybe_unused]] auto currentChain	 = NoChain;
	auto topLayer		 = LayerInfo::NoLayer;
	auto currentLayer	 = LayerInfo::NoLayer;
	auto currentDecodeTarget = NoDecodeTarget;

	//Log
	//UltraDebug("-DependencyDescriptorLayerSelector::Select() | frame [number=%llu,decodable=%d,ts:%lu,mark:%d]\n", extFrameNum, decodable, packet->GetTimestamp(),packet->GetMark());
	
	//If we are doing content adaptation
	if (spatialLayerId!=LayerInfo::MaxLayerId || temporalLayerId!=LayerInfo::MaxLayerId)
	{
		//If we have active decode target info
		if (activeDecodeTargets)
		{
			//Copy it
			forwardedDecodeTargets = activeDecodeTargets;
		} else {
			//Create it
			forwardedDecodeTargets.emplace(templateDependencyStructure->dtsCount);
			//All layers are active
			std::fill(forwardedDecodeTargets->begin(), forwardedDecodeTargets->end(), true);
		}
	}
	
	//Check if we really need to override the active decode target mask
	bool needsForwardedDecodeTargets = false;
	
	//Seach best layer target for this spatial and temporal layer
	for (auto& [decodeTarget,layerInfo] : templateDependencyStructure->decodeTargetLayerMapping)
	{
		//Debug
		//UltraDebug("-DependencyDescriptorLayerSelector::Select() | Trying decode target [dt:%llu,layer:layer:S%dL%d,active:%d]\n",
		//	decodeTarget,
		//	layerInfo.spatialLayerId,
		//	layerInfo.temporalLayerId,
		//	!activeDecodeTargets || (*activeDecodeTargets)[decodeTarget]
		// );
		
		//Check if layers are lower than our content adaptation selected ones
		if (layerInfo.spatialLayerId <= spatialLayerId && layerInfo.temporalLayerId <= temporalLayerId )
		{
			//If decode target is active
			if (!activeDecodeTargets || (*activeDecodeTargets)[decodeTarget])
			{
				//If we don't have chain info
				if (templateDependencyStructure->decodeTargetProtectedByChain.empty())
				{
					//Use current target	
					currentDecodeTarget = decodeTarget;
					break;
				}
					
				//Check we have chain for current target
				if (templateDependencyStructure->decodeTargetProtectedByChain.size()<decodeTarget)
					//Try next
					continue;
				
				//Get chain for current target
				auto chain = templateDependencyStructure->decodeTargetProtectedByChain[decodeTarget];
				
				//Check chain info is correct
				if (frameDiffsChains.size()<chain)
					//Try next
					continue;
				
				//If we don't have a top layer yet
				if (!topLayer.IsValid())
				{
					//Store top most layer
					topLayer = layerInfo;
					topChain = chain;
				}

				//Get previous frame numner in current chain
				 auto prevFrameInCurrentChain = extFrameNum - frameDiffsChains[chain];
				 
				 //Log
				 //UltraDebug("-DependencyDescriptorLayerSelector::Select() | Frame [dt:%llu,chain:%d,prev:%d]\n",decodeTarget,chain,prevFrameInCurrentChain);
				  
				 //If it is not us, check if previus frame was not sent
				 if (prevFrameInCurrentChain && 
				     prevFrameInCurrentChain!=extFrameNum &&
				     !forwardedFrames.Contains(prevFrameInCurrentChain))
					//Chain is broken, try next
					continue;

				//Got it
				currentLayer = layerInfo;
				currentChain = chain;
				currentDecodeTarget = decodeTarget;
				break;
			}
		} else {
			//Disable layer
			(*forwardedDecodeTargets)[decodeTarget] = false;
			needsForwardedDecodeTargets = true;
		}
	}
	
	//If we have not changed the targets
	if (!needsForwardedDecodeTargets)
		//Do not override it
		forwardedDecodeTargets.reset();
	
	//If there is none available
	if (currentDecodeTarget==NoDecodeTarget)
	{
		//Request intra
		waitingForIntra = true;
		//Ignore packet
		return Warning("-DependencyDescriptorLayerSelector::Select() | No decode target availalable\n");
	}
	
	//Check dti info is correct
	if (decodeTargetIndications.size()<currentDecodeTarget)
	{
		//Request intra
		waitingForIntra = true;
		//Ignore packet
		return Warning("-DependencyDescriptorLayerSelector::Select() | No decode target information available [dt:%d]\n",currentDecodeTarget);
	}
	
	//Get decode target indicattion
	auto dti = decodeTargetIndications[currentDecodeTarget];

	//Log
	//Debug("-DependencyDescriptorLayerSelector::Select() | Selected [number=%llu,t:%d,dt:%llu,chain:%d,dti:%d,top:{S%dT%d],current:{S%dT%d],frame:[S%dT%d]\n",
	//	extFrameNum,
	//	dependencyDescriptor->frameDependencyTemplateId,
	//	currentDecodeTarget,
	//	currentChain,
	//	dti,
	//	topLayer.spatialLayerId,
	// 	topLayer.temporalLayerId,
	//	currentLayer.spatialLayerId,
	// 	currentLayer.temporalLayerId,
	//	frameDependencyTemplate.spatialLayerId,
	//	frameDependencyTemplate.temporalLayerId
	//);
	
	//If frame is not decodable
	if (!decodable)
	{
		//Request iframe if chain for the top active layer is broken
		waitingForIntra = (topLayer!=currentLayer);
		//Log
		//UltraDebug("-DependencyDescriptorLayerSelector::Select() | Discarding packet, not decodable\n")
		//Ignore packet
		return false;
	}
	
	//If frame is not present in selected decode target
	if (dti==DecodeTargetIndication::NotPresent)
	{
		//Log
		//UltraDebug("-DependencyDescriptorLayerSelector::Select() | Discarding packet, not present\n");
		//Ignore packet
		return false;
	}

	//RTP mark is set for the last frame layer of the selected spatial layer
	mark = packet->GetMark() || (dependencyDescriptor->endOfFrame && spatialLayerId==frameDependencyTemplate.spatialLayerId);

	//If it is the last in current frame
	if (dependencyDescriptor->endOfFrame)
		//We only count full forwarded frames
		forwardedFrames.Add(extFrameNum);
	
	//UltraDebug("-DependencyDescriptorLayerSelector::Select() | Accepting packet [extSegNum:%u,mark:%d,layer:S%dL%d]\n",
	//	 packet->GetExtSeqNum(),
	//	 mark,
	//	 frameDependencyTemplate.spatialLayerId,
	//	 frameDependencyTemplate.temporalLayerId);
	//Select
	return true;
	
}

std::vector<LayerInfo> DependencyDescriptorLayerSelector::GetLayerIds(const RTPPacket::shared& packet)
{
	std::vector<LayerInfo> infos;
	
	//Get dependency description
	auto& dependencyDescriptor = packet->GetDependencyDescriptor();
	auto& currentTemplateDependencyStructure = packet->GetTemplateDependencyStructure();
	
	//check 
	if (dependencyDescriptor 
		&& currentTemplateDependencyStructure
		&& currentTemplateDependencyStructure->ContainsFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId))
	{
		//Get dependencey structure from template
		const auto& templateDependencyStructure = currentTemplateDependencyStructure->GetFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId);
		//Get decode target indications
		const auto& decodeTargetIndications = dependencyDescriptor->customDecodeTargetIndications 
			? dependencyDescriptor->customDecodeTargetIndications.value()
			: templateDependencyStructure.decodeTargetIndications; 
		//Do not add duplicate layers
		LayerInfo last;
		//Traverse all layers
		for (auto& [decodeTarget,layerInfo] : currentTemplateDependencyStructure->decodeTargetLayerMapping)
		{
			
			//Get decode target indicattion
			auto dti = decodeTargetIndications[decodeTarget];
			//If frame is present in selected decode target and it is not a duplicate
			if (dti!=DecodeTargetIndication::NotPresent && last!=layerInfo)
			{
				//Add layer info
				infos.push_back(layerInfo);
				//Update last
				last = layerInfo;
			}
		}
		
		const auto& frameDependencyTemplate = currentTemplateDependencyStructure->GetFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId);

		if (currentTemplateDependencyStructure->resolutions.size() > frameDependencyTemplate.spatialLayerId)
		{
			auto& res = currentTemplateDependencyStructure->resolutions[frameDependencyTemplate.spatialLayerId];
			packet->SetWidth(res.width);
			packet->SetHeight(res.height);
		}
	}
	
	//Return empty layer info
	return infos;
}
