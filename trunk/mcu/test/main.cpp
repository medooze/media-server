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
        //Init avcodecs
        av_register_all();
        avcodec_register_all();

	return TestPlan::ExecuteAll();
}

