# Specify the base image as as Alpine Linux
FROM alpine:3.23.0 AS build
# Update and install necessary packages
RUN apk update && \
    apk add --no-cache \
    build-base:0.5-r3 \
    git:2.52.0-r0 \
    linux-headers:6.18 \
    uv:0.5.31-r0 \
    libev-dev:4.33-r1 \
    libnsl:2.0.1-r1 \
    openssl-dev:3.5.4-r0 \
    llvm:21-r0
# Set the working directory
WORKDIR /llm_mitmproxy
# Copy source files and configuration into the container
COPY src/ ./src/
COPY lib/ ./lib/
COPY crt/ ./crt/
COPY Makefile ./Makefile
COPY .clang-format ./.clang-format
COPY .clangd ./.clangd
# Creat user and group for running the application
RUN addgroup -S mitmp && adduser -S mitmp -G mitmp
USER mitmp
COPY --chown=mitmp:mitmp --from=build \
    ./llm_mitmproxy/build/src/proxy \
    ./app/
ENTRYPOINT [ "./app/proxy" ]
