/* 
 * File:   VP9PayloadDescription.cpp
 * Author: Sergio
 * 
 * Created on 1 de febrero de 2017, 9:46
 */

#include "VP9PayloadDescription.h"



/*
        +-+-+-+-+-+-+-+-+                           
   N_G: |  T  |U| R |-|-| (OPTIONAL)                 
        +-+-+-+-+-+-+-+-+              -\           
        |    P_DIFF     | (OPTIONAL)    . - R times 
        +-+-+-+-+-+-+-+-+              -/           
 */

VP9InterPictureDependency::VP9InterPictureDependency()
{
	temporalLayerId = 0;
	switchingPoint = false;
}

DWORD VP9InterPictureDependency::GetSize()
{
	return 1 + referenceIndexDiff.size();
}

DWORD VP9InterPictureDependency::Parse(const BYTE* data, DWORD size)
{
	//Check size
	if (size<1)
		return 0;
	//Get values
	temporalLayerId = data[0] >> 5;
	switchingPoint  = data[0] & 0x10;
	//Number of pdifs
	BYTE pdifs = data[0] >> 2 & 0x03;
	//Check size
	if (size<pdifs+1u)
		return 0;
	//Get each one
	for (BYTE j=0;j<pdifs;++j)
		//Get it
		referenceIndexDiff.push_back(data[j+1]);
	//Return length
	return 1+pdifs;
}

DWORD VP9InterPictureDependency::Serialize(BYTE *data,DWORD size)
{
	//Check size
	if (size<GetSize())
		//Error
		return 0;
	
	//Doubel check size
	if (referenceIndexDiff.size()>3)
		//Error
		return 0;
	
	//Set number of frames in gof
	data[0] = temporalLayerId;
	data[0] = data[0]<<1 | switchingPoint;
	data[0] = data[0]<<2 | referenceIndexDiff.size() ;
	//Reserved
	data[0] = data[0] << 2;
	
	//Len
	DWORD len = 1;
	
	//Get each one
	for (auto it = referenceIndexDiff.begin(); it!=referenceIndexDiff.end(); ++it)
	{
		//Get it
		data[len] = *it;
		//Inc
		len ++;
	}
	
	//Return length
	return len;
}

void VP9InterPictureDependency::Dump()
{
	Debug("\t\t[VP9InterPictureDependency temporalLayerId=%d switchingPoint=%d]\n",temporalLayerId,switchingPoint);
	for (auto it = referenceIndexDiff.begin(); it!=referenceIndexDiff.end(); ++it)
		Debug("\t\t\t[ReferenceIndex diff:%d/]\n",*it);
	Debug("\t\t[VP9InterPictureDependency/]\n");
}

/*
        +-+-+-+-+-+-+-+-+
   V:   | N_S |Y|G|-|-|-|
        +-+-+-+-+-+-+-+-+              -\
   Y:   |     WIDTH     | (OPTIONAL)    .
        +               +               .
        |               | (OPTIONAL)    .
        +-+-+-+-+-+-+-+-+               . - N_S + 1 times
        |     HEIGHT    | (OPTIONAL)    .
        +               +               .
        |               | (OPTIONAL)    .
        +-+-+-+-+-+-+-+-+              -/            
   G:   |      N_G      | (OPTIONAL)
        +-+-+-+-+-+-+-+-+                          
	|...............|  N_G * VP9InterPictureDependency
 	|...............|
        +-+-+-+-+-+-+-+-+                          
 */

VP9ScalabilityScructure::VP9ScalabilityScructure()
{
	numberSpatialLayers = 0;
	spatialLayerFrameResolutionPresent = false;
	groupOfFramesDescriptionPresent = false;
}

DWORD VP9ScalabilityScructure::GetSize()
{
	//Heder
	DWORD len =  1;
	
	//If we have spatial resolutions
	if (spatialLayerFrameResolutionPresent)
		//Increase length
		len += spatialLayerFrameResolutions.size()*4;
	
	//Is gof description present
	if (groupOfFramesDescriptionPresent)
	{
		//Inc len
		len ++;
		//Fore each  oen
		for (auto it = groupOfFramesDescription.begin(); it!=groupOfFramesDescription.end(); ++it)
			//Inc length
			len += it->GetSize();
	}	
	
	//REturn length
	return len;
}

