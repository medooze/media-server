#include "Deinterlacer.h"

extern "C" {
#include <libavutil/opt.h>
#include <libavutil/common.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>

}
#include "log.h"

Deinterlacer::Deinterlacer() : videoBufferPool(2, 4)
{
        //Get input/ouput filter nodes
        bufferSrc = avfilter_get_by_name("buffer");
        bufferSink = avfilter_get_by_name("buffersink");
        //Allocate graph input and ouptut
        outputs = avfilter_inout_alloc();
        inputs = avfilter_inout_alloc();
        //Create graph
        filterGraph = avfilter_graph_alloc();
}

Deinterlacer::~Deinterlacer()
{
        if (inputs)
                avfilter_inout_free(&inputs);
        if (outputs)
                avfilter_inout_free(&outputs);
        if (filterGraph)
                avfilter_graph_free(&filterGraph);

}


int Deinterlacer::Start(uint32_t width, uint32_t height)
{
        int ret;

        //Make sure we have been able to allocate input/outputs and graph
        if (!outputs || !inputs || !filterGraph)
                return Error("-Deinterlacer::Start() | No input/ouput/graphs\n");

        //Store width and height
        this->width = width;
        this->height = height;

        //Set new size in pool
        videoBufferPool.SetSize(width, height);

        //Create input source
        char args[512];
        snprintf(args, sizeof(args), "width=%d:height=%d:pix_fmt=%d:time_base=1/1000", width, height, AV_PIX_FMT_YUV420P);
        if ((ret = avfilter_graph_create_filter(&bufferSrcCtx, bufferSrc, "in", args, nullptr, filterGraph)) < 0)
                //Error
                return Error("-Deinterlacer::Start() | Cannot create buffer source [err:%d]\n", ret);

        //Create ouput sink
        if ((ret = avfilter_graph_create_filter(&bufferSinkCtx, bufferSink, "out", nullptr, nullptr, filterGraph)) < 0)
                //Error
                return Error("-Deinterlacer::Start() | Cannot create buffer sink [err:%d]\n", ret);


        //Set ouput format
        enum AVPixelFormat fmts[] = { AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE };
        if ((ret = av_opt_set_int_list(bufferSinkCtx, "pix_fmts", fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN)) < 0)
                //Error
                return Error("-Deinterlacer::Start() | Cannot set output pixel format [err:%d]\n", ret);

        /*
         * Set the endpoints for the filter graph. The filterGraph will
         * be linked to the graph described by filters_descr.
         */

         /*
          * The buffer source output must be connected to the input pad of
          * the first filter described by filters_descr; since the first
          * filter input label is not specified, it is set to "in" by
          * default.
          */
        outputs->name = av_strdup("in");
        outputs->filter_ctx = bufferSrcCtx;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        /*
         * The buffer sink input must be connected to the output pad of
         * the last filter described by filters_descr; since the last
         * filter output label is not specified, it is set to "out" by
         * default.
         */
        inputs->name = av_strdup("out");
        inputs->filter_ctx = bufferSinkCtx;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        //Create deinterlacer filter
        if ((ret = avfilter_graph_parse_ptr(filterGraph, "yadif", &inputs, &outputs, nullptr)) < 0)
                return Error("-Deinterlacer::Start() | Error filter graph\n");

        if ((ret = avfilter_graph_config(filterGraph, nullptr)) < 0)
                return Error("-Deinterlacer::Start() | Error configuring graph\n");

        //Ok
        return true;
}

void Deinterlacer::Process(const VideoBuffer::const_shared& videoBuffer)
{
        //Allocate frame
        AVFrame* input = av_frame_alloc();

        //Get planes
        const Plane& y = videoBuffer->GetPlaneY();
        const Plane& u = videoBuffer->GetPlaneU();
        const Plane& v = videoBuffer->GetPlaneV();

        //Set input data
        input->data[0] = (unsigned char*)y.GetData();
        input->data[1] = (unsigned char*)u.GetData();
        input->data[2] = (unsigned char*)v.GetData();
        input->data[3] = nullptr;
        input->linesize[0] = y.GetStride();
        input->linesize[1] = u.GetStride();
        input->linesize[2] = v.GetStride();
        input->linesize[3] = 0;
        input->format = AV_PIX_FMT_YUV420P;
        input->width = width;
        input->height = height;
        input->pts = 1;

        //Set color range
        switch (videoBuffer->GetColorRange())
        {
                case VideoBuffer::ColorRange::Partial:
                        //219*2^(n-8) "MPEG" YUV ranges
                        input->color_range = AVCOL_RANGE_MPEG;
                        break;
                case VideoBuffer::ColorRange::Full:
                        //2^n-1   "JPEG" YUV ranges
                        input->color_range = AVCOL_RANGE_JPEG;
                        break;
                default:
                        //Unknown
                        input->color_range = AVCOL_RANGE_UNSPECIFIED;
        }

        //Get color space
        switch (videoBuffer->GetColorSpace())
        {
                case VideoBuffer::ColorSpace::SRGB:
                        ///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
                        input->colorspace = AVCOL_SPC_RGB;
                        break;
                case VideoBuffer::ColorSpace::BT709:
                        ///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
                        input->colorspace = AVCOL_SPC_BT709;
                        break;
                case VideoBuffer::ColorSpace::BT601:
                        ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
                        input->colorspace = AVCOL_SPC_BT470BG;
                        break;
                case VideoBuffer::ColorSpace::SMPTE170:
                        ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
                        input->colorspace = AVCOL_SPC_SMPTE170M;
                        break;
                case VideoBuffer::ColorSpace::SMPTE240:
                        ///< functionally identical to above
                        input->colorspace = AVCOL_SPC_SMPTE240M;
                        break;
                case VideoBuffer::ColorSpace::BT2020:
                        ///< ITU-R BT2020 non-constant luminance system
                        input->colorspace = AVCOL_SPC_BT2020_NCL;
                        break;
                default:
                        //Unknown
                        input->colorspace = AVCOL_SPC_UNSPECIFIED;
        }
        
        //Duplicate reference to the video buffer
        VideoBuffer::const_shared* opaque = new VideoBuffer::const_shared(videoBuffer);

        //Create bufer source reference to avoid mem copies
        input->buf[0] = av_buffer_create(nullptr, 0, [](void* opaque, uint8_t* data) {
                //Free input video buffer reference when avframe is consumed
                delete (VideoBuffer::shared*)opaque;
        }, (void*)opaque, AV_BUFFER_FLAG_READONLY);

        //Push the frame into the filtergraph
        if (av_buffersrc_add_frame_flags(bufferSrcCtx, input, AV_BUFFERSRC_FLAG_KEEP_REF) < 0)
                //Just print error
                Error("-Deinterlacer::Process() | Error while feeding the filtergraph\n");
        
       //Free frame ref, it will be freed when consumed inside the graph wh
       av_frame_unref(input);
}

