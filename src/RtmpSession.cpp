#include "RtmpSession.h"
#include <iostream>
#include <iomanip>
#include <random>
#include <algorithm>
#include <boost/endian/conversion.hpp>

// Helper function to create a properly formatted RTMP chunk
std::shared_ptr<std::vector<unsigned char>> create_chunk(int csid, uint32_t timestamp, uint8_t msg_type_id, uint32_t msg_stream_id, const std::vector<unsigned char>& payload) {
    auto chunk = std::make_shared<std::vector<unsigned char>>();
    
    // Basic Header (FMT = 0)
    chunk->push_back((0x00 << 6) | csid);

    // Message Header (11 bytes)
    uint32_t ts_big = boost::endian::native_to_big(timestamp);
    uint8_t* ts_bytes = reinterpret_cast<uint8_t*>(&ts_big);
    chunk->insert(chunk->end(), {ts_bytes[1], ts_bytes[2], ts_bytes[3]});

    uint32_t payload_size_big = boost::endian::native_to_big((uint32_t)payload.size());
    uint8_t* ps_bytes = reinterpret_cast<uint8_t*>(&payload_size_big);
    chunk->insert(chunk->end(), {ps_bytes[1], ps_bytes[2], ps_bytes[3]});

    chunk->push_back(msg_type_id);
    
    uint32_t msid_little = boost::endian::native_to_little(msg_stream_id);
    uint8_t* msid_bytes = reinterpret_cast<uint8_t*>(&msid_little);
    chunk->insert(chunk->end(), {msid_bytes[0], msid_bytes[1], msid_bytes[2], msid_bytes[3]});

    // Payload
    chunk->insert(chunk->end(), payload.begin(), payload.end());
    
    return chunk;
}


RtmpSession::RtmpSession(tcp::socket socket) 
    : socket_(std::move(socket))
{}

void RtmpSession::start() {
    std::cout << "[INFO] Starting new session..." << std::endl;
    do_handshake();
}

// --- Handshake Methods Implementation ---
void RtmpSession::do_handshake() {
    auto self = shared_from_this();
    auto handshake_buffer = std::make_shared<std::vector<unsigned char>>(1537);
    boost::asio::async_read(socket_, boost::asio::buffer(*handshake_buffer),
        [this, self, handshake_buffer](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            on_c1_read(ec, bytes_transferred, handshake_buffer);
        });
}

void RtmpSession::on_c1_read(const boost::system::error_code& ec, std::size_t bytes_transferred, std::shared_ptr<std::vector<unsigned char>> handshake_buffer) {
    if (ec) {
        std::cerr << "[ERROR] Handshake: C1 read failed: " << ec.message() << std::endl;
        return;
    }
    std::cout << "[INFO] Handshake: C1 received (" << bytes_transferred << " bytes)." << std::endl;
    
    unsigned char client_version = (*handshake_buffer)[0];
    
    auto response_buffer = std::make_shared<std::vector<unsigned char>>();
    response_buffer->reserve(1 + 1536 + 1536);
    response_buffer->push_back(client_version);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);
    for(int i = 0; i < 1536; ++i) { response_buffer->push_back(distrib(gen)); }

    response_buffer->insert(response_buffer->end(), handshake_buffer->begin() + 1, handshake_buffer->end());
    
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(*response_buffer),
        [this, self, response_buffer](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            on_s2_written(ec, bytes_transferred);
        });
}

void RtmpSession::on_s2_written(const boost::system::error_code& ec, std::size_t bytes_transferred) {
     if (ec) {
        std::cerr << "[ERROR] Handshake: S2 write failed: " << ec.message() << std::endl;
        return;
    }
    std::cout << "[INFO] Handshake: S2 sent (" << bytes_transferred << " bytes)." << std::endl;
    
    auto c2_buffer = std::make_shared<std::vector<unsigned char>>(1536);
    auto self = shared_from_this();
    boost::asio::async_read(socket_, boost::asio::buffer(*c2_buffer),
         [this, self, c2_buffer](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            on_c2_read(ec, bytes_transferred);
        });
}

void RtmpSession::on_c2_read(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "[ERROR] Handshake: C2 read failed: " << ec.message() << std::endl;
        return;
    }
    std::cout << "[INFO] Handshake: C2 received (" << bytes_transferred << " bytes). Handshake complete." << std::endl;
    do_read();
}
// --- End of Handshake Methods ---


void RtmpSession::do_read() {
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(buffer_),
        [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            on_message_read(ec, bytes_transferred);
        });
}

