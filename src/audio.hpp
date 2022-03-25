// Copyright (c) 2022 github.com/System233
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
#include <mmsystem.h>
#include <mmreg.h>
struct AudioFormat{
    int id=0x12345678;
    int samplesPerSec;
    int bitsPerSample;
    int channels;
    int format;
    AudioFormat(WAVEFORMATEX*pwfx){
        samplesPerSec=pwfx->nSamplesPerSec;
        bitsPerSample=pwfx->wBitsPerSample;
        channels=pwfx->nChannels;
        format=0;
    }
    bool check(){
        return id==0x12345678;
    }
    WAVEFORMATEX toWFX(){
        WAVEFORMATEX wfx;
        wfx.nSamplesPerSec=samplesPerSec;
        wfx.wBitsPerSample=bitsPerSample;
        wfx.nChannels=channels;
        wfx.cbSize=0;//sizeof(WAVEFORMATEX);
        wfx.nBlockAlign=bitsPerSample*channels/8;
        wfx.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;
        wfx.nAvgBytesPerSec=wfx.nBlockAlign*samplesPerSec;
        return wfx;
    }
};