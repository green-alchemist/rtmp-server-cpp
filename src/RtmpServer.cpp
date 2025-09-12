#include "RtmpServer.h"
#include "RtmpSession.h"
#include <iostream>

RtmpServer::RtmpServer(boost::asio::io_context& io_context, short port)
    : acceptor_(io_context, tcp::endpoint(tcp::v4(), port)) {
    do_accept();
}

void RtmpServer::do_accept() {
    acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::cout << "Accepted new connection." << std::endl;
                // Create a session to handle the new client
                std::make_shared<RtmpSession>(std::move(socket))->start();
            }
            // Continue listening for the next connection
            do_accept();
        });
}