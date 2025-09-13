# C++ RTMP Server

This project is a from-scratch implementation of an RTMP (Real-Time Messaging Protocol) server written in C++ using the Boost.Asio library for asynchronous networking. The application is containerized with Docker and managed with Docker Compose for a clean, reproducible build and runtime environment.

The primary goal of this server is to act as a simple, high-performance ingest point for RTMP streams, with the eventual aim of supporting restreaming to other services.

## Prerequisites

To build and run this project, you will need the following software installed:

  * **Docker:** [Get Docker](https://docs.docker.com/get-docker/)
  * **Docker Compose:** (Included with Docker Desktop)

No C++ compiler, CMake, or Boost libraries are required on your host machine; the entire build environment is self-contained within the Docker image.

## How to Build and Run

1.  **Clone the Repository:**

    ```bash
    git clone <your-repo-url>
    cd rtmp-server-cpp
    ```

2.  **Build and Run with Docker Compose:**
    From the root of the project directory, run the following command. This will build the Docker image and start the server.

    ```bash
    docker compose up --build
    ```

    The server will be running in the foreground and listening on port `1935`.

3.  **Stopping the Server:**
    Press `Ctrl+C` in the terminal where the server is running. To remove the container, run:

    ```bash
    docker compose down
    ```

## How to Test

You can test the server using any RTMP-capable broadcasting software. The following instructions are for **OBS Studio**.

1.  **Open OBS Studio.**
2.  Go to **File \> Settings \> Stream**.
3.  Set the **Service** to `Custom...`.
4.  For the **Server**, enter: `rtmp://localhost:1935/live`
5.  For the **Stream Key**, you can use any value (e.g., `test`).
6.  Click **OK** to save the settings.
7.  Set up a scene in OBS (e.g., using an image or display capture).
8.  Click **Start Streaming**.

If the connection is successful, OBS will show a green connection indicator, and you will see detailed log output in your server's terminal window.

## Current Status

The server currently implements the following parts of the RTMP protocol:

  * ✅ Full RTMP Handshake (C0/S0, C1/S1, C2/S2)
  * ✅ Basic Chunk Stream Parsing (Formats 0, 1, 2, and 3)
  * ✅ Handling of initial RTMP commands:
      * `Set Chunk Size`
      * `Window Acknowledgement Size`
      * `Set Peer Bandwidth`
      * `connect`
      * `createStream`
      * `publish`
  * ✅ Sending correct, sequenced responses to establish a stable connection with clients like OBS.

At this stage, the server can successfully ingest a stream but does not yet process or forward the audio/video data.