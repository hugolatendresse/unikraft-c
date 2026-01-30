FROM gcc:13-bookworm AS build

WORKDIR /src
COPY src/ /src/

# Build statically linked C binary
RUN gcc -std=c11 -O2 -static-pie -fPIE \
    -o /blockio-test \
    main.c benchmark.c file_io.c block_io.c \
    -lm

# Create directory structure
FROM alpine:latest AS dirs
RUN mkdir -p /rootfs/dev /rootfs/data /rootfs/tmp

# Final image
FROM scratch
COPY --from=build /blockio-test /blockio-test
COPY --from=dirs /rootfs/dev /dev
COPY --from=dirs /rootfs/data /data
COPY --from=dirs /rootfs/tmp /tmp
