#include "FlashSoundHandler.h"

#define Log printf

FlashSoundHandler::FlashSoundHandler()
	: muted(0)
{
}

FlashSoundHandler::~FlashSoundHandler()
{
}

int 	FlashSoundHandler::create_sound(void* data, unsigned int data_bytes, std::auto_ptr<gnash::media::SoundInfo> sinfo)
{
	Log("-create_sound [%d]\n",data_bytes);

	gnash::media::SoundInfo *info = sinfo.get();

	Log("%x",info);

	unsigned long 	sampleRate = sinfo->getSampleRate();
	unsigned long 	sampleCount = sinfo->getSampleCount();
	unsigned int 	format = sinfo->getFormat();
	bool 		stereo = sinfo->isStereo();
	bool 		is16bit = sinfo->is16bit();

	Log("-create_sound [%d,%d,%d,%d,%d,%d]\n",data_bytes,sampleRate,sampleCount,format,stereo,is16bit);

	switch (format)
	{
		case 7: //NATIVE16
			break;

		case 2: //MP3
			break;

		case 0: //RAW
		case 1: //ADPCM
		case 3: //UNCOMPRESSED
		case 6: //NELLYMOSER
		default:
			return 0;
	}
	return 1;
}

void 	FlashSoundHandler::get_info(int sound_handle, int* format, bool* stereo)
{
	Log("-get_info\n");
}

gnash::media::SoundInfo*	FlashSoundHandler::get_sound_info(int handle_id)
{
	Log("-get_sound_info\n");

	return NULL;
}

long	FlashSoundHandler::fill_stream_data(unsigned char* data, unsigned int data_bytes, unsigned int sample_count, int handle_id)
{
	Log("-fill_stream_data\n");
	return 1;
}

void	FlashSoundHandler::reset()
{
	stop_all_sounds();
}

// this gets called when a stream gets more data
long 	FlashSoundHandler::fill_stream_data(void* data, unsigned int data_bytes, unsigned int sample_count, int handle_id)
{
	Log("-fill_stream_data\n");
	return 1;
}

// Play the index'd sample.
void	FlashSoundHandler::play_sound(int sound_handle, int loop_count, int /*offset*/, long start_position, const std::vector<sound_envelope>* /*envelopes*/)
{
	Log("play_sound\n");
}

void	FlashSoundHandler::stop_sound(int sound_handle)
{
	Log("-stop_sound\n");
}

// this gets called when it's done with a sample.
void	FlashSoundHandler::delete_sound(int sound_handle)
{
	Log("-delete_sound\n");
}

// This will stop all sounds playing. Will cause problems if the soundhandler is made static
// and supplys sound_handling for many SWF's, since it will stop all sounds with no regard
// for what sounds is associated with what SWF.
void	FlashSoundHandler::stop_all_sounds()
{
	Log("-stop_all_sounds\n");
}

//	returns the sound volume level as an integer from 0 to 100,
//	where 0 is off and 100 is full volume. The default setting is 100.
int	FlashSoundHandler::get_volume(int sound_handle) 
{
	Log("-get_volume\n");
	return 0; // Invalid handle
}

//	A number from 0 to 100 representing a volume level. 
//	100 is full volume and 0 is no volume. The default setting is 100.
void	FlashSoundHandler::set_volume(int sound_handle, int volume) 
{
	Log("-set_volume\n");
}

// gnash calls this to mute audio
void FlashSoundHandler::mute() {
	stop_all_sounds();
	muted = true;
}

// gnash calls this to unmute audio
void FlashSoundHandler::unmute() {
	muted = false;
}

bool FlashSoundHandler::is_muted() {
	return muted;
}

void FlashSoundHandler::attach_aux_streamer(aux_streamer_ptr /*ptr*/, void* /*owner*/)
{
	Log("-attach_aux_streamer\n");
}

void FlashSoundHandler::detach_aux_streamer(void* /*owner*/)
{
	Log("-detach_aux_streamer\n");
}

unsigned int FlashSoundHandler::get_duration(int sound_handle)
{
	Log("-get_duration\n");
	
	return 0;
}

unsigned int FlashSoundHandler::get_position(int sound_handle)
{
	Log("-get_position\n");

	return 0;
}
