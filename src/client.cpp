// Copyright (c) 2022 github.com/System233
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
#include <sdkddkver.h>
#include <ctime>
#include <iostream>
#include <string>
#include "ikcp.hpp"
#include "audio.hpp"
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <aacdecoder_lib.h>
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
class client:public kcp_client{
    boost::asio::steady_timer m_req_timer;
    IAudioClient2 *pAudioClient = NULL;
    IAudioRenderClient *pRenderClient = NULL;
    UINT32 numBufferFrames=0;
    boost::array<short,0x10000>m_aac_buffer;
    HANDLE_AACDECODER mDecoder;
public:
  client(boost::asio::io_context& io_context,udp::endpoint endpoint,udp::endpoint server_endpoint,uint32_t conv_id,size_t timeout=10000)
  :kcp_client(io_context,endpoint,server_endpoint,conv_id,timeout),m_req_timer(io_context){}
  void kcp_handle(kcp_context const* kcp,boost::asio::const_buffer buffer)override{
    auto format_test=(AudioFormat*)buffer.data();
    if(format_test->check()){
      initAudioDevice(format_test);
    }else if(pRenderClient){

      UINT32 bufferPadding=0;
      BYTE *pData=NULL;
      WAVEFORMATEX*pwfx;
      pAudioClient->GetMixFormat(&pwfx);
      UINT32 currentFrames=buffer.size()/pwfx->nBlockAlign;
      pAudioClient->GetCurrentPadding(&bufferPadding);
      pRenderClient->GetBuffer(currentFrames,&pData);
      if(pData){
        // auto pcm=decode(buffer);
        std::cout<<"buffer:"<<bufferPadding<<'/'<<numBufferFrames<<"      \r";
        std::memcpy(pData,buffer.data(),buffer.size());
        pRenderClient->ReleaseBuffer(currentFrames,0);
      }
    }
  }
  boost::asio::const_buffer decode(boost::asio::const_buffer buffer){
    uint8_t *buf=(uint8_t*)buffer.data();
    unsigned int num=buffer.size();
    unsigned int validBytes=num;
    auto err=aacDecoder_Fill(mDecoder,&buf,&num,&validBytes);
    err=aacDecoder_DecodeFrame(mDecoder,m_aac_buffer.data(),m_aac_buffer.size(),0);
    auto info=aacDecoder_GetStreamInfo(mDecoder);
    auto len=info->frameSize*info->numChannels;
    return boost::asio::const_buffer(m_aac_buffer.data(),len);
  }
  void stopAudioDevice(){
    if(pRenderClient){
      pAudioClient->Stop();
      pRenderClient->Release();
      pRenderClient=NULL;
      aacDecoder_Close(mDecoder);
    }
  }
  void initAudioDevice(AudioFormat*fmt){
    stopAudioDevice();
    auto wfx=fmt->toWFX();
    wfx.wFormatTag=WAVE_FORMAT_PCM;
    wfx.nBlockAlign=4;
    wfx.wBitsPerSample=16;
    wfx.nAvgBytesPerSec=wfx.nBlockAlign*wfx.nSamplesPerSec;
    pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_RATEADJUST  | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        10000000,0,&wfx,NULL
    );
    pAudioClient->GetBufferSize(&numBufferFrames);
    pAudioClient->GetService(__uuidof(IAudioRenderClient),(LPVOID*)&pRenderClient);
    pAudioClient->Start();
    
    mDecoder=aacDecoder_Open(TRANSPORT_TYPE::TT_MP4_ADTS,1);
    aacDecoder_SetParam(mDecoder,AACDEC_PARAM::AAC_PCM_MAX_OUTPUT_CHANNELS,fmt->channels);
    aacDecoder_SetParam(mDecoder,AACDEC_PARAM::AAC_PCM_MIN_OUTPUT_CHANNELS,fmt->channels);
    aacDecoder_SetParam(mDecoder,AACDEC_PARAM::AAC_PCM_OUTPUT_CHANNEL_MAPPING,1);
    
  }
  ~client(){
    stopAudioDevice();
    pAudioClient->Release();
  }
  void initialize()override{
    context()->send(boost::asio::buffer("format",5));

    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    WAVEFORMATEXTENSIBLE wfx;
    CoCreateInstance(CLSID_MMDeviceEnumerator,NULL,CLSCTX_ALL,IID_IMMDeviceEnumerator,(LPVOID*)&pEnumerator);
    pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole,&pDevice);
    pDevice->Activate(__uuidof(IAudioClient2),CLSCTX_ALL,NULL,(LPVOID*)&pAudioClient);
    
    pEnumerator->Release();
    pDevice->Release();

    return;
  }
};
int main(int argc,char**argv)
{
  if(argc<3){
    std::cout<<argv[0]<<" [ip] [port]"<<std::endl;
    return 0;
  }
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
  try
  {
    std::cout<<"connect to "<<argv[1]<<':'<<argv[2]<<std::endl;
    boost::asio::io_context io_context;
    client kcp(io_context,udp::endpoint(),udp::endpoint(boost::asio::ip::make_address(argv[1]),atoi(argv[2])),1);
    kcp.start();
    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

    CoUninitialize();
  return 0;
}