DWORD VP9ScalabilityScructure::Parse(const BYTE* data, DWORD size)
{
	//Check kength
	if (size<1)
		//Error
		return 0;
	
	//Parse header
	numberSpatialLayers			= (data[0] >> 5) + 1;
	spatialLayerFrameResolutionPresent	= data[0] & 0x10;
	groupOfFramesDescriptionPresent		= data[0] & 0x08;
	
	//Heder
	DWORD len =  1;
	
	//If we have spatial resolutions
	if (spatialLayerFrameResolutionPresent)
	{
		//Endusre lenght
		if (len+numberSpatialLayers*4>size)
			//Error
			return 0;
		
		//For each spatial layer
		for (BYTE i=0;i<numberSpatialLayers;++i)
		{
			//Get dimensions
			WORD width = get2(data,len);
			WORD height = get2(data,len+2);
			//Append
			spatialLayerFrameResolutions.push_back(std::make_pair(width,height));
			//Inc length
			len += 4;
			
		}
	}
	
	//Is gof description present
	if (groupOfFramesDescriptionPresent)
	{
		//Get number of frames in group
		BYTE n = data[len];
		//Inc len
		len ++;
		//Fore each  one
		for (BYTE i=0; i<n;++i)
		{
			//Dependency
			VP9InterPictureDependency dependency;
			//Parse it
			DWORD l = dependency.Parse(data+len,size-len);
			//If failed
			if (!l)
				//Error
				return 0;
			//Append
			groupOfFramesDescription.push_back(dependency);
			//Inc lenght
			len += l;
		}
	}	
	
	//REturn length
	return len;
}

DWORD VP9ScalabilityScructure::Serialize(BYTE *data,DWORD size)
{
	//Check size
	if (size<GetSize())
		//Error
		return 0;
	
	//Serialize header
	data[0] = numberSpatialLayers-1;
	data[0] = data[0] << 1 | spatialLayerFrameResolutionPresent;
	data[0] = data[0] << 1 | groupOfFramesDescriptionPresent;
	//Reserved
	data[0] = data[0] << 3;
	//Heder
	DWORD len =  1;
	
	//If we have spatial resolutions
	if (spatialLayerFrameResolutionPresent)
	{
		//For each spatial layer
		for (auto it = spatialLayerFrameResolutions.begin(); it!= spatialLayerFrameResolutions.end(); ++it)
		{
			//Set dimensions
			set2(data,len,it->first);
			set2(data,len+2,it->second);
			::Dump(data+len,4);
			//Inc length
			len += 4;
		}
	}
	
	//Is gof description present
	if (groupOfFramesDescriptionPresent)
	{
		//Set number of grames
		data[len] = groupOfFramesDescription.size();
		//Inc len
		len ++;
		//Fore each  one
		for (auto it = groupOfFramesDescription.begin(); it!=groupOfFramesDescription.end(); ++it)
		{
			//Serialize it
			DWORD l = it->Serialize(data+len,size-len);
			//If failed
			if (!l)
				//Error
				return 0;
			//Inc lenght
			len += l;
		}
	}	
	
	//REturn length
	return len;
}
void VP9ScalabilityScructure::Dump()
{
	Debug("\t[VP9ScalabilityScructure \n");
	Debug("\t\t numberSpatialLayers=%d\n"			, numberSpatialLayers);
	Debug("\t\t spatialLayerFrameResolutionPresent=%d\n"	, spatialLayerFrameResolutionPresent);
	Debug("\t\t groupOfFramesDescriptionPresent=%d\n"	, groupOfFramesDescriptionPresent);
	Debug("\t]\n");
	if (spatialLayerFrameResolutionPresent)
		for (auto it = spatialLayerFrameResolutions.begin(); it!= spatialLayerFrameResolutions.end(); ++it)
			Debug("\t\t[SpatialLayerFrameResolution width=%d height=%d/]\n",it->first,it->second);
	if (groupOfFramesDescriptionPresent)
		for (auto it = groupOfFramesDescription.begin(); it!=groupOfFramesDescription.end(); ++it)
			it->Dump();
	Debug("\t[VP9ScalabilityScructure/]\n");
}

