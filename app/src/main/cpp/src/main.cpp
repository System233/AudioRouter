//
// Created by master on 2022/3/21.
//

#include <jni.h>
/*

template<class T,class R>
auto cast(T value){
    return reinterpret_cast<T>(value);
}

extern "C" JNIEXPORT jlong JNICALL
Java_com_github_system233_audiorouter_KCP_create(JNIEnv *env, jclass thiz, jint conv) {
    // TODO: implement create()
    return (jlong)new ikcp::ukcp(conv);
}
extern "C"
JNIEXPORT void JNICALL
Java_com_github_system233_audiorouter_KCP_flush(JNIEnv *env, jclass thiz, jlong ctx) {
    // TODO: implement flush()
    reinterpret_cast<ikcp::ukcp*>(ctx)->flush();
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_github_system233_audiorouter_KCP_send(JNIEnv *env, jclass thiz, jlong ctx,
                                               jbyteArray buf) {
    // TODO: implement recv()
    auto ptr=env->GetByteArrayElements(buf, nullptr);
    auto len=reinterpret_cast<ikcp::ukcp*>(ctx)->recv(ptr, env->GetArrayLength(buf));
    env->ReleaseByteArrayElements(buf,ptr,JNI_ABORT);
    return len;
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_github_system233_audiorouter_KCP_recv(JNIEnv *env, jclass thiz, jlong ctx,
                                               jbyteArray buf) {
    // TODO: implement recv()
    auto ptr=env->GetByteArrayElements(buf,nullptr);
    auto len=reinterpret_cast<ikcp::ukcp*>(ctx)->recv(ptr, env->GetArrayLength(buf));
    env->ReleaseByteArrayElements(buf,ptr,JNI_COMMIT);
    return len;
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_github_system233_audiorouter_KCP_connect(JNIEnv *env, jclass thiz, jlong ctx, jstring ip,
                                                  jint port) {
    // TODO: implement connect()
    auto addr=getString(env,ip);
    return reinterpret_cast<ikcp::ukcp*>(ctx)->connect(net::make_addr(addr,port));
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_github_system233_audiorouter_KCP_bind(JNIEnv *env, jclass thiz, jlong ctx, jstring ip,
                                               jint port) {
    auto addr=getString(env,ip);
    return reinterpret_cast<ikcp::ukcp*>(ctx)->bind(net::make_addr(addr,port));
}
extern "C"
JNIEXPORT void JNICALL
Java_com_github_system233_audiorouter_KCP_release(JNIEnv *env, jclass thiz, jlong ctx) {
    delete reinterpret_cast<ikcp::ukcp*>(ctx);
}
extern "C"
JNIEXPORT jint JNICALL
Java_com_github_system233_audiorouter_KCP_loop(JNIEnv *env, jclass thiz, jlong ctx) {
    return reinterpret_cast<ikcp::ukcp*>(ctx)->loop();
}

*/


//
// server.cpp
// ~~~~~~~~~~
//
// Copyright (c) 2003-2021 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include<android/log.h>
#include <ctime>
#include <iostream>
#include <string>

