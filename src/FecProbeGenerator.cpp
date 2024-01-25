#include "FecProbeGenerator.h"
#include "media.h"
#include "rtp/RTPPacket.h"
#include <arpa/inet.h>
#include "log.h"

std::vector<RTPPacket::shared> FecProbeGenerator::PrepareFecProbePackets(
	WORD fecRate, bool produceAtLeastOnePacket)
{
	std::vector<RTPPacket::shared> protectedPackets;

	// Get all video packets from history for the last sent video stream,
	// for which we did not created fec packets yet.
	std::optional<DWORD> protectedSsrc;
	for (size_t i=0; i<history.length(); ++i)
	{
		auto candidate = history.at(i);
		if (candidate->GetMediaType() != MediaFrame::Video) continue;

		DWORD ssrc = candidate->GetSSRC();
		if (!protectedSsrc.has_value()) protectedSsrc = ssrc;

		if (ssrc != protectedSsrc.value()) {
			// Log("-FecProbeGenerator::PrepareFecProbePackets() | Skip packet with wrong ssrc [ssrc:%u, protectedSsrc:%u]\n", ssrc, protectedSsrc);
			continue;
		}

		// Accept media packets with seq num after lastExtSeqNum. Check it, only if previous probe generation
		// happened for lastSSrc_. Accept all, when we have new ssrc.
		if (lastSsrc.has_value() && lastExtSeqNum.has_value() && lastSsrc.value() == candidate->GetSSRC())
			if (candidate->GetExtSeqNum() <= lastExtSeqNum.value())
			{
				// Log("-FecProbeGenerator::PrepareFecProbePackets() | Skip packet which was alredy used for protection [lastExtSeqNum:%u, extSeqNum:%u]\n", lastExtSeqNum.value(), candidate->GetExtSeqNum());
				continue;
			}

		protectedPackets.push_back(candidate);
	}

	if (protectedPackets.empty())
	{
		// Log("-FecProbeGenerator::PrepareFecProbePackets() | No packets to protect\n");
		return {};
	}

	DWORD fecPacketsCount = (protectedPackets.size() * fecRate + (1 << 7)) >> 8;
	if (!fecPacketsCount && produceAtLeastOnePacket)
		fecPacketsCount = 1;
	if (!fecPacketsCount) return {};

	// Log("-FecProbeGenerator::PrepareFecProbePackets() | Encoding Fec [protectedPackets.size(): %u, first seq num: %u, last seq num: %u]\n",
		// protectedPackets.size(), protectedPackets.front()->GetSeqNum(), protectedPackets.back()->GetSeqNum());

	std::vector<RTPPacket::shared> fecPackets;
	fecPackets.reserve(fecPacketsCount);

	// When fec rate is higher than 100%, which means fecPacketsCount > protectedPackets.size()
	// (can happen in probing case, when we need to send more than 2x media bitrate of probing),
	// We need to duplicate fec packets protecting single media packet (100% fec rate means the same
	// number of fec packets as protected media packets, each protecting single media packet)
	// The number of fec packets protecting a single packets is fecPacketsProtectingSingleMediaPacketCount,
	// and their payloads are exactly the same.

	// So, for example, for 10 media packets and 250% protection rate, there will be 25 fec packets
	// produced. 20 fec packets protecting single media packet - 2 fec packets protecting
	// the same media packet (duplicates), and 5 fec packets protecting 2 media packets
	// ((0, 5), (1, 6), (2, 7), (3, 8), (4, 9)). This way the protection is equal.

	// Generate fec packets (here fecPacketsCount >= protectedPackets.size()) protecting single
	// media packet.
	size_t fecPacketsProtectingSingleMediaPacketCount = fecPacketsCount / protectedPackets.size();
	if (fecPacketsProtectingSingleMediaPacketCount)
	{
		for (size_t i = 0; i < protectedPackets.size(); ++i)
		{
			auto fecPacket = fec.EncodeFec({protectedPackets.at(i)}, extMap);
			if (!fecPacket) continue;

			fecPackets.push_back(fecPacket);
			for (size_t j = 0; j < fecPacketsProtectingSingleMediaPacketCount - 1; ++j)
			{
				fecPackets.push_back(fecPackets.back());
			}
		}
	}

	// Generate fec packets (here fecPacketsCount < protectedPackets.size()) equaly protecting
	// media packets. Media packet X will be protected by FEC packet (X % fecPacketsCount),
	// or differently ith FEC packet will protect every fecPacketCountth packet media packet,
	// from the ith packet.
	fecPacketsCount = fecPacketsCount - fecPacketsProtectingSingleMediaPacketCount * protectedPackets.size();
	for (size_t i = 0; i < fecPacketsCount; ++i)
	{
		std::vector<RTPPacket::shared> selectedPackets;

		for (size_t j = i; j < protectedPackets.size(); j+= fecPacketsCount)
		{
			selectedPackets.push_back(protectedPackets.at(j));
		}

		fecPackets.push_back(fec.EncodeFec(selectedPackets, extMap));
	}

	lastSsrc = protectedPackets.back()->GetSSRC();
	lastExtSeqNum = protectedPackets.back()->GetExtSeqNum();

	return fecPackets;
}
