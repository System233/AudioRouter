// Copyright (c) 2022 github.com/System233
// 
// This software is released under the MIT License.
// https://opensource.org/licenses/MIT
#include "ikcp.h"
#include <cstdint>
class ikcp:public ikcpcb{
public:
  using output_fn=int(const char *buf, int len, ikcpcb *kcp, void *user);
  ikcp()=delete;
  ~ikcp()=delete;
  void release(){
    return ikcp_release(this);
  }
  void setoutput(output_fn output){
    return ikcp_setoutput(this,output);
  }
  int send(void const*buf,size_t len){
    return ikcp_send(this,static_cast<char const*>(buf),len);
  }
  int waitsnd(){
    return ikcp_waitsnd(this);
  }
  int wndsize(int sndwnd, int rcvwnd){
    return ikcp_wndsize(this,sndwnd,rcvwnd);
  }
  int setmtu(int mtu){
    return ikcp_setmtu(this,mtu);
  }
  template<class...Args>
  void log(int mask, const char *fmt, Args...args){
    ikcp_log(this,mask,fmt,std::forward<Args>(args)...);
  }
  int peeksize(){
    return ikcp_peeksize(this);
  }
  int input(void const*buf,size_t len){
    return ikcp_input(this,static_cast<char const*>(buf),len);
  }
  int nodelay(int nodelay, int interval, int resend, int nc){
    return ikcp_nodelay(this, nodelay, interval, resend, nc);
  }
  uint32_t check(uint32_t time){
    return ikcp_check(this, time);
  }
  void update(uint32_t time){
    ikcp_update(this, time);
  }
  int recv(void*buf,size_t len){
    return ikcp_recv(this,static_cast<char*>(buf),len);
  }
  uint32_t getconv(){
    return ikcp_getconv(this);
  }
  void flush(){
    ikcp_flush(this);
  }
  static ikcp*create(uint32_t conv_id,void*user){
    return static_cast<ikcp*>(ikcp_create(conv_id,user));
  }
  static void allocator(decltype(malloc) malloc,decltype(free) free){
    ikcp_allocator(malloc,free);
  }
};
template<>
class std::default_delete<ikcp>{
  void operator()(ikcp*pkcp){
    pkcp->release();
  }
};


#include <unordered_map>
#include <boost/array.hpp>
#include <boost/beast/core/span.hpp>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>
using boost::asio::ip::tcp;
using boost::asio::ip::udp;

class udp_server{
protected:
  boost::asio::io_context&m_io_context;
  udp::socket m_socket;
  udp::endpoint m_remote_endpoint;
  boost::array<char,0x10000> m_recv_buffer;
  virtual void start_recvice(){
      m_socket.async_receive_from(
        boost::asio::buffer(m_recv_buffer),
        m_remote_endpoint,
        boost::bind(&udp_server::handle_recvice,this,boost::asio::placeholders::error,boost::asio::placeholders::bytes_transferred)
      );
  }
  virtual void handle_recvice(boost::system::error_code const& error,size_t bytes_transferred){
    if(!error){
      udp_handle(boost::asio::const_buffer(m_recv_buffer.data(),bytes_transferred),m_remote_endpoint);
    }
    start_recvice();
  }
  virtual size_t udp_handle(boost::asio::const_buffer buffer,udp::endpoint const&endpoint){return 0;};
public:
  udp_server(boost::asio::io_context& io_context,udp::endpoint const&endpoint):m_io_context(io_context),m_socket(io_context,endpoint){
    
  }
  auto udp_send(boost::asio::const_buffer const&buffer,udp::endpoint const&endpoint){
    return m_socket.async_send_to(buffer,endpoint,boost::asio::use_future);
  }
  virtual void start(){
    start_recvice();
  }
};