void RtmpSession::on_message_read(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (!ec) {
        buffer_pos_ = 0;
        buffer_size_ = bytes_transferred;

        while (buffer_pos_ < buffer_size_) {
             if (buffer_size_ - buffer_pos_ < 1) break;
            uint8_t basic_header = buffer_[buffer_pos_];
            int fmt = basic_header >> 6;
            int csid = basic_header & 0x3F;
            buffer_pos_++;

            ChunkStreamContext& context = chunk_streams_[csid];

            if (fmt == 0) {
                if (buffer_size_ - buffer_pos_ < 11) break;
                context.message_length = (buffer_[buffer_pos_ + 3] << 16) | (buffer_[buffer_pos_ + 4] << 8) | buffer_[buffer_pos_ + 5];
                context.message_type_id = buffer_[buffer_pos_ + 6];
                context.message_stream_id = boost::endian::load_little_u32(&buffer_[buffer_pos_ + 7]);
                buffer_pos_ += 11;
            } else if (fmt == 1) {
                if (buffer_size_ - buffer_pos_ < 7) break;
                context.message_length = (buffer_[buffer_pos_ + 3] << 16) | (buffer_[buffer_pos_ + 4] << 8) | buffer_[buffer_pos_ + 5];
                context.message_type_id = buffer_[buffer_pos_ + 6];
                buffer_pos_ += 7;
            } else if (fmt == 2) {
                 if (buffer_size_ - buffer_pos_ < 3) break;
                 buffer_pos_ += 3;
            } else { // fmt == 3
                // No header, reuse previous.
            }

            size_t bytes_to_read = std::min((size_t)context.message_length - context.message_payload.size(), (size_t)incoming_chunk_size_);
            bytes_to_read = std::min(bytes_to_read, buffer_size_ - buffer_pos_);
            
            context.message_payload.insert(context.message_payload.end(), buffer_.begin() + buffer_pos_, buffer_.begin() + buffer_pos_ + bytes_to_read);
            buffer_pos_ += bytes_to_read;
            
            if (context.message_payload.size() >= context.message_length) {
                on_message_complete(context, csid);
                context.message_payload.clear();
            }
        }
        
        do_read();
    } else if (ec != boost::asio::error::eof) {
        std::cerr << "[ERROR] Read error: " << ec.message() << std::endl;
    } else {
        std::cout << "[INFO] Client disconnected." << std::endl;
    }
}

void RtmpSession::on_message_complete(ChunkStreamContext& context, int csid) {
    std::cout << "[INFO] Completed a message on CSID " << csid
              << " of type " << (int)context.message_type_id
              << " with size " << context.message_length << " bytes." << std::endl;

    switch(context.message_type_id) {
        case 1: // Set Chunk Size
            incoming_chunk_size_ = boost::endian::load_big_u32(context.message_payload.data());
            std::cout << "[INFO] Client requested chunk size: " << incoming_chunk_size_ << std::endl;
            break;
        case 5: // Window Acknowledgement Size
            // Client is acknowledging our window size. No action needed.
            break;
        case 20: // AMF0 Command
             handle_command_message(context.message_payload, csid, context.message_stream_id);
             break;
    }
}

void RtmpSession::handle_command_message(const std::vector<unsigned char>& payload, int csid, uint32_t message_stream_id) {
    if (payload.size() < 3 || payload[0] != 0x02) return;

    uint16_t command_len = boost::endian::load_big_u16(&payload[1]);
    std::string command_name(payload.begin() + 3, payload.begin() + 3 + command_len);
    std::cout << "[INFO] Received command: " << command_name << std::endl;

    size_t offset = 3 + command_len;
    if (payload.size() < offset + 9 || payload[offset] != 0x00) return;
    
    uint64_t transaction_id_bits = boost::endian::load_big_u64(&payload[offset + 1]);
    double transaction_id = *reinterpret_cast<double*>(&transaction_id_bits);

    if (command_name == "connect") {
        send_window_ack_size(5000000, [this, csid, transaction_id]() {
            send_peer_bandwidth(5000000, 2, [this, csid, transaction_id]() {
                set_chunk_size([this, csid, transaction_id]() {
                    send_connect_response(csid, transaction_id);
                });
            });
        });
    } else if (command_name == "createStream") {
        send_create_stream_response(csid, transaction_id);
    } else if (command_name == "publish") {
        send_publish_response(csid, message_stream_id);
    }
}

