#include "test.h"
#include "rtp.h"
#include "BitHistory.h"
#include "DependencyDescriptorLayerSelector.h"
#include "aac/aacdecoder.h"

#include <sstream>
#include <string>
#include <regex>

template<class C, class T>
bool isEqual(C const& c, std::initializer_list<T> const& l)
{
	return c.size() == l.size() && std::equal(std::begin(c), std::end(c), std::begin(l));
}

TemplateDependencyStructure parseExampleTemplate(const std::vector<std::string>& lines)
{
	TemplateDependencyStructure tds;
	
	//For each line
	for (auto& line : lines)
	{
		//tokenize on tab
		std::vector<std::string> cells;
		std::istringstream sline(line);
		for (std::string cell; std::getline(sline, cell, '\t'); )
			cells.push_back(cell);
		
		FrameDependencyTemplate fdt;
		fdt.spatialLayerId	= std::stoi(cells[1]);
		fdt.temporalLayerId	= std::stoi(cells[2]);
		
		//Tokenize frame diffs on coma
		std::istringstream scell(cells[3]);
		for (std::string fdiff; std::getline(scell, fdiff, ','); )
			fdt.frameDiffs.push_back(std::stoul(fdiff));
		
		for (int i=4;i<cells.size();++i)
		{
			switch(cells[i][0])
			{
				//Decode targets are ordered in reverse order
				case 'S':
					fdt.decodeTargetIndications.push_back(DecodeTargetIndication::Switch);
					break;
				case 'R':
					fdt.decodeTargetIndications.push_back(DecodeTargetIndication::Required);
					break;
				case 'D':
					fdt.decodeTargetIndications.push_back(DecodeTargetIndication::Discardable);
					break;
				case '-':
					fdt.decodeTargetIndications.push_back(DecodeTargetIndication::NotPresent);
					break;
				default:
					fdt.frameDiffsChains.push_back(std::stoul(cells[i]));
			}
		}
	
		tds.frameDependencyTemplates.push_back(fdt);
	}
	
	tds.dtsCount	= tds.frameDependencyTemplates[0].decodeTargetIndications.size();
	tds.chainsCount = tds.frameDependencyTemplates[0].frameDiffsChains.size();
	tds.CalculateLayerMapping();
	tds.Dump();
	return tds;
}

struct FrameDescription 
{
	uint32_t idx;
	uint32_t frameNumber;
	std::optional<std::vector<uint32_t>> customFrameDiffs;
	std::optional<std::vector<uint32_t>> customFrameDiffsChains;
};

std::vector<RTPPacket::shared> generateRTPStream(const std::vector<FrameDescription>& frames, const TemplateDependencyStructure& templateDependencyStructure, const std::vector<int> lost = {})
{
	std::vector<RTPPacket::shared> packets;
	
	//For each frame
	for (const auto& frame : frames)
	{
		//Check if it is lost
		if (std::any_of(lost.begin(),lost.end(),[=](const auto &num){ return num==frame.frameNumber; }))
			continue;
		uint32_t frameDependencyTemplateId = frame.idx - 1;
		//Get the template associated to the frame
		const auto& frameDependencyTemplate = templateDependencyStructure.frameDependencyTemplates[frameDependencyTemplateId];
		
		//it is keyframe if doesn't have any frame dependencies
		bool isIntra = frameDependencyTemplate.frameDiffs.empty();
		
		//Create rtp packet
		auto packet = std::make_shared<RTPPacket>(MediaFrame::Video, VideoCodec::AV1);
	
		//Set basic rtp info, only packet per frame
		packet->SetClockRate(90000);
		packet->SetType(96);
		packet->SetSeqNum(frame.frameNumber);
		packet->SetTimestamp(frame.frameNumber*3000);
		packet->SetMark(true);
		
		//Create dependency descriptor
		DependencyDescriptor dependencyDescriptor = {true, true, frameDependencyTemplateId, frame.frameNumber };
		dependencyDescriptor.customFrameDiffs = frame.customFrameDiffs;
		dependencyDescriptor.customFrameDiffsChains = frame.customFrameDiffsChains;
		//Only send template structure on intra
		if (isIntra)
			dependencyDescriptor.templateDependencyStructure = templateDependencyStructure;
			
		//Set dependency descriptor and template dependency structure
		packet->SetDependencyDescriptor(dependencyDescriptor);
		packet->OverrideTemplateDependencyStructure(templateDependencyStructure);
		
		packets.push_back(packet);
	}
	
	return packets;
}

