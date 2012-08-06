/*
** g711.h
**
**
** u-law, A-law and linear PCM conversions.
*/

/*
** linear2alaw() - Convert a 16-bit linear PCM value to 8-bit A-law
**
*/
extern "C" unsigned char linear2alaw(short	pcm_val);

/*
** alaw2linear() - Convert an A-law value to 16-bit linear PCM
**
*/
extern "C" short  alaw2linear(unsigned char a_val);

/*
** linear2ulaw() - Convert a linear PCM value to u-law
**
*/

extern "C" unsigned char linear2ulaw(short	pcm_val);

/*
** ulaw2linear() - Convert a u-law value to 16-bit linear PCM
**
*/

extern "C" short ulaw2linear(unsigned char	u_val);

/*
** alaw2ulaw() - A-law to u-law conversion
**
*/

extern "C" unsigned char alaw2ulaw(unsigned char aval);

/*
** u-law to A-law conversioni
**
*/
extern "C" unsigned char ulaw2alaw(unsigned char uval);
