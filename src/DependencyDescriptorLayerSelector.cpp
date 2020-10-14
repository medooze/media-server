#include <vector>

#include "DependencyDescriptorLayerSelector.h"

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
	//check 
	if (!packet->HasDependencyDestriptor())
		return Warning("-DependencyDescriptorLayerSelector::Select() | coulnd't retrieve DependencyDestriptor\n");
	
	//Check we already have received a template structure
	if (!packet->HasTemplateDependencyStructure())
		//Drop packet
		return false;
	
	//Get dependency description
	auto& dependencyDescriptor = packet->GetDependencyDescriptor();
	auto& currentTemplateDependencyStructure = packet->GetTemplateDependencyStructure();
	
	//Get extended frame number
	uint32_t extFrameNum = frameNumberExtender.Extend(dependencyDescriptor.frameNumber);
	
	//Check if we have not received the first frame 
	if (currentFrameNumber==std::numeric_limits<uint32_t>::max())
	{
		//If it is not first packet in frame
		if (!dependencyDescriptor.startOfFrame)
			//Ignore packet
			return false;
		//Ensure if has a template structure
		if (!dependencyDescriptor.templateDependencyStructure)
			//Ignore packet
			return false;
		
		//We need to find what is the best decode target for our constrains
		nextDecodeTarget = 0;
		
		//Store current frame
		currentFrameNumber = extFrameNum;
	} 
	
	//Ensure that we have the packet frame dependency template
	if (currentTemplateDependencyStructure.ContainsFrameDependencyTemplate(dependencyDescriptor.frameDependencyTemplateId))
		//Skip
		return Warning("-DependencyDescriptorLayerSelector::Select() | Current frame dependency templates don't contain reference templateId");
	
	//Check we have chain for current targe
	if (currentTemplateDependencyStructure.decodeTargetProtectedByChain.size()<currentDecodeTarget)
		//TODO: what to do here?
		return false;
	
	//Get chain for current target
	auto currentChain = currentTemplateDependencyStructure.decodeTargetProtectedByChain[currentDecodeTarget];
		
	//Get template
	const auto& frameDependencyTemplate = currentTemplateDependencyStructure.GetFrameDependencyTemplate(dependencyDescriptor.frameDependencyTemplateId);
	
	//Get dtis for current frame
	auto& decodeTargetIndications	= dependencyDescriptor.customDecodeTargetIndications	? dependencyDescriptor.customDecodeTargetIndications.value()	: frameDependencyTemplate.decodeTargetIndications; 
	auto& frameDiffs		= dependencyDescriptor.customFrameDiffs			? dependencyDescriptor.customFrameDiffs.value()			: frameDependencyTemplate.frameDiffs;
	auto& frameDiffsChains		= dependencyDescriptor.customFrameDiffsChains		? dependencyDescriptor.customFrameDiffsChains.value()		: frameDependencyTemplate.frameDiffsChains;
	
	//Chec dti info is correct
	if (decodeTargetIndications.size()<currentDecodeTarget)
		//Ignore packet
		return false;
	
	//Get decode target indication for this frame in current decode target
	auto dti = decodeTargetIndications[currentDecodeTarget];
	
	//Check chain info is correct
	if (frameDiffsChains.size()<currentChain)
		//Ignore packet
		return false;
	
	//Get previous frame numner in current chain
	auto prevFrameInCurrentChain = extFrameNum - frameDiffsChains[currentChain];
	
	//Get referenced frames 
	std::vector<uint32_t> referencedFrames(frameDiffs.size());
	
	//Calculate frame number from diff
	for(size_t i=0; i<frameDiffs.size(); ++i)
		referencedFrames[i] = extFrameNum - frameDiffs[i];
	
	
	//We will only forward full frames
	// TODO: check rtp seq num continuity?
	if (extFrameNum>currentFrameNumber && !dependencyDescriptor.startOfFrame)
		//Discard packet
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
	if (dependencyDescriptor.endOfFrame)
		//We only count full forwarded frames
		forwardedFrames.push_back(currentFrameNumber);
	
	//UltraDebug("-DependencyDescriptorLayerSelector::Select() | Accepting packet [extSegNum:%u,mark:%d,sid:%d,tid:%d,current:S%dL%d]\n",packet->GetExtSeqNum(),mark,desc.spatialLayerId,desc.temporalLayerId,spatialLayerId,temporalLayerId);
	//Select
	return true;
	
}

 LayerInfo DependencyDescriptorLayerSelector::GetLayerIds(const RTPPacket::shared& packet)
{
	LayerInfo info;
	
	//Return layer info
	return info;
}
