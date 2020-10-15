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
	if (!packet->HasTemplateDependencyStructure())
		//Drop packet
		return false;
	
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
	if (currentTemplateDependencyStructure->ContainsFrameDependencyTemplate(dependencyDescriptor->frameDependencyTemplateId))
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
	
	//Chec dti info is correct
	if (decodeTargetIndications.size()<currentDecodeTarget)
		//Ignore packet
		return false;
	
	//Get decode target indication for this frame in current decode target
	auto dti = decodeTargetIndications[currentDecodeTarget];
	
	//Previus frame in chain
	auto prevFrameInCurrentChain = NoFrame;
	
	//Check chain info is correct
	if (frameDiffsChains.size()<currentChain)
		//Get previous frame numner in current chain
		 prevFrameInCurrentChain = extFrameNum - frameDiffsChains[currentChain];
	
	//Get referenced frames 
	std::vector<uint64_t> referencedFrames(frameDiffs.size());
	
	//Get all referenced frames
	for(size_t i=0; i<frameDiffs.size(); ++i)
		//Calculate frame number from diff
		referencedFrames[i] = extFrameNum - frameDiffs[i];
	
	
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
