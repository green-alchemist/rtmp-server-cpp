#include "RtmpServer.h"
#include <iostream>

int main() {
    try {
        // The io_context is the core of Asio's I/O functionality
        boost::asio::io_context io_context;

        // Create and run our RTMP server on the default port 1935
        RtmpServer server(io_context, 1935);

        std::cout << "🚀 C++ RTMP Server listening on port 1935" << std::endl;

        // This call blocks and runs the I/O service
        io_context.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
    return 0;
}