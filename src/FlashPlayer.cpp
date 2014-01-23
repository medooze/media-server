#define log_debug printf
#include "FlashPlayer.h"
#include <movie_root.h>
#include <GnashException.h>
#include <VM.h>
#include <URL.h>
#include <tools.h>
#include <SystemClock.h>
#include <movie_instance.h>

FlashPlayer::FlashPlayer() 
{
	// Set all to null
	movie = NULL;
	video = NULL;
	sound = NULL;
	bitmap = NULL;
	frame = NULL;

	// Init 
	gnash::gnashInit();
	gnash::set_use_cache_files(false);

	//Set base url
	gnash::set_base_url(gnash::URL("/root/testgnash/movies"));
}

FlashPlayer::~FlashPlayer()
{
	
}

int FlashPlayer::Init(char *file, int width, int height)
{

	Log(">FlashPlayer Init [%s,%d,%d]\n",file,width,height);

	try {
		// Load movie
		movie = gnash::create_library_movie(gnash::URL(file), file, false);
	} catch(const gnash::GnashException& er) {
		// Print error
		fprintf(stderr, "%s\n", er.what());
		//Exit
		return false;
	}
	
	//Save movie data
	this->width = width;
	this->height = height;

	// Set sound handler
	sound = new FlashSoundHandler();
	//Set the sound handler
	gnash::set_sound_handler(sound);

	// Set video handler
	video = gnash::create_render_handler_agg("RGB24");
	// Set handler
	gnash::set_render_handler(video);

	Log("<FlashPlayer Init\n");
}

int FlashPlayer::Start()
{
	//If already started
	if (!end)
		//Exit
		return 1;

	//Start
	end = 0;

	//Create thread
	pthread_create(&thread,NULL,startPlayback,(void*)this);

	//Finish ok
	return true;
}

void* FlashPlayer::startPlayback(void *par)
{
	//OBtenemos el objeto
	FlashPlayer *player = (FlashPlayer *)par;

	//Bloqueamos las señales
	//blocksignals();

	//Y ejecutamos la funcion
	player->Run();
	//Exit
	return NULL;
}

int FlashPlayer::Run()
{
	Log(">FlashPlayer Run\n");

	gnash::SystemClock clock;	
	unsigned char *src[3];
	unsigned char *dst[3];
	int srcStride[3];
	int dstStride[3];

	//Get Movie root
	root = &(gnash::VM::init(*movie,clock).getRoot());

	// Start loader thread
	movie->completeLoad();

	//Get root instance
        std::auto_ptr<gnash::movie_instance> mi ( movie->create_movie_instance() );
	_movie = mi.get();
	root->setRootMovie(mi.release());

	//Get movie size
	movieWidth = movie->get_width_pixels();
	movieHeight = movie->get_height_pixels();


	// Set bitmap size
	bitmapSize = movieWidth*movieHeight*4;
	// Create buffer for bitmap
	bitmap = (unsigned char *) malloc(bitmapSize);

	// Set frame size
	frameSize = width*height*3/2;
	// Create buffer for bitmap
	frame = (unsigned char *) malloc(frameSize);

	//Set src
	src[0] = bitmap;
	src[1] = 0;
	src[2] = 0;
	srcStride[0] = movieWidth*3;
	srcStride[1] = 0;
	srcStride[2] = 0;

	//Set dest
	dst[0] = frame;
	dst[1] = dst[0] + width*height;
	dst[2] = dst[1] + width*height/4;
	dstStride[0] = width;
	dstStride[1] = width/2;
	dstStride[2] = width/2;

	// Set bitmap
	static_cast<gnash::render_handler_agg_base *>(video)->init_buffer(bitmap,bitmapSize,movieWidth,movieHeight,movieWidth*3);

	//Create format conversor
	sws = sws_getContext(movieWidth,movieHeight,PIX_FMT_RGB24,width,height,PIX_FMT_YUV420P,SWS_BICUBIC,NULL,NULL,NULL);

	//Calculta scale on both axis
	float xscale = width / movieWidth;
        float yscale = height / movieHeight;

	//Set viewport and background
	root->set_display_viewport(0, 0, movieWidth, movieHeight);
	root->setStageScaleMode(gnash::movie_root::exactFit);
	root->set_background_alpha(1.0f);

	//Scale to fit size
	video->set_scale(1,1);
	video->set_translation(0,0);

	//Get movie fps
	float fps = movie->get_frame_rate();

	//Calculate the delay
	unsigned int delay = (unsigned int) (1000000 / fps);

	//Set initial timer
	struct timeval  lastFrameTime;
	long diff = getUpdDifTime(&lastFrameTime);

	Log("-FlashPlayer Run [%d,%d,%d,%d,%f,%f]\n",width,height,movieWidth,movieHeight,xscale,yscale);

	// Loop forever
	while(!end)
	{
		//Render
		root->display();
		//Advance
		root->advance();
		//Convert frame
		sws_scale(sws, src, srcStride, 0, movieHeight , dst, dstStride);
		//Set it to the pipe
		input.SetFrame(frame,width,height);
		//Get elapsed time
		diff = getUpdDifTime(&lastFrameTime);
		//Check elapsed time to see how much do we need to sleep
		if (delay>diff)
			//Sleep
			msleep(delay-diff);
	}

	Log("<FlashPlayer Run\n");
	
	//Exit
	return 1;
}

int FlashPlayer::Stop()
{
	Log(">Stop player\n");

	//If running
	if(!end)
	{
		//Finish loop
		end = 1;
		//Wait for thread completion
		pthread_join(thread,NULL);
	}

	Log("<Stop player\n");

	//End 
	return true;
}

int FlashPlayer::End()
{
	//Free context
	sws_freeContext(sws);

	//Free memory
	if (bitmap) free(bitmap);
	if (frame) free(frame);

	//Clean references
	movie = NULL;
	video = NULL;

	//Call GC
	gnash::clear();	
}
