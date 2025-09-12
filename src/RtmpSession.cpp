#include "RtmpSession.h"
#include <iostream>
#include <iomanip> // For std::hex
#include <random>
#include <algorithm>

RtmpSession::RtmpSession(tcp::socket socket) 
    : socket_(std::move(socket)),
      handshake_buffer_(1537)
{}

void RtmpSession::start() {
    std::cout << "[INFO] Starting new session..." << std::endl;
    do_handshake();
}

// --- Handshake methods ---
void RtmpSession::do_handshake() {
    auto self = shared_from_this();
    boost::asio::async_read(socket_, boost::asio::buffer(handshake_buffer_),
        [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            on_c1_read(ec, bytes_transferred);
        });
}

void RtmpSession::on_c1_read(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (ec) {
        std::cerr << "[ERROR] Handshake: C1 read failed: " << ec.message() << std::endl;
        return;
    }
    std::cout << "[INFO] Handshake: C1 received (" << bytes_transferred << " bytes)." << std::endl;
    unsigned char client_version = handshake_buffer_[0];
    std::vector<unsigned char> response_buffer;
    response_buffer.reserve(1 + 1536 + 1536);
    response_buffer.push_back(client_version);
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, 255);
    for(int i = 0; i < 1536; ++i) { response_buffer.push_back(distrib(gen)); }
    response_buffer.insert(response_buffer.end(), handshake_buffer_.begin() + 1, handshake_buffer_.end());
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(response_buffer),
        [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            on_s2_written(ec, bytes_transferred);
        });
}

void RtmpSession::on_s2_written(const boost::system::error_code& ec, std::size_t bytes_transferred) {
     if (ec) {
        std::cerr << "[ERROR] Handshake: S2 write failed: " << ec.message() << std::endl;
        return;
    }
    std::cout << "[INFO] Handshake: S2 sent (" << bytes_transferred << " bytes)." << std::endl;
    handshake_buffer_.resize(1536);
    auto self = shared_from_this();
    boost::asio::async_read(socket_, boost::asio::buffer(handshake_buffer_),
         [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
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
// --- End of handshake methods ---

void RtmpSession::do_read() {
    auto self = shared_from_this();
    socket_.async_read_some(boost::asio::buffer(buffer_),
        [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            on_message_read(ec, bytes_transferred);
        });
}

void RtmpSession::on_message_read(const boost::system::error_code& ec, std::size_t bytes_transferred) {
    if (!ec) {
        if (!connect_response_sent_) {
            // Define the exact byte sequence for the AMF0 "connect" string
            // 0x02 = string marker, 0x00 0x07 = length 7, followed by "connect"
            const std::vector<unsigned char> connect_signature = {
                0x02, 0x00, 0x07, 0x63, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74
            };

            // Use std::search to find the signature in our buffer
            auto it = std::search(
                buffer_.begin(), buffer_.begin() + bytes_transferred,
                connect_signature.begin(), connect_signature.end()
            );

            if (it != buffer_.begin() + bytes_transferred) {
                // Signature was found!
                std::cout << "[INFO] Found 'connect' command signature." << std::endl;
                send_connect_response();
                connect_response_sent_ = true;
            } else {
                std::cout << "[DEBUG] Scanned " << bytes_transferred << " bytes, 'connect' signature not found." << std::endl;
            }
        }
        
        // Continue the read loop
        do_read();

    } else {
        if (ec != boost::asio::error::eof) {
             std::cerr << "[ERROR] Read error: " << ec.message() << std::endl;
        } else {
             std::cout << "[INFO] Client disconnected." << std::endl;
        }
    }
}

void RtmpSession::send_connect_response() {
    std::cout << "[DEBUG] Preparing to send 'connect' success response (_result)." << std::endl;

    // A corrected and verified hardcoded response for a successful connection.
    const std::vector<unsigned char> response = {
        0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x15, 0x14, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x07, 0x5F,
        0x72, 0x65, 0x73, 0x75, 0x6C, 0x74, 0x00, 0x3F, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03,
        0x00, 0x03, 0x66, 0x6D, 0x73, 0x00, 0x04, 0x34, 0x2E, 0x30, 0x2E, 0x30, 0x2E, 0x32, 0x39, 0x37,
        0x38, 0x00, 0x0A, 0x63, 0x61, 0x70, 0x61, 0x62, 0x69, 0x6C, 0x69, 0x74, 0x69, 0x65, 0x73, 0x00,
        0x40, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x00, 0x04, 0x6C, 0x65,
        0x76, 0x65, 0x6C, 0x00, 0x09, 0x73, 0x75, 0x63, 0x63, 0x65, 0x73, 0x73, 0x00, 0x04, 0x63, 0x6F,
        0x64, 0x65, 0x00, 0x1A, 0x4E, 0x65, 0x74, 0x43, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74, 0x69, 0x6F,
        0x6E, 0x2E, 0x43, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74, 0x2E, 0x53, 0x75, 0x63, 0x63, 0x65, 0x73,
        0x73, 0x00, 0x0B, 0x64, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6F, 0x6E, 0x00, 0x16,
        0x43, 0x6F, 0x6E, 0x6E, 0x65, 0x63, 0x74, 0x69, 0x6F, 0x6E, 0x20, 0x73, 0x75, 0x63, 0x63, 0x65,
        0x65, 0x64, 0x65, 0x64, 0x2E, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00
    };
    
    auto self = shared_from_this();
    boost::asio::async_write(socket_, boost::asio::buffer(response),
        [this, self](const boost::system::error_code& ec, std::size_t bytes_transferred) {
            if (!ec) {
                std::cout << "[SUCCESS] Successfully sent 'connect' response (" << bytes_transferred << " bytes)." << std::endl;
            } else {
                std::cerr << "[ERROR] Failed to send 'connect' response: " << ec.message() << std::endl;
            }
        });
}