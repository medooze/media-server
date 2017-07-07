/* 
 * File:   recorder.h
 * Author: Sergio
 *
 * Created on 27 de junio de 2013, 10:45
 */

#ifndef RECORDERCONTROL_H
#define	RECORDERCONTROL_H


class RecorderControl
{
public:
	enum Type {FLV, MP4};
public:
	virtual bool Create(const char *filename) = 0;
	virtual bool Record() = 0;
	virtual bool Record(bool waitVideo) = 0;
	virtual bool Stop() = 0;
	virtual bool Close() = 0;
	virtual Type GetType() = 0;
};

#endif	/* RECORDER_H */