/*
   In flexible mode (with the F bit below set to 1), The first octets
   after the RTP header are the VP9 payload descriptor, with the
   following structure.
 
         0 1 2 3 4 5 6 7
        +-+-+-+-+-+-+-+-+
        |I|P|L|F|B|E|V|-| (REQUIRED)
        +-+-+-+-+-+-+-+-+
   I:   |M| PICTURE ID  | (REQUIRED)
        +-+-+-+-+-+-+-+-+
   M:   | EXTENDED PID  | (RECOMMENDED)
        +-+-+-+-+-+-+-+-+
   L:   |  T  |U|  S  |D| (CONDITIONALLY RECOMMENDED)
        +-+-+-+-+-+-+-+-+                             -\
   P,F: | P_DIFF      |N| (CONDITIONALLY REQUIRED)    - up to 3 times
        +-+-+-+-+-+-+-+-+                             -/
   V:   | SS            |
        | ..            |
        +-+-+-+-+-+-+-+-+

   In non-flexible mode (with the F bit below set to 0), The first
   octets after the RTP header are the VP9 payload descriptor, with the
   following structure.

         0 1 2 3 4 5 6 7
        +-+-+-+-+-+-+-+-+
        |I|P|L|F|B|E|V|-| (REQUIRED)
        +-+-+-+-+-+-+-+-+
   I:   |M| PICTURE ID  | (RECOMMENDED)
        +-+-+-+-+-+-+-+-+
   M:   | EXTENDED PID  | (RECOMMENDED)
        +-+-+-+-+-+-+-+-+
   L:   |  T  |U|  S  |D| (CONDITIONALLY RECOMMENDED)
        +-+-+-+-+-+-+-+-+
        |   TL0PICIDX   | (CONDITIONALLY REQUIRED)
        +-+-+-+-+-+-+-+-+
   V:   | SS            |
        | ..            |
        +-+-+-+-+-+-+-+-+
 
 */

VP9PayloadDescription::VP9PayloadDescription()
{
	pictureIdPresent = 0;
	extendedPictureIdPresent = 0;
	interPicturePredictedLayerFrame = 0;
	layerIndicesPresent = 0;
	flexibleMode = 0;
	startOfLayerFrame = 0;
	endOfLayerFrame = 0;
	scalabiltiyStructureDataPresent = 0;
	reserved = false;
	pictureId = 0;
	temporalLayerId = 0;
	switchingPoint = 0;
	spatialLayerId = 0;
	interlayerDependencyUsed = 0;
	temporalLayer0Index = 0;
}


DWORD VP9PayloadDescription::GetSize()
{
	//Heder
	DWORD len =  1;
	
	if (pictureIdPresent)
		//2 or 1 byte?
		len += extendedPictureIdPresent ? 2 : 1;
	
	//if we have layer index
	if (layerIndicesPresent)
	{
		//Inc
		len ++;
		//Non-flexible
		if (!flexibleMode)
			//TL0PICIDX
			len += 1;
	}
	
	if (flexibleMode && pictureIdPresent)
		//n P diffs
		len += referenceIndexDiff.size();
	
	if (scalabiltiyStructureDataPresent)
	{
		//Get SS size
		DWORD l = scalabilityStructure.GetSize();
		//Check
		if (!l)
			//Return error
			return 0;
		//Inc
		len += l;
	}
	
	//REturn length
	return len;
}