void RtmpSession::send_connect_response(int csid, double transaction_id) {
    std::cout << "[INFO] Sending connect response." << std::endl;
    // Hardcoded response for _result('NetConnection.Connect.Success')
    const std::vector<unsigned char> response_payload = {
        0x02, 0x00, 0x07, '_', 'r', 'e', 's', 'u', 'l', 't', 
        0x00, 0x3f, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Transaction ID 1.0
        0x03, 
        0x00, 0x08, 'f', 'm', 's', 'V', 'e', 'r', 's', 'i', 'o', 'n', 0x02, 0x00, 0x09, 'F', 'M', 'S', '/', '4', ',', '5', ',', '0',
        0x00, 0x0c, 'c', 'a', 'p', 'a', 'b', 'i', 'l', 'i', 't', 'i', 'e', 's', 0x00, 0x40, 0x3f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x09, 
        0x03, 
        0x00, 0x05, 'l', 'e', 'v', 'e', 'l', 0x02, 0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
        0x00, 0x04, 'c', 'o', 'd', 'e', 0x02, 0x00, 0x1d, 'N', 'e', 't', 'C', 'o', 'n', 'n', 'e', 'c', 't', 'i', 'o', 'n', '.', 'C', 'o', 'n', 'n', 'e', 'c', 't', '.', 'S', 'u', 'c', 'c', 'e', 's', 's',
        0x00, 0x00, 0x09
    };
    auto chunk = create_chunk(csid, 0, 20, 0, response_payload);
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(*chunk), [self, chunk](const boost::system::error_code&, std::size_t){});
}

void RtmpSession::send_create_stream_response(int csid, double transaction_id) {
    std::cout << "[INFO] Sending createStream response." << std::endl;
    const std::vector<unsigned char> response_payload = {
        0x02, 0x00, 0x07, '_', 'r', 'e', 's', 'u', 'l', 't',
        0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Transaction ID 2.0
        0x05, // null command object
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00 // Stream ID 1
    };
    auto chunk = create_chunk(csid, 0, 20, 0, response_payload);
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(*chunk), [self, chunk](const boost::system::error_code&, std::size_t){});
}

void RtmpSession::send_publish_response(int csid, uint32_t message_stream_id) {
    std::cout << "[INFO] Sending publish response." << std::endl;
    send_user_control_message(0, 1); // Stream Begin on Stream ID 1
    
    const std::vector<unsigned char> response_payload = {
        0x02, 0x00, 0x08, 'o', 'n', 'S', 't', 'a', 't', 'u', 's',
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // Transaction ID 0.0
        0x05, // null command object
        0x03, 
            0x00, 0x04, 'c', 'o', 'd', 'e', 0x02, 0x00, 0x18, 'N', 'e', 't', 'S', 't', 'r', 'e', 'a', 'm', '.', 'P', 'u', 'b', 'l', 'i', 's', 'h', '.', 'S', 't', 'a', 'r', 't',
            0x00, 0x05, 'l', 'e', 'v', 'e', 'l', 0x02, 0x00, 0x06, 's', 't', 'a', 't', 'u', 's',
        0x00, 0x00, 0x09
    };
    auto chunk = create_chunk(csid, 0, 20, message_stream_id, response_payload);
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(*chunk), [self, chunk](const boost::system::error_code&, std::size_t){});
}

void RtmpSession::send_user_control_message(uint16_t event_type, uint32_t value) {
    std::cout << "[INFO] Sending User Control Message (Event: " << event_type << ")" << std::endl;
    std::vector<unsigned char> payload(6);
    boost::endian::store_big_u16(payload.data(), event_type);
    boost::endian::store_big_u32(payload.data() + 2, value);
    
    auto chunk = create_chunk(2, 0, 4, 0, payload);
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(*chunk), [self, chunk](const boost::system::error_code&, std::size_t){});
}

void RtmpSession::send_window_ack_size(uint32_t size, std::function<void()> next_step) {
    std::cout << "[INFO] Sending Window Ack Size: " << size << std::endl;
    std::vector<unsigned char> payload(4);
    boost::endian::store_big_u32(payload.data(), size);

    auto chunk = create_chunk(2, 0, 5, 0, payload);
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(*chunk), [self, chunk, next_step](const boost::system::error_code& ec, std::size_t) {
        if (!ec && next_step) {
            next_step();
        }
    });
}

void RtmpSession::send_peer_bandwidth(uint32_t size, uint8_t limit_type, std::function<void()> next_step) {
    std::cout << "[INFO] Sending Set Peer Bandwidth." << std::endl;
    std::vector<unsigned char> payload(5);
    boost::endian::store_big_u32(payload.data(), size);
    payload[4] = limit_type;

    auto chunk = create_chunk(2, 0, 6, 0, payload);
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(*chunk), [self, chunk, next_step](const boost::system::error_code& ec, std::size_t) {
        if (!ec && next_step) {
            next_step();
        }
    });
}


void RtmpSession::set_chunk_size(std::function<void()> next_step) {
    std::cout << "[INFO] Sending Set Chunk Size: " << outgoing_chunk_size_ << std::endl;
    std::vector<unsigned char> payload(4);
    boost::endian::store_big_u32(payload.data(), outgoing_chunk_size_);
    
    auto chunk = create_chunk(2, 0, 1, 0, payload);
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(*chunk), [self, chunk, next_step](const boost::system::error_code& ec, std::size_t) {
        if (!ec && next_step) {
            next_step();
        }
    });
}

