#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <vector>


using boost::asio::ip::tcp;

class RtmpSession : public std::enable_shared_from_this<RtmpSession> {
public:
    explicit RtmpSession(tcp::socket socket);
    void start();

private:
    void do_handshake();
    void on_c1_read(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void on_s2_written(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void on_c2_read(const boost::system::error_code& ec, std::size_t bytes_transferred);
    
    void do_read();
    void on_message_read(const boost::system::error_code& ec, std::size_t bytes_transferred);

    // New method to handle the connect command and send a reply
    void send_connect_response();

    tcp::socket socket_;
    std::array<unsigned char, 8192> buffer_;
    std::vector<unsigned char> handshake_buffer_;
    
    bool connect_response_sent_ = false; 
};