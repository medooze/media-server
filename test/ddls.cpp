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
					fdt.decodeTargetIndications.insert(fdt.decodeTargetIndications.begin(),DecodeTargetIndication::Switch);
					break;
				case 'R':
					fdt.decodeTargetIndications.insert(fdt.decodeTargetIndications.begin(),DecodeTargetIndication::Required);
					break;
				case 'D':
					fdt.decodeTargetIndications.insert(fdt.decodeTargetIndications.begin(),DecodeTargetIndication::Discardable);
					break;
				case '-':
					fdt.decodeTargetIndications.insert(fdt.decodeTargetIndications.begin(),DecodeTargetIndication::NotPresent);
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

	return tds;
}


std::vector<RTPPacket::shared> generateRTPStream(const std::vector<std::pair<int,int>>& frames, const TemplateDependencyStructure& templateDependencyStructure, const std::vector<int> lost = {})
{
	std::vector<RTPPacket::shared> packets;
	
	//For each frame
	for (const auto& [idx,frameNumber] : frames)
	{
		//Check if it is lost
		if (std::any_of(lost.begin(),lost.end(),[=](const auto &num){ return num==frameNumber; }))
			continue;
		uint32_t frameDependencyTemplateId = idx - 1;
		//Get the template associated to the frame
		const auto& frameDependencyTemplate = templateDependencyStructure.frameDependencyTemplates[frameDependencyTemplateId];
		
		//it is keyframe if doesn't have any frame dependencies
		bool isIntra = frameDependencyTemplate.frameDiffs.empty();
		
		//Create rtp packet
		auto packet = std::make_shared<RTPPacket>(MediaFrame::Video, VideoCodec::AV1);
	
		//Set basic rtp info, only packet per frame
		packet->SetClockRate(90000);
		packet->SetType(96);
		packet->SetSeqNum(frameNumber);
		packet->SetTimestamp(frameNumber*3000);
		packet->SetMark(true);
		
		//Create dependency descriptor
		DependencyDescriptor dependencyDescriptor = {true, true, frameDependencyTemplateId, frameNumber };
		
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
		
		Log("test A.8.2.3 L3T3 K-SVC with Temporal Shift");
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
		templateDependencyStructure.decodeTargetProtectedByChain = { 2, 2, 2, 1, 1, 1, 0, 0, 0};
		//It is in reverse order
		std::reverse(std::begin(templateDependencyStructure.decodeTargetProtectedByChain), std::end(templateDependencyStructure.decodeTargetProtectedByChain));
		
		templateDependencyStructure.Dump();
		
		//Create frame sequence
		std::vector<std::pair<int,int>> frames = {
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

			assert(isEqual(selected, {100,101,102,105,108,111,114,117,120}));
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

			assert(isEqual(selected, {100,101,107,113,119}));
		}
		
		//Simulate loss
		{
			auto packets = generateRTPStream(frames, templateDependencyStructure, {102,110});
			std::vector<int> selected;
			DependencyDescriptorLayerSelector selector(VideoCodec::AV1);
			
			for (const auto& packet: packets)
				if (selector.Select(packet,mark))
					selected.push_back(packet->GetSeqNum());

			assert(isEqual(selected, {100,101,104,107,113,116,119}));
		}
		
	}
};

DependencyDescriptorLayerSelectorTestPlan ddls;
