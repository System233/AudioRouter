
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
using boost::asio::ip::tcp;
using boost::asio::ip::udp;
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);

class server:public kcp_server{
  IAudioCaptureClient *mCaptureClient = NULL;
  IAudioClient *mAudioClient = NULL;
  WAVEFORMATEX *pwfx = NULL;
  boost::asio::steady_timer m_req_timer;
  UINT32 numBufferFrames;
public:
  server(boost::asio::io_context& io_context,udp::endpoint endpoint,uint32_t conv_id,size_t timeout=10000)
  :kcp_server(io_context,endpoint,conv_id,timeout),
  m_req_timer(io_context){

  }
  virtual void kcp_handle(kcp_context const* ctx,boost::asio::const_buffer buffer)override{
    // std::cout<<"server recv"<<std::endl;
    AudioFormat format(pwfx);
    ctx->send(boost::asio::buffer(&format,sizeof(format)));
    // ctx->send(boost::asio::buffer("hello",6));
  }
  virtual void handle_connect(kcp_context const*ctx)override{
    std::cout<<"connect:"<<ctx->endpoint()<<std::endl;
  }
  virtual void handle_disconnect(kcp_context const*ctx)override{
    std::cout<<"disconnect:"<<ctx->endpoint()<<std::endl;
  }
  
  virtual void initialize()override{
    
    IMMDeviceEnumerator *pEnumerator = NULL;
    IMMDevice *pDevice = NULL;
    IAudioClient *pAudioClient = NULL;
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    CoCreateInstance(CLSID_MMDeviceEnumerator,NULL,CLSCTX_ALL,IID_IMMDeviceEnumerator,(LPVOID*)&pEnumerator);
    pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole,&pDevice);
    pDevice->Activate(IID_IAudioClient,CLSCTX_ALL,NULL,(LPVOID*)&mAudioClient);
    mAudioClient->GetMixFormat(&pwfx);
    mAudioClient->Initialize(
        AUDCLNT_SHAREMODE_SHARED,
        AUDCLNT_STREAMFLAGS_LOOPBACK,
        5000000,0,pwfx,NULL
    );
    mAudioClient->GetBufferSize(&numBufferFrames);
    mAudioClient->GetService(IID_IAudioCaptureClient,(LPVOID*)&mCaptureClient);
    mAudioClient->Start();
    pEnumerator->Release();
    pDevice->Release();
    start_loop();
  }
  void loop(){
    UINT32 packetSize;
    BYTE *pData;
    DWORD flags;
    UINT32 numBufferFrames;
    UINT32 numFramesAvailable;
    mCaptureClient->GetNextPacketSize(&packetSize);
    while(packetSize>0){
      mCaptureClient->GetBuffer(&pData,&numFramesAvailable,&flags,NULL,NULL);
      send_all(boost::asio::const_buffer(pData,numFramesAvailable*(pwfx->nBlockAlign)));
      mCaptureClient->ReleaseBuffer(numFramesAvailable);
      mCaptureClient->GetNextPacketSize(&packetSize);
    }
    start_loop();
  }
  void start_loop(){
    m_req_timer.expires_from_now(std::chrono::milliseconds(1));
    m_req_timer.async_wait(std::bind(&server::loop,this));
  }
  ~server(){
    mAudioClient->Stop();
    mCaptureClient->Release();
    mAudioClient->Release();
  }
};
int main()
{
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
  try
  {
    boost::asio::io_context io_context;
    server kcp(io_context,udp::endpoint(udp::v4(),10000),1);
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
