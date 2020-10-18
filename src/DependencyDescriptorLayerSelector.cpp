#include <vector>

#include "DependencyDescriptorLayerSelector.h"

constexpr uint32_t NoChain = std::numeric_limits<uint32_t>::max();
constexpr uint64_t NoFrame = std::numeric_limits<uint64_t>::max();
	
DependencyDescriptorLayerSelector::DependencyDescriptorLayerSelector(VideoCodec::Type codec)
{
	this->codec		= codec;
	waitingForIntra		= true;
	temporalLayerId		= 0;
	spatialLayerId		= 0;
	nextTemporalLayerId	= LayerInfo::MaxLayerId;
	nextSpatialLayerId	= LayerInfo::MaxLayerId;
}

void DependencyDescriptorLayerSelector::SelectTemporalLayer(BYTE id)
{
	//Set next
	nextTemporalLayerId = id;
}

void DependencyDescriptorLayerSelector::SelectSpatialLayer(BYTE id)
{
	//Set next
	nextSpatialLayerId = id;
}
	
bool DependencyDescriptorLayerSelector::Select(const RTPPacket::shared& packet,bool &mark)
{
	//Get dependency description
	auto& dependencyDescriptor = packet->GetDependencyDescriptor();
	auto& currentTemplateDependencyStructure = packet->GetTemplateDependencyStructure();
	
	//check 
	if (!dependencyDescriptor)
		return Warning("-DependencyDescriptorLayerSelector::Select() | coulnd't retrieve DependencyDestriptor\n");
	
	//Check we already have received a template structure
	if (!currentTemplateDependencyStructure)
		//Drop packet
		return Warning("-DependencyDescriptorLayerSelector::Select() | coulnd't retrieve current TemplateDependencyStructure\n");
	
	//Get extended frame number
	auto extFrameNum = frameNumberExtender.Extend(dependencyDescriptor->frameNumber);
	
	//Check if we have not received the first frame 
	if (currentFrameNumber==std::numeric_limits<uint32_t>::max())
	{
		//If it is not first packet in frame
		if (!dependencyDescriptor->startOfFrame)
			//Ignore packet
			return false;
		//Ensure if has a template structure
		if (!dependencyDescriptor->templateDependencyStructure)
			//Ignore packet
			return false;
		
		//We need to find what is the best decode target for our constrains
		nextDecodeTarget = 0;
		
		//Store current frame
		currentFrameNumber = extFrameNum;
	}
	
	//We will only forward full frames
	// TODO: check rtp seq num continuity?
	if (extFrameNum>currentFrameNumber && !dependencyDescriptor->startOfFrame)
		//Discard packet
		return false;
	
	//Ensure that we have the packet frame dependency template
	if (!currentTemplateDependencyStructure->ContainsFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId))
		//Skip
		return Warning("-DependencyDescriptorLayerSelector::Select() | Current frame dependency templates don't contain reference templateId [id:%d]\n",dependencyDescriptor->frameDependencyTemplateId);
	
	//No chain
	auto currentChain = NoChain;
		
	//Check we have chain for current targe
	if (currentDecodeTarget < currentTemplateDependencyStructure->decodeTargetProtectedByChain.size())
		//Get chain for current target
		currentChain = currentTemplateDependencyStructure->decodeTargetProtectedByChain[currentDecodeTarget];
	
	//Get template
	const auto& frameDependencyTemplate = currentTemplateDependencyStructure->GetFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId);
	
	//Get dtis for current frame
	auto& decodeTargetIndications	= dependencyDescriptor->customDecodeTargetIndications	? dependencyDescriptor->customDecodeTargetIndications.value()	: frameDependencyTemplate.decodeTargetIndications; 
	auto& frameDiffs		= dependencyDescriptor->customFrameDiffs		? dependencyDescriptor->customFrameDiffs.value()		: frameDependencyTemplate.frameDiffs;
	auto& frameDiffsChains		= dependencyDescriptor->customFrameDiffsChains		? dependencyDescriptor->customFrameDiffsChains.value()		: frameDependencyTemplate.frameDiffsChains;
	
	//Check dti info is correct
	if (decodeTargetIndications.size()<currentDecodeTarget)
		//Ignore packet
		return Warning("-DependencyDescriptorLayerSelector::Select() | Could not find current decode target [dt:%d]\n",currentDecodeTarget);
	
	//Get decode target indication for this frame in current decode target
	auto dti = decodeTargetIndications[currentDecodeTarget];
	
	//If it is not present in currnet target
	if (dti==DecodeTargetIndication::NotPresent)
		//Drop it
		return false;
	
	//Previus frame in chain
	auto prevFrameInCurrentChain = NoFrame;
	
	//Check if current frame is broken
	bool isChainBroken = false;
	
	//Check chain info is correct
	if (frameDiffsChains.size()<currentChain)
	{
		//Get previous frame numner in current chain
		 prevFrameInCurrentChain = extFrameNum - frameDiffsChains[currentChain];
		 //If it is not us
		 if (prevFrameInCurrentChain!=currentFrameNumber)
			//Check if previus frame was not sent
			isChainBroken = !forwardedFrames.Contains(prevFrameInCurrentChain);
	}
	
	//Check if it is decodable
	bool decodable = true;
	
	//Get all referenced frames
	for(size_t i=0; i<frameDiffs.size() && decodable; ++i)
	{
		//Calculate frame number from diff
		auto referencedFrame = extFrameNum - frameDiffs[i];
		//If it is not us
		if (referencedFrame!=currentFrameNumber)
			//Check if we have sent it
			decodable = forwardedFrames.Contains(referencedFrame);
	}
	
	//Check if the frame is decodable or the chain is broken
	if (!decodable || isChainBroken)
		//Drop it
		return false;
		
	
//	//If we have to wait for first intra
//	if (waitingForIntra)
//	{
//		//If this is not intra
//		if (!desc.interPicturePredictedLayerFrame)
//			//Discard
//			return false;
//		//Stop waiting
//		waitingForIntra = 0;
//	}
	
	//RTP mark is set for the last frame layer of the selected layer
//	mark = packet->GetMark() || (desc.endOfLayerFrame && spatialLayerId==desc.spatialLayerId && nextSpatialLayerId<=spatialLayerId);
	
	//If it is the last in current frame
	if (dependencyDescriptor->endOfFrame)
		//We only count full forwarded frames
		forwardedFrames.Add(currentFrameNumber);
	
	//UltraDebug("-DependencyDescriptorLayerSelector::Select() | Accepting packet [extSegNum:%u,mark:%d,sid:%d,tid:%d,current:S%dL%d]\n",packet->GetExtSeqNum(),mark,desc.spatialLayerId,desc.temporalLayerId,spatialLayerId,temporalLayerId);
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
