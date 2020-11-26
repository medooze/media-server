# media-server

[![Codacy Badge](https://api.codacy.com/project/badge/Grade/d6a13f1d0ab5428c95cb977af50e344b)](https://www.codacy.com/app/murillo128/media-server?utm_source=github.com&utm_medium=referral&utm_content=medooze/media-server&utm_campaign=badger)

WebRTC Media Server

Documentation coming soon, major refactoring ongoing. Stay tuned!

## Usage
This repository is currently a host for the base media code used in different projects. While it may take a while to propertly encapsulate it and define reusable components to create a proper SDK, you can use the following native wrappers:
 - [MCU](http://medooze.com/products/mcu.aspx).
 - [Media Server for Node.js](https://github.com/medooze/media-server-node)
 - [Media Server for Golang](https://github.com/notedit/media-server-go)
 - Media Server for Java (Coming soon)

## Functionality
We intend to implement support the following features:

- [x] Video encoders/decoders: VP8 (VP6 on decoding), H264, MP4V-ES H263P, Sorenson H263 and H263 support
- [x] Audio enoders/decoders: PCMU, PCMA, G722, GSM, SPEEX, NellyMoser, AAC (only encoding) and Opus support
- [x] Video mixing with continuous presence
- [x] Audio mixing in 8khz,16Khz,32khz and 48Khz
- [x] Flash broadcasting
- [x] MP4 multitrack recording for H264, VP8, Opus, G711 and VP9 (WIP)
- [x] MP4 playback
- [x] VAD support
- [x] [VP9 SVC](https://tools.ietf.org/html/draft-ietf-payload-tvp9-02) layer selection
- [x] VP8 Simulcast
- [x] [RTP transport wide congestion control](https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01)
- [x] Remote Bitrate Estimation and Adaptation Algorithm
- [x] Sender side BitRate estimation
- [ ] [Flex FEC draft 3](https://tools.ietf.org/html/draft-ietf-payload-flexible-fec-scheme-03)
- [x] NACK and RTX support
- [x] [RTCP reduced size](https://tools.ietf.org/html/rfc5506)
- [x] Bundle
- [x] ICE lite
- [x] [Frame Marking](https://tools.ietf.org/html/draft-ietf-avtext-framemarking-04)
- [x] [PERC double encryption](https://tools.ietf.org/html/draft-ietf-perc-double-03)
- [x] Plain RTP broadcasting/streaming
- [ ] [Layer Refresh Request (LRR) RTCP Feedback Message](https://datatracker.ietf.org/doc/html/draft-ietf-avtext-lrr-04)
- [ ] MPEG DASH
- [ ] Datachannels (WIP via [libdatachannels](https://github.com/medooze/libdatachannels))

## Support
To discuss issues related to this project or ask for help please [join the google comunity group](https://groups.google.com/forum/#!forum/medooze).

## Contributing
To get started, [sign the Contributor License Agreement](https://www.clahub.com/agreements/medooze/media-server).

## License
Dual:
 - GNU General Public License v2.0 for general use.
 - MIT for using under the [Medooze Media Server Node.](https://github.com/medooze/media-server-node)
