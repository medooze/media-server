#ifndef VIDEOPIPE_H
#define	VIDEOPIPE_H

#include <pthread.h>
#include "video.h"
#include "VideoBufferScaler.h"

class VideoPipe :
	public VideoOutput,
	public VideoInput
{
public:
	enum AllowedDownScaling
	{
		Any		= 0,
		SameOrLower	= 1,
		LowerOnly	= 2
	};
public:
	VideoPipe();
	~VideoPipe() override;

	int Init(float scaleResolutionDownBy = 0.0f, uint32_t scaleResolutionToHeight = 0, AllowedDownScaling allowedDownScaling = AllowedDownScaling::Any);
	int End();

	/** VideoInput */
	int StartVideoCapture(uint32_t width, uint32_t height, uint32_t fps) override;
	VideoBuffer::const_shared GrabFrame(uint32_t timeout) override;
	void CancelGrabFrame() override;
	int StopVideoCapture() override;

	/** VideoOutput */
	int NextFrame(const VideoBuffer::const_shared& videoBuffer) override;
	void ClearFrame() override;

private:
	uint32_t videoWidth = 0;
	uint32_t videoHeight = 0;
	int videoFPS = 0;
	float scaleResolutionDownBy = 0.0f;
	uint32_t scaleResolutionToHeight = 0;
	int inited = false;
	int capturing = false;
	int cancelledGrab = false;

	// @todo Move this into proper FIFO implementation (with overwriting overflow semantics) later (maybe one already exists, need some tests for this and could be more performant)
	// Note: Simple impl not thread safe
	template <typename DataT, int MAX_SIZE_T>
	class OverwritingFIFO
	{
	public:
		OverwritingFIFO() {}
		OverwritingFIFO(const OverwritingFIFO& rhs) = delete;
		OverwritingFIFO& operator=(const OverwritingFIFO& rhs) = delete;

		bool PushBack(const DataT& item)
		{
			// if we are full and about to overflow
			bool overflowing = ((windex + 1) % buffer.size()) == rindex;
			if (overflowing)
			{
				// Discard the next read item
				PopFront(nullptr);
			}
			assert(!(((windex + 1) % buffer.size()) == rindex));

			buffer[windex] = item;
			windex = (windex + 1) % buffer.size();

			return !overflowing;
		}

		bool PopFront(DataT* item_out)
		{
			if (windex == rindex)
			{
				return false;
			}

			if (item_out)
			{
				*item_out = std::move(buffer[rindex]);
			}
			else
			{
				buffer[rindex] = DataT();
			}
			rindex = (rindex + 1) % buffer.size();

			return true;
		}

		size_t Size() const
		{
			if (windex < rindex)
			{
				return windex + buffer.size() - rindex;
			}
			else
			{
				return windex - rindex;
			}
		}

		void Clear()
		{
			while (PopFront(nullptr));
		}

	private:
		size_t windex = 0;
		std::array<DataT, MAX_SIZE_T + 1> buffer;
		size_t rindex = 0;
	};
	OverwritingFIFO<VideoBuffer::const_shared, 100> buffer;

	pthread_mutex_t newPicMutex;
	pthread_cond_t  newPicCond;

	VideoBufferPool	videoBufferPool;
	VideoBufferScaler scaler;
	AllowedDownScaling allowedDownScaling = AllowedDownScaling::Any;
	QWORD lastPush = 0;
	QWORD lastPull = 0;

};

#endif	/* VIDEOPIPE_H */

