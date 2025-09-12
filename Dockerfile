# Stage 1: The Build Environment
# Use a specific Debian version for a predictable build environment
FROM debian:bookworm-slim AS builder

# Set the working directory inside the container
WORKDIR /app

# Install dependencies: build tools and the specific Boost library we need
RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    make \
    libboost-system-dev

# Copy the project source code into the container
COPY . .

# Create a build directory and run CMake and make
RUN cmake -S . -B build
RUN cmake --build build

# Stage 2: The Final Production Image
# We use a minimal base image to keep the size down
FROM debian:bookworm-slim

# Set the working directory
WORKDIR /app

# Copy ONLY the compiled executable from the builder stage
COPY --from=builder /app/build/rtmp_server .

# Expose the RTMP port
EXPOSE 1935

# The command to run when the container starts
CMD ["./rtmp_server"]