DWORD VP9PayloadDescription::Parse(const BYTE* data, DWORD size)
{
	//Check kength
	if (size<1)
		//Error
		return 0;
	
	//Parse header
	pictureIdPresent		= data[0] >> 7 & 0x01;
	interPicturePredictedLayerFrame = data[0] >> 6 & 0x01;
	layerIndicesPresent		= data[0] >> 5 & 0x01;
	flexibleMode			= data[0] >> 4 & 0x01;
	startOfLayerFrame		= data[0] >> 3 & 0x01;
	endOfLayerFrame			= data[0] >> 2 & 0x01;
	scalabiltiyStructureDataPresent = data[0] >> 1 & 0x01;
	reserved			= data[0]      & 0x01;
	
	//Heder
	DWORD len =  1;
	
	//Check pictrure id
	if (pictureIdPresent)
	{
		//Get extended bit p
		extendedPictureIdPresent = data[len]>>7;
		//Get picture id
		if (extendedPictureIdPresent)
		{
			//Check kength
			if (size<len+2)
				//Error
				return 0;
			//15 bits
			pictureId = get2(data,len) & 0x7FFF;
			//Inc len
			len += 2;
		} else {
			//Check kength
			if (size<len+1)
				//Error
				return 0;
			//1 bit
			pictureId = data[len];
			//Inc len
			len ++;
		}
		
	}
	
	//if we have layer index
	if (layerIndicesPresent)
	{
		//Check kength
		if (size<len+1)
			//Error
			return 0;
		//Get indices
		temporalLayerId	 	 = data[len] >> 5;
		switchingPoint		 = data[len] & 0x10;
		spatialLayerId		 = (data[len] >> 1 ) & 0x07;
		interlayerDependencyUsed = data[len] & 0x01; 
		//Inc
		len ++;
		
		//Only on nonf flexible mode
		if (!flexibleMode)
		{
			//Check kength
			if (size<len+1)
				//Error
				return 0;
			//TL0PICIDX
			temporalLayer0Index = data[len];
			//Inc length
			len += 1;	
		}
	}
		
	if (flexibleMode && interPicturePredictedLayerFrame)
	{
		//at least 1
		bool next = true;
		//Check last diff mark
		while (next)
		{
			//Check length
			if (size<len+1)
				//Error
				return 0;
			//Add ref index
			referenceIndexDiff.push_back(data[len]>>1);
			//Are there more?
			next = data[len] & 0x01;
			
			//Inc len
			len ++;
		}	
	}
	
	if (scalabiltiyStructureDataPresent)
	{
		//Get SS size
		DWORD l = scalabilityStructure.Parse(data+len,size-len);
		//Check
		if (!l)
			//Return error
			return 0;
		//Inc
		len += l;
	}
	
	//REturn length
	return len;
}

DWORD VP9PayloadDescription::Serialize(BYTE *data,DWORD size)
{
	//Check kength
	if (size<GetSize())
		//Error
		return 0;
	
	//Parse header
	data[0] = pictureIdPresent;
	data[0] = data[0] << 1 | interPicturePredictedLayerFrame;
	data[0] = data[0] << 1 | layerIndicesPresent;
	data[0] = data[0] << 1 | flexibleMode;
	data[0] = data[0] << 1 | startOfLayerFrame;
	data[0] = data[0] << 1 | endOfLayerFrame;
	data[0] = data[0] << 1 | scalabiltiyStructureDataPresent;
	data[0] = data[0] << 1 | reserved;
	
	//Heder
	DWORD len =  1;
	
	//Check pictrure id
	if (pictureIdPresent)
	{
		//Check size
		if (extendedPictureIdPresent)
		{
			//1 bit mark + 15 bits 
			set2(data,len,pictureId);
			//Set martk
			data[len] = data[len] | 0x80;
			//Inc len
			len += 2;
		} else {
			//1 bit
			data[len] = pictureId & 0x7F;
			//Inc len
			len ++;
		}
		
	}
	
	//if we have layer index
	if (layerIndicesPresent)
	{
		//Get indices
		data[len] = temporalLayerId;
		data[len] = data[len] << 1 | switchingPoint;
		data[len] = data[len] << 3 | (spatialLayerId & 0x03);
		data[len] = data[len] << 1 | interlayerDependencyUsed;
		//Inc
		len ++;
		//If not on flexible mode
		if (!flexibleMode)
		{
			//TL0PICIDX
			 data[len] = temporalLayer0Index;
			//Inc length
			len += 1;	
		}
	}
		
        if (flexibleMode && interPicturePredictedLayerFrame)
        {
                bool next = true;
                //Check last diff mark
                while (next)
                {
                        //Check length
                        if (size<len+1)
                                //Error
                                return 0;
                        //Add ref index
                        referenceIndexDiff.push_back(data[len]>>1);
                        //Are there more?
                        next = data[len] & 0x01;

                        //Inc len
                        len ++;
                }
        }


	if (scalabiltiyStructureDataPresent)
	{
		//Get SS size
		DWORD l = scalabilityStructure.Serialize(data+len,size-len);
		//Check
		if (!l)
			//Return error
			return 0;
		//Inc
		len += l;
	}
	
	//REturn length
	return len;
}