class kcp_server:public udp_server{
public:
  class kcp_context{
    kcp_server&m_server;
    udp::endpoint m_endpoint;
    ikcp*m_kcp;
    boost::asio::steady_timer m_timer;
    std::chrono::system_clock::time_point m_time;
    bool m_alive;
    int async_output(const char *buf, int len, ikcpcb *kcp){
      m_server.udp_send(boost::asio::buffer(buf,len),m_endpoint);
      return len;
    }
    static int kcp_output(const char *buf, int len, ikcpcb *kcp, void *user){
      return reinterpret_cast<kcp_context*>(user)->async_output(buf, len, kcp);
    }
    public:
      uint32_t now(){
        return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
      }
      kcp_context(kcp_server&server,udp::endpoint endpoint)
        :m_server(server),m_endpoint(endpoint),m_kcp(nullptr),
        m_timer(server.m_io_context),
        m_alive(true),
        m_time(std::chrono::system_clock::now())
      {
        m_kcp=ikcp::create(server.conv_id(),this);
        m_kcp->setoutput(kcp_output);
        m_kcp->nodelay(1, 20, 2, 1);
      }
      kcp_context(kcp_context const&)=delete;
      kcp_context(kcp_context&&)=delete;
      int recv(boost::asio::mutable_buffer buffer)const{
        return m_kcp->recv(buffer.data(),buffer.size());
      }
      int send(boost::asio::const_buffer buffer)const{
        return m_kcp->send(buffer.data(),buffer.size());
      }
      int input(boost::asio::const_buffer buffer)const{
        return m_kcp->input(buffer.data(),buffer.size());
      }
      void update(uint32_t time)const{
        m_kcp->update(time);
      }
      uint32_t check(uint32_t time)const{
        return m_kcp->check(time);
      }
      auto ping(){
        m_time=std::chrono::system_clock::now();
        return m_time;
      }
      bool alive()const{
        auto delta=std::chrono::system_clock::now()-m_time;
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(delta);
        return m_alive&&ms.count()<m_server.timeout();
      }
      void stop(){
        m_alive=false;
        m_timer.cancel();
        m_timer.wait();
      }
      
      boost::asio::steady_timer& timer(){return m_timer;}
      ikcp*kcp()const{return m_kcp;}
      udp::endpoint endpoint()const{return m_endpoint;}
      ~kcp_context(){
        m_kcp->release();
      }
  };
private:
  uint32_t m_conv_id;
  size_t m_timeout;
  std::unordered_map<udp::endpoint,std::unique_ptr<kcp_context>>m_kcps;
  boost::array<char,0x10000>m_kcp_buffer;
  void check_kcp_recv(kcp_context*kcp){
    auto len=kcp->recv(boost::asio::mutable_buffer(m_kcp_buffer.data(),m_kcp_buffer.size()));
    if(len>0){
      kcp_handle(boost::asio::const_buffer(m_kcp_buffer.data(),len));
    }
  }
  size_t udp_handle(boost::asio::const_buffer buffer,udp::endpoint const&endpoint)override{
    auto kcp=find_endpoint(endpoint);
    if(!kcp){
        kcp=add_endpoint(endpoint);
        kcp_connect(kcp);
    }
    kcp->ping();
    auto len=kcp->input(buffer);
    check_kcp_recv(kcp);
    return len;
  }
public:
  kcp_server(boost::asio::io_context& io_context,udp::endpoint endpoint,uint32_t conv_id,size_t timeout=10000)
    :udp_server(io_context,endpoint),m_conv_id(conv_id),
      m_timeout(timeout)
  {
  }
  kcp_context*find_endpoint(udp::endpoint const&endpoint){
    auto it=m_kcps.find(endpoint);
    return it==m_kcps.end()?nullptr:it->second.get();
  }
  kcp_context*add_endpoint(udp::endpoint const&endpoint){
    if(m_kcps.count(endpoint)){
      return nullptr;
    }
    auto ctx=std::make_unique<kcp_context>(*this,endpoint);
    return m_kcps.emplace(endpoint,std::move(ctx)).first->second.get();
  }
  void set_timeout(size_t timeout){m_timeout=timeout;}
  size_t timeout()const{return m_timeout;}
  uint32_t conv_id()const{return m_conv_id;}

  int push(boost::asio::const_buffer buffer){
    int total=0;
    for(auto&kcp:m_kcps){
      total+=kcp.second->send(buffer);
    }
    return total;
  }
  void kcp_disconnect(kcp_context*kcp){
    handle_disconnect(kcp);
    kcp->stop();
    m_kcps.erase(kcp->endpoint());
  }
  void kcp_connect(kcp_context*kcp){
    kcp_loop(kcp);
    handle_connect(kcp);
  }
  void kcp_loop(kcp_context*kcp){
    if(!kcp->alive()){
      kcp_disconnect(kcp);
      return;
    }
    auto _now=kcp->now();
    kcp->update(_now);
    auto next=kcp->check(_now);
    auto duration=std::chrono::milliseconds(next-_now);
    kcp->timer().expires_from_now(duration);
    kcp->timer().async_wait(boost::bind(&kcp_server::kcp_loop,this,kcp));
  }
  void start()override{
    udp_server::start();
  }
  virtual void kcp_handle(boost::asio::const_buffer buffer){

  }
  virtual bool authorize(boost::asio::const_buffer key){
    return true;
  }
  virtual void initialize(kcp_context const*ctx){}
  virtual void handle_connect(kcp_context const*ctx){}
  virtual void handle_disconnect(kcp_context const*ctx){}
};








