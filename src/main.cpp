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
using boost::asio::ip::tcp;
using boost::asio::ip::udp;
#include <mmsystem.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);
struct PingPacket{
    char name[8]={'p','i','n','g'};
    uint64_t time;
    static PingPacket*check(boost::asio::const_buffer buffer){
        static PingPacket test;
        if(buffer.size()==sizeof(PingPacket)&&!std::strncmp(test.name,(char const*)buffer.data(),sizeof(test.name))){
            return (PingPacket*)(buffer.data());
        }
        return nullptr;
    }
};
#include <aacenc_lib.h>
class server:public kcp_server{
  IAudioCaptureClient *mCaptureClient = NULL;
  IAudioClient *mAudioClient = NULL;
  WAVEFORMATEX *pwfx = NULL;
  boost::asio::steady_timer m_req_timer;
  UINT32 numBufferFrames;
  HANDLE_AACENCODER mEncoder;
  boost::array<char,0x10000>m_aac_buffer;
public:
  server(boost::asio::io_context& io_context,udp::endpoint endpoint,uint32_t conv_id,size_t timeout=10000)
  :kcp_server(io_context,endpoint,conv_id,timeout),
  m_req_timer(io_context){

  }
  
  virtual void kcp_handle(kcp_context const* ctx,boost::asio::const_buffer buffer)override{
    // std::cout<<"server recv"<<std::endl;
    if(auto pkt=PingPacket::check(buffer)){
        ctx->send(buffer);
        return;
    }
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

    aacEncOpen(&mEncoder,0,pwfx->nChannels);
    aacEncoder_SetParam(mEncoder,AACENC_PARAM::AACENC_BITRATEMODE,5);
    aacEncoder_SetParam(mEncoder,AACENC_PARAM::AACENC_SAMPLERATE,pwfx->nSamplesPerSec);
    aacEncoder_SetParam(mEncoder,AACENC_PARAM::AACENC_CHANNELMODE,pwfx->nChannels);
    aacEncoder_SetParam(mEncoder,AACENC_PARAM::AACENC_TRANSMUX,2);
    aacEncEncode(mEncoder,nullptr,nullptr,nullptr,nullptr);//INIT
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
      // auto aac=encode(boost::asio::const_buffer(pData,numFramesAvailable*(pwfx->nBlockAlign)));
      send_all(boost::asio::const_buffer(pData,numFramesAvailable*(pwfx->nBlockAlign)));
      mCaptureClient->ReleaseBuffer(numFramesAvailable);
      mCaptureClient->GetNextPacketSize(&packetSize);
    }
    start_loop();
  }
  boost::asio::const_buffer encode(boost::asio::const_buffer pcm_buffer){
    AACENC_OutArgs outArgs;
    AACENC_InArgs inArgs;
    AACENC_BufDesc inBufferDesc,outBufferDesc;
    /*
    int inbufId[]={AACENC_BufferIdentifier::IN_AUDIO_DATA};
    int inbufElSizes[]={pwfx->nBlockAlign/pwfx->nChannels,sizeof(*m_ancillary_buffer.data()),sizeof(metaDataSetup)};
    int inbufSizes[]={pcm_buffer.size()/(pwfx->nBlockAlign/pwfx->nChannels),m_ancillary_buffer.size(),1};
    void *inbufs[]={(void*)pcm_buffer.data(),m_ancillary_buffer.data(),&metaDataSetup};
    inBufferDesc.numBufs=sizeof(inbufs)/sizeof(void*);
    inBufferDesc.bufElSizes=inbufElSizes;
    inBufferDesc.bufSizes=inbufSizes;
    inBufferDesc.bufs=inbufs;
    inBufferDesc.bufferIdentifiers=inbufId;
    */
    int inbufId[]={AACENC_BufferIdentifier::IN_AUDIO_DATA};
    int inbufElSizes[]={pwfx->nBlockAlign/pwfx->nChannels};
    int inbufSizes[]={(int)pcm_buffer.size()/(pwfx->nBlockAlign/pwfx->nChannels)};
    void *inbufs[]={(void*)pcm_buffer.data()};
    inBufferDesc.numBufs=sizeof(inbufs)/sizeof(void*);
    inBufferDesc.bufElSizes=inbufElSizes;
    inBufferDesc.bufSizes=inbufSizes;
    inBufferDesc.bufs=inbufs;
    inBufferDesc.bufferIdentifiers=inbufId;

    int outbufId[]={AACENC_BufferIdentifier::OUT_BITSTREAM_DATA };
    int outbufElSizes[]={sizeof(*m_aac_buffer.data())};
    int outbufSizes[]={(int)m_aac_buffer.size()};
    void *outbufs[]={(void*)m_aac_buffer.data()};
    outBufferDesc.numBufs=sizeof(outbufs)/sizeof(void*);
    outBufferDesc.bufElSizes=outbufElSizes;
    outBufferDesc.bufSizes=outbufSizes;
    outBufferDesc.bufs=outbufs;
    outBufferDesc.bufferIdentifiers=outbufId;
    inArgs.numInSamples=pcm_buffer.size()/pwfx->nBlockAlign;
    inArgs.numAncBytes=0;
    auto err=aacEncEncode(mEncoder,&inBufferDesc,&outBufferDesc,&inArgs,&outArgs);
    return boost::asio::const_buffer(m_aac_buffer.data(),outArgs.numOutBytes);
  }
  void start_loop(){
    m_req_timer.expires_from_now(std::chrono::milliseconds(1));
    m_req_timer.async_wait(std::bind(&server::loop,this));
  }
  ~server(){
    mAudioClient->Stop();
    mCaptureClient->Release();
    mAudioClient->Release();
    aacEncClose(&mEncoder);
  }
};
int main(int argc,char**argv)
{
  int port=10000;
  if(argc==2){
    port=atoi(argv[1]);
  } else if(argc>2){
    std::cout<<"Usage:\nkcp_s [port]"<<std::endl;
    return 0;
  }
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
  try
  {
    std::cout<<"listening on ::"<<port<<std::endl;
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