void VP9PayloadDescription::Dump()
{
	Debug("[VP9PayloadDescription \n");
	Debug("\t pictureIdPresent=%d\n"		, pictureIdPresent);
	Debug("\t extendedPictureIdPresent=%d\n"	, extendedPictureIdPresent);
	Debug("\t interPicturePredictedLayerFrame=%d\n"	, interPicturePredictedLayerFrame);
	Debug("\t layerIndicesPresent=%d\n"		, layerIndicesPresent);
	Debug("\t flexibleMode=%d\n"			, flexibleMode);
	Debug("\t startOfLayerFrame=%d\n"		, startOfLayerFrame);
	Debug("\t endOfLayerFrame=%d\n"			, endOfLayerFrame);
	Debug("\t scalabiltiyStructureDataPresent=%d\n"	, scalabiltiyStructureDataPresent);
	Debug("\t pictureId=%d\n"			, pictureId);
	Debug("\t temporalLayerId=%d\n"			, temporalLayerId);
	Debug("\t switchingPoint=%u\n"			, switchingPoint);
	Debug("\t spatialLayerId=%d\n"			, spatialLayerId);
	Debug("\t interlayerDependencyUsed=%d\n"	, interlayerDependencyUsed);
	Debug("\t temporalLayer0Index=%d\n"		, temporalLayer0Index);
	Debug("]\n");
	if (flexibleMode && pictureIdPresent)
		for (auto it=referenceIndexDiff.begin(); it!=referenceIndexDiff.end(); ++it)
			Debug("\t[Reference diff=%d/]\n",*it);
	if (scalabiltiyStructureDataPresent)
		scalabilityStructure.Dump();
	Debug("[VP9PayloadDescription/]\n");
}

/*
uncompressed_header()
{
	frame_marker f(2)
	profile_low_bit f(1)
	profile_high_bit f(1)
	Profile = (profile_high_bit << 1) + profile_low_bit
	if (Profile == 3)
		reserved_zero f(1)
	show_existing_frame f(1)
	if (show_existing_frame == 1)
	{
		frame_to_show_map_idx f(3)
		header_size_in_bytes = 0
		refresh_frame_flags = 0
		loop_filter_level = 0
		return
	}
	LastFrameType = frame_type
	frame_type f(1)
	show_frame f(1)
	error_resilient_mode f(1)
	if (frame_type == KEY_FRAME)
	{
		frame_sync_code()
		color_config()
		frame_size()
		render_size()
		refresh_frame_flags = 0xFF
		FrameIsIntra = 1
	}
	else
	{
		if (show_frame == 0)
		{
			intra_only f(1)
		}
		else
		{
			intra_only = 0
		}
		FrameIsIntra = intra_only
		if (error_resilient_mode == 0)
		{
			reset_frame_context f(2)
		}
		else
		{
			reset_frame_context = 0
		}
		if (intra_only == 1)
		{
			frame_sync_code()
			if (Profile > 0)
			{
				color_config()
			}
			else
			{
				color_space = CS_BT_601
				subsampling_x = 1
				subsampling_y = 1
				BitDepth = 8
			}
			refresh_frame_flags f(8)
			frame_size()
			render_size()
		}
		else
		{
			refresh_frame_flags f(8)
			for (i = 0; i < 3; i++)
			{
				ref_frame_idx[i] f(3)
				ref_frame_sign_bias[LAST_FRAME + i] f(1)
			}
			frame_size_with_refs()
			allow_high_precision_mv f(1)
			read_interpolation_filter()
		}
	}
	if (error_resilient_mode == 0)
	{
		refresh_frame_context f(1)
		frame_parallel_decoding_mode f(1)
	}
	else
	{
		refresh_frame_context = 0
		frame_parallel_decoding_mode = 1
	}
	frame_context_idx f(2)
	if (FrameIsIntra || error_resilient_mode)
	{
		setup_past_independence()
		if (frame_type == KEY_FRAME || error_resilient_mode == 1 ||
			reset_frame_context == 3)
		{
			for (i = 0; i < 4; i++)
			{
				save_probs(i)
			}
		}
		else if (reset_frame_context == 2)
		{
			save_probs(frame_context_idx)
		}
		frame_context_idx = 0
	}
	loop_filter_params()
	quantization_params()
	segmentation_params()
	tile_info()
	header_size_in_bytes f(16)
}
 
 */
