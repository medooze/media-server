inline int avcodec_encode_video(AVCodecContext *avctx, uint8_t *buf, int buf_size,
                         const AVFrame *pict)
{
    AVPacket pkt;
    int ret, got_packet = 0;

    if(buf_size < AV_INPUT_BUFFER_MIN_SIZE){
        av_log(avctx, AV_LOG_ERROR, "buffer smaller than minimum size\n");
        return -1;
    }

    av_init_packet(&pkt);
    pkt.data = buf;
    pkt.size = buf_size;

    ret = avcodec_encode_video2(avctx, &pkt, pict, &got_packet);
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

    return ret ? ret : pkt.size;
}