#include <ikcp.hpp>
#define TAG "KCP"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,TAG ,__VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,TAG ,__VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,TAG ,__VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,TAG ,__VA_ARGS__)
#define LOGF(...) __android_log_print(ANDROID_LOG_FATAL,TAG ,__VA_ARGS__)
//#include "audio.hpp"
static JavaVM* g_vm;
static jmethodID m_callback;
static jfieldID m_instance;
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
class client:public kcp_client{
    boost::asio::steady_timer m_req_timer,m_ping_timer;
    JNIEnv *m_env;
    jobject m_thiz;
    bool m_connected=false;
    int m_ping=0;
public:
  client(boost::asio::io_context& io_context,udp::endpoint endpoint,udp::endpoint server_endpoint,uint32_t conv_id,size_t timeout=10000)
  :kcp_client(io_context,endpoint,server_endpoint,conv_id,timeout),m_req_timer(io_context),m_ping_timer(io_context){}
    ~client(){
      m_env->DeleteGlobalRef(m_thiz);
    }
  void set_env(JNIEnv *env,jobject thiz){
    env->GetJavaVM(&g_vm);
    g_vm->GetEnv((void**)&m_env,JNI_VERSION_1_6);
    m_thiz=m_env->NewGlobalRef(thiz);
    auto clazz=m_env->GetObjectClass(thiz);
    m_instance=m_env->GetFieldID(clazz,"instance","J");
    m_env->SetLongField(thiz,m_instance,(jlong)this);
    m_callback=m_env->GetMethodID(clazz,"handle","([B)V");
    m_env->DeleteLocalRef(clazz);
    LOGD("m_callback %p",m_callback);
    LOGD("m_instance %p",m_instance);
  }
  static bool test(boost::asio::const_buffer buffer){
    return buffer.size()>4&&(*(int*)buffer.data()==0x12345678);
  }
  static uint64_t now(){return time(NULL);}
  void stop(){
    m_io_context.stop();
  }
  void kcp_handle(kcp_context const* kcp,boost::asio::const_buffer buffer)override{
//    LOGD("kcp_handle %p",buffer.size());
    if(auto pkg=PingPacket::check(buffer)){
      m_ping=now()-pkg->time;
      return;
    }
    m_connected=true;
    auto buf=m_env->NewByteArray(buffer.size());
    auto ptr=m_env->GetByteArrayElements(buf,nullptr);
    std::memcpy(ptr,buffer.data(),buffer.size());
    m_env->ReleaseByteArrayElements(buf,ptr,JNI_COMMIT);
//    auto clazz=m_env->GetObjectClass(m_thiz);
//    auto m_callback=m_env->GetMethodID(clazz,"handle","([B)V");
    m_env->CallVoidMethod(m_thiz,m_callback,buf);
    m_env->DeleteLocalRef(buf);
//    m_env->DeleteLocalRef(clazz);
  }
  void request(){
    if(m_connected){
      return;
    }
    context()->send(boost::asio::buffer("format",5));
    m_req_timer.expires_from_now(std::chrono::milliseconds(1000));
    m_req_timer.async_wait(std::bind(&client::request,this));
  }
  void start_ping(){
    PingPacket pkt;
    pkt.time=now();
    context()->send(boost::asio::buffer(&pkt,sizeof(pkt)));
    m_ping_timer.expires_from_now(std::chrono::milliseconds(100));
    m_req_timer.async_wait(std::bind(&client::start_ping,this));
  }
  void initialize()override{

    LOGD("initialize");
  }
};
//int main()
//{
//  try
//  {
//    boost::asio::io_context io_context;
//    client kcp(io_context,udp::endpoint(),udp::endpoint(boost::asio::ip::make_address("127.0.0.1"),10000),1);
//    kcp.start();
//    io_context.run();
//  }
//  catch (std::exception& e)
//  {
//    std::cerr << e.what() << std::endl;
//  }
//  return 0;
//}
std::string getString(JNIEnv*env,jstring str){
  auto len=env->GetStringUTFLength(str);
  auto ptr=env->GetStringUTFChars(str,nullptr);
  std::string data(ptr,len);
  env->ReleaseStringUTFChars(str,ptr);
  return data;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_github_system233_audiorouter_AudioService_start(JNIEnv *env, jobject thiz, jstring host, jint port) {
  LOGD("Start");
  try
  {
    std::string addr=getString(env,host);
    LOGD("Server %s:%d",addr.c_str(),port);
    boost::asio::io_context io_context;
    client kcp(io_context,udp::endpoint(),udp::endpoint(boost::asio::ip::make_address(addr),port),1);
    kcp.set_env(env,thiz);
    kcp.start();
    io_context.run();
    LOGD("Server exit");
  }
  catch (std::exception& e)
  {
    LOGD("ERROR %s",e.what());
    std::cerr << e.what() << std::endl;
  }
}
extern "C"
JNIEXPORT void JNICALL
Java_com_github_system233_audiorouter_AudioService_stop(JNIEnv *env, jobject thiz) {
  // TODO: implement stop()
  auto s=(client*)env->GetLongField(thiz,m_instance);
  s->stop();
  LOGD("Stop");
}