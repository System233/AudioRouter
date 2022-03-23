
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
#include "ikcp.hpp"
class client:public kcp_client{
    boost::asio::steady_timer m_req_timer;
public:
  client(boost::asio::io_context& io_context,udp::endpoint endpoint,udp::endpoint server_endpoint,uint32_t conv_id,size_t timeout=1000)
  :kcp_client(io_context,endpoint,server_endpoint,conv_id,timeout),m_req_timer(io_context){}
  void kcp_handle(boost::asio::const_buffer buffer)override{
      std::cout<<"success"<<std::endl;
      m_req_timer.cancel();
  }
  void initialize()override{
    std::cout<<"say hello"<<std::endl;
    udp_send(boost::asio::buffer("hello",5),context().endpoint());
    m_req_timer.expires_from_now(std::chrono::seconds(1));
    m_req_timer.async_wait(std::bind(&client::initialize,this));
    return;
  }
};
int main()
{
  try
  {
    boost::asio::io_context io_context;
    client kcp(io_context,udp::endpoint(),udp::endpoint(boost::asio::ip::make_address("127.0.0.1"),10000),1);
    kcp.start();
    io_context.run();
  }
  catch (std::exception& e)
  {
    std::cerr << e.what() << std::endl;
  }

  return 0;
}