class DependencyDescriptorLayerSelectorTestPlan: public TestPlan
{
public:
	DependencyDescriptorLayerSelectorTestPlan() : TestPlan("Dependency layer selector test plan")
	{
	}
	
	int init() 
	{
		Log("DDLS::Init\n");
		return true;
	}
	
	int end()
	{
		Log("DDLS::End\n");
		return true;
	}
	
	
	virtual void Execute()
	{
		init();
		
		Log("testBitHistory\n");
		testBitHistory();
		
		Log("test A.8.1.3 Dynamic Prediction Structure\n");
		testCustomDiffs();
			
		Log("test A.8.2.1 L1T3\n");
		testL1T3();
		
		Log("test A.8.2.2 L3T3 Full SVC\n");
		testL3T3FullSVC();
			
		Log("test A.8.2.3 L3T3 K-SVC with Temporal Shift\n");
		testL3T3KSVCTemporalShitt();
		
		end();
	}
	
	virtual void testBitHistory()
	{
		{
			BitHistory<256> history;

			history.Add(1);
			assert(history.Contains(1));
			assert(history.ContainsOffset(0));

			history.Add(2);
			assert(history.Contains(1));
			assert(history.ContainsOffset(1));

			history.Add(4);
			assert(history.Contains(1));
			assert(!history.Contains(3));
			assert(history.ContainsOffset(3));
			assert(!history.ContainsOffset(1));

			history.Add(4);
			assert(history.Contains(1));
			assert(history.ContainsOffset(3));

			history.Add(65);
			assert(history.Contains(1));
			assert(!history.Contains(3));
			assert(history.ContainsOffset(64));

			history.Add(67);
			assert(history.Contains(1));
			assert(!history.Contains(3));
			assert(history.ContainsOffset(65));

			history.Add(240);
			assert(history.Contains(1));
			assert(!history.Contains(3));
			assert(history.ContainsOffset(239));
		}
		
		{
			BitHistory<256> history;

			for (int i = 1; i<=64;++i)
				history.Add(i);
			
			for (int i = 64; i<=256+64;++i)
				history.Add(i);
			
			
			history.Add(256+64+64);
			assert(history.Contains(256+64));
			assert(!history.Contains(256+64+1));
			
		}
	}
	
	void testL1T3()
	{

		//A.8.2.1 L1T3 Single Spatial Layer with 3 Temporal Layers
		auto templateDependencyStructure = parseExampleTemplate({
			"1	0	0		0	S	S	S",
			"2	0	0	4	4	S	S	S",
			"3	0	1	2	2	S	D	-",
			"4	0	2	1	1	D	-	-",
			"5	0	2	1	3	D	-	-",
		});
		templateDependencyStructure.decodeTargetProtectedByChain = { 0, 0, 0};
		
		//Create frame sequence
		std::vector<FrameDescription> frames = {
			{1  , 10},
			{4  , 11},
			{3  , 12},
			{5  , 13},
			{2  , 14},
			{4  , 15},
		};
		//Create frame layer info
		std::vector<std::vector<LayerInfo>> info = {
			{{2,0},{1,0},{0,0}},
			{{2,0}},
			{{2,0},{1,0}},
			{{2,0}},
			{{2,0},{1,0},{0,0}},
			{{2,0}},
		};
		//Generate rtp packets
		auto packets = generateRTPStream(frames, templateDependencyStructure);
		bool mark = true;
		
		//Check layer infos
		for (int i=0; i<packets.size();++i)
			assert(DependencyDescriptorLayerSelector::GetLayerIds(packets[i])==info[i]);
		
		
		
		//No content adaptation
		{
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);

			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());

