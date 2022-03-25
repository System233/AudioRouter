
//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <ctime>
#include <iostream>
#include <string>
#include "ikcp.hpp"
#include "audio.hpp"
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
class client:public kcp_client{
    boost::asio::steady_timer m_req_timer;
    IAudioClient2 *pAudioClient = NULL;
    IAudioRenderClient *pRenderClient = NULL;
    UINT32 numBufferFrames=0;
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
        std::cout<<"buffer:"<<bufferPadding<<'/'<<numBufferFrames<<"      \r";
        std::memcpy(pData,buffer.data(),buffer.size());
        pRenderClient->ReleaseBuffer(currentFrames,0);
      }
    }
  }
  void stopAudioDevice(){
    if(pRenderClient){
      pAudioClient->Stop();
      pRenderClient->Release();
      pRenderClient=NULL;
    }
  }
  void initAudioDevice(AudioFormat*fmt){
    stopAudioDevice();
    auto wfx=fmt->toWFX();
    pAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_RATEADJUST  | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY,
        10000000,0,&wfx,NULL
    );
    pAudioClient->GetBufferSize(&numBufferFrames);
    pAudioClient->GetService(__uuidof(IAudioRenderClient),(LPVOID*)&pRenderClient);
    pAudioClient->Start();
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
