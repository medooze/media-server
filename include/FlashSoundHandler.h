#ifndef FLASH_SOUND_HANDLER_H_
#define FLASH_SOUND_HANDLER_H_
#include <sound_handler.h>

class FlashSoundHandler :
	public gnash::media::sound_handler
{
public:
	FlashSoundHandler();
	virtual ~FlashSoundHandler();

	virtual int	create_sound(void* data, unsigned int data_bytes, std::auto_ptr<gnash::media::SoundInfo> sinfo);
	virtual long	fill_stream_data(unsigned char* data, unsigned int data_bytes, unsigned int sample_count, int handle_id);
	virtual void	reset();
	virtual long	fill_stream_data(void* data, unsigned int data_bytes, unsigned int sample_count, int handle_id);
	virtual void 	get_info(int sound_handle, int* format, bool* stereo);
	virtual gnash::media::SoundInfo* get_sound_info(int handle_id);
	virtual void	play_sound(int sound_handle, int loop_count, int secondOffset, long start, const std::vector<sound_envelope>* envelopes);
	virtual void	stop_all_sounds();
	virtual int	get_volume(int sound_handle);
	virtual void	set_volume(int sound_handle, int volume);
	virtual void	stop_sound(int sound_handle);
	virtual void	delete_sound(int sound_handle);
	virtual void	mute();
	virtual void	unmute();
	virtual bool	is_muted();
	virtual void	attach_aux_streamer(aux_streamer_ptr ptr, void* owner);
	virtual void	detach_aux_streamer(void* udata);
	virtual unsigned int get_duration(int sound_handle);
	virtual unsigned int get_position(int sound_handle);

private:
	bool muted;
};


#endif