			//All frames must be forwarded
			assert(isEqual(selected, {10,11,12,13,14,15}));
			//No change on the decode target mask
			auto forwarded = selector.GetForwardedDecodeTargets();
			assert(!forwarded);
		}
		
		//S0T1
		{
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			selector.SelectSpatialLayer(0);
			selector.SelectTemporalLayer(1);
			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
			//Only S1T1 frames and belllow are forwared
			assert(isEqual(selected, {10,12,14}));
			//Mask updated to disable layers > S1T1
			auto forwarded = selector.GetForwardedDecodeTargets();
			assert(forwarded);
			assert(isEqual(*forwarded, {0,1,1}));
		}
		
		//Simulate loss
		{
			auto packets = generateRTPStream(frames, templateDependencyStructure, {12});
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			
			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
			
			assert(isEqual(selected, {10,11,14,15}));
		}
	}
	
	void testCustomDiffs()
	{
		//A.8.1.3 Dynamic Prediction Structure
		auto templateDependencyStructure = parseExampleTemplate({
			"1	0	0		0	S	S",
			"2	0	0	2	2	S	R",
			"3	1	0	1	1	R	-",
			"4	1	0	1,2	1	R	-",
		});
		templateDependencyStructure.decodeTargetProtectedByChain = { 0, 0, 0};
		
		//Create frame sequence
		std::vector<FrameDescription> frames = {
			{1  ,  99},
			{3  , 100},
			{2  , 101},
			{4  , 102},
			{2  , 103},
			{4  , 104},
			{3  , 105 , {}    , {{2}}},
			{3  , 106 , {}    , {{3}}},
			{3  , 107 , {}    , {{4}}},
			{2  , 108 , {{5}} , {{5}}},
			{4  , 109},
			{2  , 110},
			{4  , 111},
		};
		
		//Create frame layer info
		std::vector<std::vector<LayerInfo>> info = {
			{{0,1},{0,0}},
			{{0,1}},
			{{0,1},{0,0}},
			{{0,1}},
			{{0,1},{0,0}},
			{{0,1}},
			{{0,1}},
			{{0,1}},
			{{0,1}},
			{{0,1},{0,0}},
			{{0,1}},
			{{0,1},{0,0}},
			{{0,1}},
		};
		//Generate rtp packets
		auto packets = generateRTPStream(frames, templateDependencyStructure);
		bool mark = true;
		
		//Check layer infos
		for (int i=0; i<packets.size();++i)
			assert(DependencyDescriptorLayerSelector::GetLayerIds(packets[i])==info[i]);
		
		
		//No content adaptation
		{
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);

			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());

			//All frames must be forwarded
			assert(isEqual(selected, {99,100,101,102,103,104,105,106,107,108,109,110,111}));
			//No change on the decode target mask
			auto forwarded = selector.GetForwardedDecodeTargets();
			assert(!forwarded);
		}
		
		//S0T0
		{
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			selector.SelectSpatialLayer(0);
			selector.SelectTemporalLayer(0);
			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
			//Only S0T0
			assert(isEqual(selected, {99,101,103,108,110}));
			//Mask should not be updated
			auto forwarded = selector.GetForwardedDecodeTargets();
			assert(forwarded);
			assert(isEqual(*forwarded, {0,1}));
		}
		
		//S1T0
		{
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			selector.SelectSpatialLayer(1);
			selector.SelectTemporalLayer(0);
			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
			//All
			assert(isEqual(selected, {99,100,101,102,103,104,105,106,107,108,109,110,111}));
			//Mask should not be updated
			auto forwarded = selector.GetForwardedDecodeTargets();
			assert(!forwarded);
		}
		
		//Simulate loss
		{
			auto packets = generateRTPStream(frames, templateDependencyStructure, {106});
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			
			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
			
			assert(isEqual(selected, {99,100,101,102,103,104,105,108,110}));
		}
	}
	
	void testL3T3FullSVC()
	{
		//A.8.2.2 L3T3 Full SVC
		auto templateDependencyStructure = parseExampleTemplate({
			"1	0	0		0	0	0	S	S	S	S	S	S	S	S	S",
			"2	0	0	12	12	11	10	R	R	R	R	R	R	S	S	S",
			"3	0	1	6	6	5	4	R	R	-	R	R	-	S	D	-",
			"4	0	2	3	3	2	1	R	-	-	R	-	-	D	-	-",
			"5	0	2	3	9	8	7	R	-	-	R	-	-	D	-	-",
			"6	1	0	1	1	1	1	S	S	S	S	S	S	-	-	-",
			"7	1	0	12,1	1	1	1	R	R	R	S	S	S	-	-	-",
			"8	1	1	6,1	7	6	5	R	R	-	S	D	-	-	-	-",
			"9	1	2	3,1	4	3	2	R	-	-	D	-	-	-	-	-",
			"10	1	2	3,1	10	9	8	R	-	-	D	-	-	-	-	-",
			"11	2	0	1	2	1	1	S	S	S	-	-	-	-	-	-",
			"12	2	0	12,1	2	1	1	S	S	S	-	-	-	-	-	-",
			"13	2	1	6,1	8	7	6	S	D	-	-	-	-	-	-	-",
			"14	2	2	3,1	5	4	3	D	-	-	-	-	-	-	-	-",
			"15	2	2	3,1	11	10	9	D	-	-	-	-	-	-	-	-",
		});
		templateDependencyStructure.resolutions = {
			{1280 , 960},
			{640  , 480},
			{320  , 240},
		};
		templateDependencyStructure.decodeTargetProtectedByChain = { 2, 2, 2, 1, 1, 1, 0, 0, 0};
		
		//Create frame sequence
		std::vector<FrameDescription> frames = {
			{1  , 100},
			{6  , 101},
			{11 , 102},
			{4  , 103},
			{9  , 104},
			{14 , 105},
			{3  , 106},
			{8  , 107},
			{13 , 108},
			{5  , 109},
			{10 , 110},
			{15 , 111},
			{2  , 112},
			{7  , 113},
			{12 , 114},
			{4  , 115},
			{9  , 116},
			{14 , 117},
			{3  , 118},
			{8  , 119},
			{13 , 120},
			{5  , 121},
			{10 , 122},
			{15 , 123},
		};
		
		//Generate rtp packets
		auto packets = generateRTPStream(frames, templateDependencyStructure);
		bool mark = true;
		
		
		//No content adaptation
		{
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);

			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());

			//All frames must be forwarded
			assert(isEqual(selected, {100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123}));
			//No change on the decode target mask
			auto forwarded = selector.GetForwardedDecodeTargets();
			assert(!forwarded);
		}
		
		//S1T1
		{
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			selector.SelectSpatialLayer(1);
			selector.SelectTemporalLayer(1);
			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
			//Only S1T1 frames and belllow are forwared
			assert(isEqual(selected, {100,101,106,107,112,113,118,119}));
			//Mask updated to disable layers > S1T1
			auto forwarded = selector.GetForwardedDecodeTargets();
			assert(forwarded);
			assert(isEqual(*forwarded, {0,0,0,0,1,1,1,1,1}));
		}
		
		//Simulate loss
		{
			auto packets = generateRTPStream(frames, templateDependencyStructure, {104,113});
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			
			for (const auto& packet: packets)
			{
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
				//We should have an intra request on second loss
				if (packet->GetSeqNum()<113)
					assert(!selector.IsWaitingForIntra());
				else if (packet->GetSeqNum()==114)
					assert(selector.IsWaitingForIntra());
			}
			
			assert(isEqual(selected, {100,101,102,103,106,107,108,109,110,111,112,115,118,121}));
		}
		
		//Simulate loss no intra request
		{
			auto packets = generateRTPStream(frames, templateDependencyStructure, {108});
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			
			for (const auto& packet: packets)
			{
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
				assert(!selector.IsWaitingForIntra());
			}
			
			assert(isEqual(selected, {100,101,102,103,104,105,106,107,109,110,112,113,114,115,116,117,118,119,120,121,122,123}));
		}
	}
	
	void testL3T3KSVCTemporalShitt()
	{
		//A.8.2.3 L3T3 K-SVC with Temporal Shift
		 auto templateDependencyStructure = parseExampleTemplate({
			"1	0	0		0	0	0	S	S	S	S	S	S	S	S	S",
			"2	0	0	3	3	2	1	-	-	-	-	-	-	S	S	S",
			"3	0	0	12	12	8	1	-	-	-	-	-	-	S	S	S",
			"4	0	1	6	6	2	7	-	-	-	-	-	-	S	D	-",
			"5	0	2	3	3	5	4	-	-	-	-	-	-	D	-	-",
			"6	0	2	3	9	5	10	-	-	-	-	-	-	D	-	-",
			"7	0	2	3	3	11	4	-	-	-	-	-	-	D	-	-",
			"8	1	0	1	1	1	1	S	S	S	S	S	S	-	-	-",
			"9	1	0	6	4	6	5	-	-	-	S	S	S	-	-	-",
			"10	1	0	12	4	12	5	-	-	-	S	S	S	-	-	-",
			"11	1	1	6	10	6	11	-	-	-	S	D	-	-	-	-",
			"12	1	2	3	1	3	2	-	-	-	D	-	-	-	-	-",
			"13	1	2	3	7	3	8	-	-	-	D	-	-	-	-	-",
			"14	1	2	3	1	9	2	-	-	-	D	-	-	-	-	-",
			"15	2	0	1	2	1	1	S	S	S	-	-	-	-	-	-",
			"16	2	0	12	11	7	12	S	S	S	-	-	-	-	-	-",
			"17	2	1	6	5	1	6	S	D	-	-	-	-	-	-	-",
			"18	2	2	3	2	4	3	D	-	-	-	-	-	-	-	-",
			"19	2	2	3	8	4	9	D	-	-	-	-	-	-	-	-",
			"20	2	2	3	2	10	3	D	-	-	-	-	-	-	-	-",
		});
		templateDependencyStructure.resolutions = {
			{1280 , 960},
			{640  , 480},
			{320  , 240},
		};
		templateDependencyStructure.decodeTargetProtectedByChain = { 2, 2, 2 , 1, 1, 1, 0, 0, 0};
		
		templateDependencyStructure.Dump();
		
		//Create frame sequence
		std::vector<FrameDescription> frames = {
			{1  , 100},
			{8  , 101},
			{15 , 102},
			{2  , 103},
			{12 , 104},
			{18 , 105},
			{5  , 106},
			{9  , 107},
			{17 , 108},
			{4  , 109},
			{13 , 110},
			{19 , 111},
			{6  , 112},
			{11 , 113},
			{16 , 114},
			{3  , 115},
			{14 , 116},
			{20 , 117},
			{7  , 118},
			{10 , 119},
			{17 , 120}
		};
		//Create frame layer info
		std::vector<std::vector<LayerInfo>> info = {
			{{2,2},{1,2},{0,2},{2,1},{1,1},{0,1},{2,0},{1,0},{0,0}},
			{{2,2},{1,2},{0,2},{2,1},{1,1},{0,1}},
			{{2,2},{1,2},{0,2}},
			{{2,0},{1,0},{0,0}},
			{{2,1}},
			{{2,2}},
			{{2,0}},
			{{2,1},{1,1},{0,1}},
			{{2,2},{1,2}},
			{{2,0},{1,0}},
			{{2,1}},
			{{2,2}},
			{{2,0}},
			{{2,1},{1,1}},
			{{2,2},{1,2},{0,2}},
			{{2,0},{1,0},{0,0}},
			{{2,1}},
			{{2,2}},
			{{2,0}},
			{{2,1},{1,1},{0,1}},
			{{2,2},{1,2}},
		};
		//Generate rtp packets
		auto packets = generateRTPStream(frames, templateDependencyStructure);
		bool mark = true;
		
		//Check layer infos
		for (int i=0; i<packets.size();++i)
			assert(DependencyDescriptorLayerSelector::GetLayerIds(packets[i])==info[i]);
		
		
		//No content adaptation
		{
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);

			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());

			//All S2 frames must be forwarded
			assert(isEqual(selected, {100,101,102,105,108,111,114,117,120}));
			//No change on the decode target mask
			auto forwarded = selector.GetForwardedDecodeTargets();
			assert(!forwarded);
		}
		
		//S1T1
		{
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			selector.SelectSpatialLayer(1);
			selector.SelectTemporalLayer(1);
			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
			//Only S1T1 frames forwared
			assert(isEqual(selected, {100,101,107,113,119}));
			//Mask updated to disable layers > S1T1
			auto forwarded = selector.GetForwardedDecodeTargets();
			assert(forwarded);
			assert(isEqual(*forwarded, {0,0,0,0,1,1,1,1,1}));
		}
		
		//Simulate loss
		{
			auto packets = generateRTPStream(frames, templateDependencyStructure, {102,110});
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			
			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
			//S2 is not forwared and neither some of the S1T2
			assert(isEqual(selected, {100,101,104,107,113,116,119}));
			
			//Now generate an iframe
			std::vector<FrameDescription> frames2 = {
				{1  , 121},
				{8  , 122},
				{15 , 123}
			};
			
			auto packets2 = generateRTPStream(frames2, templateDependencyStructure, {102,110});
			selected.clear();
			for (const auto& packet: packets2)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());
			//Evyrthing should get back to normal
			assert(isEqual(selected, {121,122,123}));
		}
		
	}
};

DependencyDescriptorLayerSelectorTestPlan ddls;
