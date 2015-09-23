inline int avcodec_encode_audio(AVCodecContext *avctx,
                                             uint8_t *buf, int buf_size,
                                             const short *samples)
{
    AVPacket pkt;
    AVFrame frame0;
    AVFrame *frame;
    int ret, samples_size, got_packet;

    av_init_packet(&pkt);
    pkt.data = buf;
    pkt.size = buf_size;

    if (samples) {
        frame = &frame0;
        //avcodec_get_frame_defaults(frame);

        if (avctx->frame_size) {
            frame->nb_samples = avctx->frame_size;
        } else {
            /* if frame_size is not set, the number of samples must be
               calculated from the buffer size */
            int64_t nb_samples;
            if (!av_get_bits_per_sample(avctx->codec_id)) {
                av_log(avctx, AV_LOG_ERROR, "avcodec_encode_audio() does not "
                       "support this codec\n");
                return AVERROR(EINVAL);
            }
            nb_samples = (int64_t)buf_size * 8 /
                         (av_get_bits_per_sample(avctx->codec_id) *
                         avctx->channels);
            if (nb_samples >= INT_MAX)
                return AVERROR(EINVAL);
            frame->nb_samples = nb_samples;
        }

        /* it is assumed that the samples buffer is large enough based on the
           relevant parameters */
        samples_size = av_samples_get_buffer_size(NULL, avctx->channels,
                                                  frame->nb_samples,
                                                  avctx->sample_fmt, 1);
        if ((ret = avcodec_fill_audio_frame(frame, avctx->channels,
                                            avctx->sample_fmt,
                                            (const uint8_t*)samples, samples_size, 1)))
            return ret;

    } else {
        frame = NULL;
    }

    got_packet = 0;
    ret = avcodec_encode_audio2(avctx, &pkt, frame, &got_packet);
    if (!ret && got_packet && avctx->coded_frame) {
        avctx->coded_frame->pts       = pkt.pts;
        avctx->coded_frame->key_frame = !!(pkt.flags & AV_PKT_FLAG_KEY);
    }
    /* free any side data since we cannot return it */
    if (pkt.side_data_elems > 0) {
        int i;
        for (i = 0; i < pkt.side_data_elems; i++)
            av_free(pkt.side_data[i].data);
        av_freep(&pkt.side_data);
        pkt.side_data_elems = 0;
    }

    if (frame && frame->extended_data != frame->data)
        av_free(frame->extended_data);

    return ret ? ret : pkt.size;
}
