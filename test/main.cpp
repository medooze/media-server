/*
 * 
 */
#include "test.h"
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
}


int main(int argc, char** argv)
{
	Logger::EnableDebug(true);

	return TestPlan::ExecuteAll();
}

