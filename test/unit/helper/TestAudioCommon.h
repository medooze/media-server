#ifndef TESTAUDIOCOMMON_H
#define	TESTAUDIOCOMMON_H

#include <utility>
#include <fftw3.h>
#include <cmath>

static std::pair<double,double> findPeakFrequency(float* data, int sampleRate, int numSamples)
{
    fftw_complex* in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*numSamples);
    fftw_complex* out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex)*numSamples);
    fftw_plan plan = fftw_plan_dft_1d(numSamples, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    for(int i=0;i<numSamples;i++)
    {
        //printf("data=%f\n", data[i]);
        in[i][0] = data[i];
        
        in[i][1] = 0.0;
    }

    fftw_execute(plan);
    int peakIdx = 0;
    double peakMagnitude = 0.0;
    for(int i=0;i<numSamples/2;i++)
    {
        double magnitude = sqrt(out[i][0]*out[i][0] + out[i][1]*out[i][1]);
        if (magnitude > peakMagnitude)
        {
            peakMagnitude = magnitude;
            peakIdx = i;
        }
    }
    double peakFreq = peakIdx * sampleRate / numSamples;

    fftw_destroy_plan(plan);
    fftw_free(in);
    fftw_free(out);
    return {peakFreq, peakMagnitude/numSamples*2.0};
}
#endif	/* TESTAUDIOCOMMON_H */