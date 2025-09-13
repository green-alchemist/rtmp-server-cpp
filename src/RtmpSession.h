#pragma once
#include <boost/asio.hpp>
#include <memory>
#include <vector>
#include <map>
#include <functional>

using boost::asio::ip::tcp;

struct ChunkStreamContext {
    std::vector<unsigned char> message_payload;
    uint32_t message_length = 0;
    uint8_t message_type_id = 0;
    uint32_t message_stream_id = 0;
};

class RtmpSession : public std::enable_shared_from_this<RtmpSession> {
public:
    explicit RtmpSession(tcp::socket socket);
    void start();

private:
    void do_handshake();
    void on_c1_read(const boost::system::error_code& ec, std::size_t bytes_transferred, std::shared_ptr<std::vector<unsigned char>> handshake_buffer);
    void on_s2_written(const boost::system::error_code& ec, std::size_t bytes_transferred);
    void on_c2_read(const boost::system::error_code& ec, std::size_t bytes_transferred);
    
    void do_read();
    void on_message_read(const boost::system::error_code& ec, std::size_t bytes_transferred);

    void on_message_complete(ChunkStreamContext& context, int csid);
    void handle_command_message(const std::vector<unsigned char>& payload, int csid, uint32_t message_stream_id);
    
    // Specific response methods with chaining
    void send_connect_response(int csid, double transaction_id);
    void send_create_stream_response(int csid, double transaction_id);
    void send_publish_response(int csid, uint32_t message_stream_id);
    void send_user_control_message(uint16_t event_type, uint32_t value);
    void send_window_ack_size(uint32_t size, std::function<void()> next_step);
    void send_peer_bandwidth(uint32_t size, uint8_t limit_type, std::function<void()> next_step);
    void set_chunk_size(std::function<void()> next_step);


    tcp::socket socket_;
    std::array<unsigned char, 8192> buffer_;
    size_t buffer_pos_ = 0;
    size_t buffer_size_ = 0;
    uint32_t incoming_chunk_size_ = 128;
    uint32_t outgoing_chunk_size_ = 4096;


    std::map<int, ChunkStreamContext> chunk_streams_;
};