class kcp_client:public udp_server{

  uint32_t m_conv_id;
  size_t m_timeout;
  boost::array<char,0x10000>m_kcp_buffer;
  // boost::asio::steady_timer m_timer;
  

  class kcp_context{
    kcp_client&m_server;
    udp::endpoint m_endpoint;
    ikcp*m_kcp;
    boost::asio::steady_timer timer;
    std::chrono::system_clock::time_point m_time;
    int async_output(const char *buf, int len, ikcpcb *kcp){
      auto result=m_server.udp_send(boost::asio::buffer(buf,len),m_endpoint);
      result.wait();
      return result.get();
    }
    static int kcp_output(const char *buf, int len, ikcpcb *kcp, void *user){
      return reinterpret_cast<kcp_context*>(user)->async_output(buf, len, kcp);
    }
    static uint32_t now(){
      return std::chrono::time_point_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()).time_since_epoch().count();
    }
    public:
      kcp_context(kcp_client&server,udp::endpoint endpoint):m_server(server),m_endpoint(endpoint),m_kcp(nullptr),timer(server.m_io_context){
        m_kcp=ikcp::create(server.conv_id(),this);
        m_kcp->setoutput(kcp_output);
        m_kcp->nodelay(1, 20, 2, 1);
        loop();
      }
      int recv(boost::asio::mutable_buffer buffer){
        return m_kcp->recv(buffer.data(),buffer.size());
      }
      int send(boost::asio::const_buffer buffer){
        return m_kcp->send(buffer.data(),buffer.size());
      }
      int input(boost::asio::const_buffer buffer){
        return m_kcp->input(buffer.data(),buffer.size());
      }
      void update(uint32_t time){
        m_kcp->update(time);
      }
      uint32_t check(uint32_t time){
        return m_kcp->check(time);
      }
      auto ping(){
        m_time=std::chrono::system_clock::now();
        return m_time;
      }
      bool alive(){
        auto delta=std::chrono::system_clock::now()-m_time;
        auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(delta);
        return ms.count()<m_server.timeout();
      }
      udp::endpoint endpoint()const{return m_endpoint;}
      ikcp*kcp()const{return m_kcp;}
      void loop(){
        auto _now=now();
        m_kcp->update(_now);
        auto next=m_kcp->check(_now);
        auto duration=std::chrono::milliseconds(next-_now);
        timer.expires_from_now(duration);
        timer.async_wait(boost::bind(&kcp_context::loop,this));
      }
      ~kcp_context(){
        timer.cancel();
        m_kcp->release();
      }
  };

  kcp_context m_kcp;
  void check_kcp_recv(kcp_context*kcp){
    auto len=kcp->recv(boost::asio::mutable_buffer(m_kcp_buffer.data(),m_kcp_buffer.size()));
    if(len>0){
      kcp_handle(boost::asio::const_buffer(m_kcp_buffer.data(),len));
    }
  }
  size_t udp_handle(boost::asio::const_buffer buffer,udp::endpoint const&endpoint)override{
    m_kcp.ping();
    auto len=m_kcp.input(buffer);
    check_kcp_recv(&m_kcp);
    return len;
  }
public:
  kcp_client(boost::asio::io_context& io_context,udp::endpoint endpoint,udp::endpoint server_endpoint,uint32_t conv_id,size_t timeout=1000)
    :udp_server(io_context,endpoint),m_conv_id(conv_id),
      // m_timer(io_context),
      m_kcp(*this,server_endpoint),
      m_timeout(timeout)
  {
  }
  void set_timeout(size_t timeout){m_timeout=timeout;}
  size_t timeout()const{return m_timeout;}
  uint32_t conv_id()const{return m_conv_id;}
  kcp_context const&context()const{return m_kcp;}
  
  void start()override{
    udp_server::start();
    initialize();
  }
  virtual void initialize(){}
  virtual void kcp_handle(boost::asio::const_buffer buffer){}
};