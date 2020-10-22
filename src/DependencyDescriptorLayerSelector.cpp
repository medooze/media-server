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
	//Get dependency description
	auto& dependencyDescriptor = packet->GetDependencyDescriptor();
	auto& templateDependencyStructure = packet->GetTemplateDependencyStructure();
	auto& activeDecodeTargets = packet->GetActiveDecodeTargets();
	
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
	auto extFrameNum = frameNumberExtender.Extend(dependencyDescriptor->frameNumber);
	
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
	
	//Check if it is decodable
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
	
	//No chain or decode target
	auto currentChain	 = NoChain;
	auto currentDecodeTarget = NoDecodeTarget;
	
	Debug("-DependencyDescriptorLayerSelector::Select() | frame [number=%llu,decodable=%d]\n", extFrameNum, decodable);
	
	//Seach best lahyer target for this spatial and temporal layer
	for (uint32_t i = 0; i<templateDependencyStructure->dtsCount; ++i)
	{
		//Iterate in reverse order, high spatial layers first, then temporal layers within same spatial layer
		uint32_t decodeTarget = templateDependencyStructure->dtsCount-i-1;
		
		//Debug
		Debug("-DependencyDescriptorLayerSelector::Select() | Trying decode target [dt:%llu,layer:layer:S%dL%d,active:%d]\n",
			decodeTarget,
			templateDependencyStructure->decodeTargetLayerMapping[decodeTarget].spatialLayerId,
			templateDependencyStructure->decodeTargetLayerMapping[decodeTarget].temporalLayerId,
			
			!activeDecodeTargets || activeDecodeTargets->at(decodeTarget)
		 );
		
		//Check if it is our current selected layer 
		if (templateDependencyStructure->decodeTargetLayerMapping[decodeTarget].spatialLayerId <= spatialLayerId && 
		    templateDependencyStructure->decodeTargetLayerMapping[decodeTarget].temporalLayerId <= temporalLayerId )
		{
			//If decode target is active
			if (!activeDecodeTargets || activeDecodeTargets->at(decodeTarget))
			{
				//Check dti info is correct
				if (decodeTargetIndications.size()<decodeTarget)
					//Try next
					continue;

				//Get decode target indicattion
				auto dti = decodeTargetIndications[decodeTarget];
				
				//If frame is not present in current target
				if (dti==DecodeTargetIndication::NotPresent)
					//Try next
					continue;
				
				//If we don't have chain info
				if (templateDependencyStructure->decodeTargetProtectedByChain.empty())
				{
					//Got it	
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

				//Get previous frame numner in current chain
				 auto prevFrameInCurrentChain = extFrameNum - frameDiffsChains[chain];
				 
				 //Log
				 Debug("-DependencyDescriptorLayerSelector::Select() | Frame [dt:%llu,chain:%d,prev:%d,dti:%d]\n",decodeTarget,chain,prevFrameInCurrentChain,dti);
				  
				 //If it is not us, check if previus frame was not sent
				 if (prevFrameInCurrentChain && 
				     prevFrameInCurrentChain!=extFrameNum &&
				     !forwardedFrames.Contains(prevFrameInCurrentChain))

					//Chain is broken, try next
					continue;

				//Got it
				currentChain = chain;
				currentDecodeTarget = decodeTarget;
				break;
			}
		}
	}

	 //Log
	Debug("-DependencyDescriptorLayerSelector::Select() | Selected [dt:%llu,chain:%d]\n",currentDecodeTarget,currentChain);
				 
	//If there is none available
	if (currentDecodeTarget==NoDecodeTarget)
	{
		//Request intra
		waitingForIntra = true;
		//Ignore packet
		return false;
	}
	
	//If frame is not decodable but we can't discard it
	if (!decodable && decodeTargetIndications[currentDecodeTarget]==DecodeTargetIndication::Discardable)
		//Ignore packet but do not discard packet
		return false;
	
	//RTP mark is set for the last frame layer of the selected spatial layer
	mark = packet->GetMark() || (dependencyDescriptor->endOfFrame && spatialLayerId==frameDependencyTemplate.spatialLayerId);
	
	//Not waiting for intra
	waitingForIntra = false;
	
	//If it is the last in current frame
	if (dependencyDescriptor->endOfFrame)
		//We only count full forwarded frames
		forwardedFrames.Add(extFrameNum);
	
	UltraDebug("-DependencyDescriptorLayerSelector::Select() | Accepting packet [extSegNum:%u,mark:%d,layer:S%dL%d]\n",
		 packet->GetExtSeqNum(),
		 mark,
		 frameDependencyTemplate.spatialLayerId,
		 frameDependencyTemplate.temporalLayerId);
	//Select
	return true;
	
}

 LayerInfo DependencyDescriptorLayerSelector::GetLayerIds(const RTPPacket::shared& packet)
{
	//Get dependency description
	auto& dependencyDescriptor = packet->GetDependencyDescriptor();
	auto& currentTemplateDependencyStructure = packet->GetTemplateDependencyStructure();
	
	//check 
	if (dependencyDescriptor 
		&& currentTemplateDependencyStructure
		&& currentTemplateDependencyStructure->ContainsFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId))
		//Get layer info from template
		return currentTemplateDependencyStructure->GetFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId);
	
	//Return empty layer info
	return LayerInfo();
}