VideoBuffer::shared Deinterlacer::GetNextFrame()
{
        //Create graph output frame
        AVFrame* output = av_frame_alloc();

        /* pull filtered frames from the filtergraph */
        if (av_buffersink_get_frame(bufferSinkCtx, output) < 0)
        {
                //Release frame
                av_frame_free(&output);
                return nullptr;
        }
        //TODO: Avoid creating a new buffer and reuse output buffers instead
        
        //Get new frame
        auto videoBuffer = videoBufferPool.allocate();

        //Set color range
        switch (output->color_range)
        {
                case AVCOL_RANGE_MPEG:
                        //219*2^(n-8) "MPEG" YUV ranges
                        videoBuffer->SetColorRange(VideoBuffer::ColorRange::Partial);
                        break;
                case AVCOL_RANGE_JPEG:
                        //2^n-1   "JPEG" YUV ranges
                        videoBuffer->SetColorRange(VideoBuffer::ColorRange::Full);
                        break;
                default:
                        //Unknown
                        videoBuffer->SetColorRange(VideoBuffer::ColorRange::Unknown);
        }

        //Get color space
        switch (output->colorspace)
        {
                case AVCOL_SPC_RGB:
                        ///< order of coefficients is actually GBR, also IEC 61966-2-1 (sRGB)
                        videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::SRGB);
                        break;
                case AVCOL_SPC_BT709:
                        ///< also ITU-R BT1361 / IEC 61966-2-4 xvYCC709 / SMPTE RP177 Annex B
                        videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::BT709);
                        break;
                case AVCOL_SPC_BT470BG:
                        ///< also ITU-R BT601-6 625 / ITU-R BT1358 625 / ITU-R BT1700 625 PAL & SECAM / IEC 61966-2-4 xvYCC601
                        videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::BT601);
                        break;
                case AVCOL_SPC_SMPTE170M:
                        ///< also ITU-R BT601-6 525 / ITU-R BT1358 525 / ITU-R BT1700 NTSC
                        videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::SMPTE170);
                        break;
                case AVCOL_SPC_SMPTE240M:
                        ///< functionally identical to above
                        videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::SMPTE240);
                        break;
                case AVCOL_SPC_BT2020_NCL:
                        ///< ITU-R BT2020 non-constant luminance system
                        videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::BT2020);
                        break;
                case AVCOL_SPC_BT2020_CL:
                        ///< ITU-R BT2020 constant luminance system
                        videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::BT2020);
                        break;
                default:
                        //Unknown
                        videoBuffer->SetColorSpace(VideoBuffer::ColorSpace::Unknown);
                }

        //Get planes
        Plane& y = videoBuffer->GetPlaneY();
        Plane& u = videoBuffer->GetPlaneU();
        Plane& v = videoBuffer->GetPlaneV();

        //Copy Y plane
        for (uint32_t i = 0; i < std::min<uint32_t>(height, y.GetHeight()); i++)
                memcpy(y.GetData() + i * y.GetStride(), &output->data[0][i * output->linesize[0]], y.GetWidth());

        //Copy U and V planes
        for (uint32_t i = 0; i < std::min<uint32_t>({height / 2, u.GetHeight(), v.GetHeight() }); i++)
        {
                memcpy(u.GetData() + i * u.GetStride(), &output->data[1][i * output->linesize[1]], u.GetWidth());
                memcpy(v.GetData() + i * v.GetStride(), &output->data[2][i * output->linesize[2]], v.GetWidth());
        }

        //Release frame
        av_frame_free(&output);

        //Return video buffer
        return videoBuffer;
}