#pragma once
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

class RtmpServer {
public:
    RtmpServer(boost::asio::io_context& io_context, short port);

private:
    void do_accept();

    tcp::acceptor acceptor_;
};