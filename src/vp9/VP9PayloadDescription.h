/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   VP9PayloadDescription.h
 * Author: Sergio
 *
 * Created on 1 de febrero de 2017, 9:46
 */

#ifndef VP9PAYLOADDESCRIPTION_H
#define VP9PAYLOADDESCRIPTION_H
#include "config.h"
#include "tools.h"
#include "log.h"
#include <vector>
#include <map>


/*
 *  RTP Payload Format for VP9 Video
 *    https://tools.ietf.org/html/draft-ietf-payload-vp9-02#ref-I-D.ietf-avtext-lrr
 */

struct VP9InterPictureDependency
{
	
	/*
	T: The temporal layer ID of current frame.  In the case of non-
	   flexible mode, if PID is mapped to a frame in a specified GOF,
	   then the value of T MUST match the corresponding T value of the
	   mapped frame in the GOF.
	*/	
	BYTE temporalLayerId;

	/*
       U: Switching up point.  If this bit is set to 1 for the current
         frame with temporal layer ID equal to T, then "switch up" to a
         higher frame rate is possible as subsequent higher temporal
         layer frames will not depend on any frame before the current
         frame (in coding time) with temporal layer ID greater than T.
	*/	
	bool switchingPoint;
	
	/*
	P_DIFF:  The reference index (in 7 bits) specified as the relative
	  PID from the current frame.  For example, when P_DIFF=3 on a
	  packet containing the frame with PID 112 means that the frame
	  refers back to the frame with PID 109.  This calculation is
	  done modulo the size of the PID field, i.e., either 7 or 15
	  bits.
	 */
	std::vector<BYTE> referenceIndexDiff;
	
	
	VP9InterPictureDependency();
	DWORD GetSize();
	DWORD Parse(const BYTE* data, DWORD size);

	DWORD Serialize(BYTE *data,DWORD size);
	void Dump();
	
};
struct VP9ScalabilityScructure
{
	/*
	  N_S:  N_S + 1 indicates the number of spatial layers present in the
	    VP9 stream.
	 */
	BYTE numberSpatialLayers;
	/*	
	Y: Each spatial layer's frame resolution present.  When set to one,
	   the OPTIONAL WIDTH (2 octets) and HEIGHT (2 octets) MUST be
	   present for each layer frame.  Otherwise, the resolution MUST NOT
	   be present.
	 */
	bool spatialLayerFrameResolutionPresent;
	/*
	 G: GOF description present flag.
	 */
	bool groupOfFramesDescriptionPresent;

	/*
	-: Bit reserved for future use.  MUST be set to zero and MUST be
	     ignored by the receiver.	 
	 NOTE: spatialLayerFrameResolutions.size()==numberSpatialLayers
	 */
	std::vector<std::pair<WORD,WORD>> spatialLayerFrameResolutions;
	
	/*
	N_G:  N_G indicates the number of frames in a GOF.  If N_G is greater
	  than 0, then the SS data allows the inter-picture dependency
	  structure of the VP9 stream to be pre-declared, rather than
	  indicating it on the fly with every packet.  If N_G is greater
	  than 0, then for N_G pictures in the GOF, each frame's temporal
	  layer ID (T), switch up point (U), and the R reference indices
	  (P_DIFFs) are specified.

	  The very first frame specified in the GOF MUST have T set to 0.

	  G set to 0 or N_G set to 0 indicates that either there is only one
	  temporal layer or no fixed inter-picture dependency information is
	  present going forward in the bitstream.

	  Note that for a given super frame, all layer frames follow the
	  same inter-picture dependency structure.  However, the frame rate
	  of each spatial layer can be different from each other and this
	  can be controlled with the use of the D bit described above.  The
	  specified dependency structure in the SS data MUST be for the
	  highest frame rate layer.	 
	*/
	std::vector<VP9InterPictureDependency> groupOfFramesDescription;
	
	VP9ScalabilityScructure();
	DWORD GetSize();
	DWORD Parse(const BYTE* data, DWORD size);

	DWORD Serialize(BYTE *data,DWORD size);
	void Dump();
};

struct VP9PayloadDescription
{
	/*
	I: Picture ID (PID) present.  When set to one, the OPTIONAL PID MUST
	   be present after the mandatory first octet and specified as below.
	   Otherwise, PID MUST NOT be present.
	 */
	bool pictureIdPresent;
	/*
	P: Inter-picture predicted layer frame.  When set to zero, the layer
	   frame does not utilize inter-picture prediction.  In this case,
	   up-switching to current spatial layer's frame is possible from
	   directly lower spatial layer frame.  P SHOULD also be set to zero
	   when encoding a layer synchronization frame in response to an LRR
	   [I-D.ietf-avtext-lrr] message (see Section 5.4).  When P is set to
	   zero, the T bit (described below) MUST also be set to 0 (if
	   present).	 
	 */
	bool interPicturePredictedLayerFrame;
	/*
	L: Layer indices present.  When set to one, the one or two octets
	   following the mandatory first octet and the PID (if present) is as
	   described by "Layer indices" below.  If the F bit (described
	   below) is set to 1 (indicating flexible mode), then only one octet
	   is present for the layer indices.  Otherwise if the F bit is set
	   to 0 (indicating non-flexible mode), then two octets are present
	   for the layer indices.	
	 */
	bool layerIndicesPresent;
	/*
	F: Flexible mode.  F set to one indicates flexible mode and if the P
	     bit is also set to one, then the octets following the mandatory
	     first octet, the PID, and layer indices (if present) are as
	     described by "Reference indices" below.  This MUST only be set to
	     1 if the I bit is also set to one; if the I bit is set to zero,
	     then this MUST also be set to zero and ignored by receivers.  The
	     value of this F bit CAN ONLY CHANGE on the very first packet of a
	     key picture.  This is a packet with the P bit equal to zero, S or
	     D bit (described below) equal to zero, and B bit (described below)
	     equal to 1.
	*/	
	bool flexibleMode;
	/*
	  B: Start of a layer frame.  MUST be set to 1 if the first payload
	      octet of the RTP packet is the beginning of a new VP9 layer frame,
	      and MUST NOT be 1 otherwise.  Note that this layer frame might not
	      be the very first layer frame of a super frame.

	 */	
	bool startOfLayerFrame;
	/*
	E: End of a layer frame.  MUST be set to 1 for the final RTP packet
	   of a VP9 layer frame, and 0 otherwise.  This enables a decoder to
	   finish decoding the layer frame, where it otherwise may need to
	   wait for the next packet to explicitly know that the layer frame
	   is complete.  Note that, if spatial scalability is in use, more
	   layer frames from the same super frame may follow; see the
	   description of the M bit above.	 
	 */
	bool endOfLayerFrame;
	/*
	V: Scalability structure (SS) data present.  When set to one, the
	   OPTIONAL SS data MUST be present in the payload descriptor.
	   Otherwise, the SS data MUST NOT be present.
	 */
	bool scalabiltiyStructureDataPresent;
	/*
	-: Bit reserved for future use.  MUST be set to zero and MUST be
	   ignored by the receiver.
	 */
	bool reserved;
	
