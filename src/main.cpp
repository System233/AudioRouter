
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

using boost::asio::ip::tcp;
using boost::asio::ip::udp;

class server:public kcp_server{
public:
  server(boost::asio::io_context& io_context,udp::endpoint endpoint,uint32_t conv_id,size_t timeout=1000)
  :kcp_server(io_context,endpoint,conv_id,timeout){

  }
  virtual void kcp_handle(boost::asio::const_buffer buffer)override{
    std::cout<<"server recv"<<std::endl;
  }
  virtual bool authorize(boost::asio::const_buffer key)override{
    std::cout<<"authorize:"<<(char const*)key.data()<<std::endl;
    return !std::strcmp("hello",(char const*)key.data());
  }
  virtual void initialize(kcp_context const*ctx)override{
    ctx->send(boost::asio::buffer("hello",6));
  }
  virtual void handle_connect(kcp_context const*ctx)override{
    ctx->send(boost::asio::buffer("hello",6));
    std::cout<<"connect:"<<ctx->endpoint()<<std::endl;
  }
  virtual void handle_disconnect(kcp_context const*ctx)override{
    std::cout<<"disconnect:"<<ctx->endpoint()<<std::endl;
  }
};
int main()
{
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

  return 0;
}
