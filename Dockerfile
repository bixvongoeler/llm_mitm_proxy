# =============================================================================
# STAGE 1: Build Stage - compiles the proxy binary
# =============================================================================
FROM alpine:3.21.3 AS build

# Update and install necessary packages for compilation
# Note: Alpine uses '=' for version pinning, not ':'
RUN apk update && \
    apk add --no-cache \
    build-base=0.5-r3 \
    git=2.47.3-r0 \
    linux-headers=6.6-r1 \
    libev-dev=4.33-r1 \
    libnsl-dev=2.0.1-r0 \
    openssl-dev=3.3.5-r0

# Set the working directory
WORKDIR /llm_mitmproxy

# Copy source files and configuration into the container
COPY src/ ./src/
COPY lib/ ./lib/
COPY crt/ ./crt/
COPY Makefile ./Makefile
COPY .clang-format ./.clang-format
COPY .clangd ./.clangd

# Build the proxy binary
RUN make proxy

# =============================================================================
# STAGE 2: Runtime Stage - minimal image to run the proxy
# =============================================================================
FROM alpine:3.21.3 AS runtime

# Install only runtime dependencies (smaller image, no build tools!)
RUN apk add --no-cache \
    libev=4.33-r1 \
    libnsl=2.0.1-r0 \
    openssl=3.3.5-r0

# Create user and group for running the application (security best practice)
RUN addgroup -S mitmp && adduser -S mitmp -G mitmp

# Set working directory
WORKDIR /app

# Copy the compiled binary FROM the build stage (this is the key fix!)
COPY --from=build /llm_mitmproxy/proxy ./proxy

# Copy certificates needed at runtime
COPY --from=build /llm_mitmproxy/crt/ ./crt/

# Change ownership to the non-root user
RUN chown -R mitmp:mitmp /app

# Switch to non-root user
USER mitmp

ENTRYPOINT ["./proxy", "9999", "crt/proxy_ca.crt", "crt/proxy_ca.key"]