	/*	
	   M: The most significant bit of the first octet is an extension flag.
	      The field MUST be present if the I bit is equal to one.  If set,
	      the PID field MUST contain 15 bits; otherwise, it MUST contain 7
	      bits.  See PID below.
	*/
	bool extendedPictureIdPresent;
	
	/*
	Picture ID (PID):  Picture ID represented in 7 or 15 bits, depending
	   on the M bit.  This is a running index of the pictures.  The field
	   MUST be present if the I bit is equal to one.  If M is set to
	   zero, 7 bits carry the PID; else if M is set to one, 15 bits carry
	   the PID in network byte order.  The sender may choose between a 7-
	   or 15-bit index.  The PID SHOULD start on a random number, and
	   MUST wrap after reaching the maximum ID.  The receiver MUST NOT
	   assume that the number of bits in PID stay the same through the
	   session.

	   In the non-flexible mode (when the F bit is set to 0), this PID is
	   used as an index to the GOF specified in the SS data bleow.  In
	   this mode, the PID of the key frame corresponds to the very first
	   specified frame in the GOF.  Then subsequent PIDs are mapped to
	   subsequently specified frames in the GOF (modulo N_G, specified in
	   the SS data below), respectively.
	 */
	WORD pictureId;

	/*
	T: The temporal layer ID of current frame.  In the case of non-
	   flexible mode, if PID is mapped to a frame in a specified GOF,
	   then the value of T MUST match the corresponding T value of the
	   mapped frame in the GOF.
	*/	
	BYTE temporalLayerId;

	/*
       U: Switching up point.  If this bit is set to 1 for the current
         frame with temporal layer ID equal to T, then "switch up" to a
         higher frame rate is possible as subsequent higher temporal
         layer frames will not depend on any frame before the current
         frame (in coding time) with temporal layer ID greater than T.
	*/	
	bool switchingPoint;
	
	/*
	S: The spatial layer ID of current frame.  Note that frames with
           spatial layer S > 0 may be dependent on decoded spatial layer
           S-1 frame within the same super frame.
	 */
	BYTE spatialLayerId;
	/*
	D: Inter-layer dependency used.  MUST be set to one if current
	   spatial layer S frame depends on spatial layer S-1 frame of the
	   same super frame.  MUST only be set to zero if current spatial
	   layer S frame does not depend on spatial layer S-1 frame of the
	   same super frame.  For the base layer frame with S equal to 0,
	   this D bit MUST be set to zero.
	*/
	bool interlayerDependencyUsed;
	/*
	TL0PICIDX:  8 bits temporal layer zero index.  TL0PICIDX is only
	  present in the non-flexible mode (F = 0).  This is a running
	  index for the temporal base layer frames, i.e., the frames with
	  T set to 0.  If T is larger than 0, TL0PICIDX indicates which
	  temporal base layer frame the current frame depends on.
	  TL0PICIDX MUST be incremented when T is equal to 0.  The index
	  SHOULD start on a random number, and MUST restart at 0 after
	  reaching the maximum number 255.
	*/	
	BYTE temporalLayer0Index;
	
	/*
	P_DIFF:  The reference index (in 7 bits) specified as the relative
	  PID from the current frame.  For example, when P_DIFF=3 on a
	  packet containing the frame with PID 112 means that the frame
	  refers back to the frame with PID 109.  This calculation is
	  done modulo the size of the PID field, i.e., either 7 or 15
	  bits.
	 */
	std::vector<BYTE> referenceIndexDiff;
	
	/*
	The scalability structure (SS) data describes the resolution of each
	  layer frame within a super frame as well as the inter-picture
	  dependencies for a group of frames (GOF).  If the VP9 payload
	  descriptor's "V" bit is set, the SS data is present in the position
	  indicated in Figure 2 and Figure 3.	 
	 */
	VP9ScalabilityScructure scalabilityStructure;

	VP9PayloadDescription();
	DWORD GetSize();
	DWORD Parse(const BYTE* data, DWORD size);

	DWORD Serialize(BYTE *data,DWORD size);
	void Dump();
};

#endif /* VP9PAYLOADDESCRIPTION_H */

