#include <sys/stat.h> 
#include <fcntl.h>
#include <arpa/inet.h>
#include "PCAPReader.h"

#include "log.h"

int extract(const char* orig, const char* pcap, const char* filename)
{
	int fd = FD_INVALID;
	uint32_t originIp = 0;
	inet_pton(AF_INET, orig, &originIp);

	//Open file
	if ((fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0666)) < 0)
		//Error
		return Error("Could not open file [err:%d]\n", errno);


	PCAPReader reader;
	if (!reader.Open(pcap))
		//Error
		return Error("Could not open pcap\n");

	while (auto next = reader.Next())
	{
		if (reader.GetOriginIp() == originIp)
		{
			auto data = reader.GetUDPData();
			auto size = reader.GetUDPSize();

			if (!(data[0] & 1000'0000))
			{
				data += 16;
				size -= 16;
				write(fd, data, size);
			}
		}
	}
	close(fd);
	reader.Close();
}

int main(int argc, char** argv)
{
	const char* source = nullptr;
	const char* pcap = nullptr;
	const char* mpegts = nullptr;

	//Get all
	for (int i = 1; i < argc; i++)
	{
		//Check options
		if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
		{
			//Show usage
			printf("SRT PCAP to MPEGTS extractor");
			printf("Usage: srtextract [-h] [--help] [--pcap file] [--mpetgs file]\r\n\r\n"
				"Options:\r\n"
				" -h,--help        Print help\r\n"
				" -d               Enable debug logging\r\n"
				" -dd              Enable more debug logging\r\n"
				" --source         Source IP address\r\n"
				" --pcap           Input pcap file containing the SRT data\r\n"
				" --mpegts         Output mpegts file\r\n"
			);
			//Exit
			return 0;
		}
		else if (strcmp(argv[i], "-d") == 0)
			//Enable debug
			Logger::EnableDebug(true);
		else if (strcmp(argv[i], "-dd") == 0)
			//Enable debug
			Logger::EnableUltraDebug(true);
		else if (strcmp(argv[i], "--source") == 0 && (i + 1 < argc))
			//Get soure ip
			source = argv[++i];
		else if (strcmp(argv[i], "--mcu-pid") == 0 && (i + 1 < argc))
			//Get pcap file
			pcap = argv[++i];
		else if (strcmp(argv[i], "--mcu-crt") == 0 && (i + 1 < argc))
			//Get mpegts file
			mpegts = argv[++i];
		else if (strcmp(argv[i], "--type=zygote") == 0) {
			//Exit process
			Log("Unknown arg:'%s'\n", argv[1]);
		}

	}

	if (!source)
		return Error("Source ip not set\n");
	if (!pcap)
		return Error("PCAP file not set\n");
	if (!mpegts)
		return Error("MPEGTS file not set\n");

	extract(source, pcap, mpegts);
	
}

