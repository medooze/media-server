/*
 * 
 */
#include "test.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}
#include <algorithm>
#include <vector>
#include <string>


int main(int argc, char** argv)
{
	Logger::EnableDebug(true);

	std::vector<std::string> args;
	for (int i = 1; i < argc; i++)
		args.push_back(argv[i]);

	return TestPlan::ExecuteAll([&](const TestPlan& test) {
		if (args.empty())
			return true;
		return std::find(args.begin(), args.end(), test.GetName()) != args.end();
	});
}

