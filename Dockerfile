FROM registry.cn-hangzhou.aliyuncs.com/accer/ubuntu-gcc-cmake-docker:latest AS builder

LABEL Description="Build environment"

WORKDIR /build

ADD 3rdparty 3rdparty
ADD src src
ADD CMakeLists.txt CMakeLists.txt

RUN cmake . && cmake --build . --target mediasoup_pull_bench

FROM ubuntu:20.04
RUN apt-get update && apt-get install -y tzdata

ENV TZ=Asia/Shanghai
ENV LD_LIBRARY_PATH=/app/output/lib

WORKDIR /app
COPY --from=builder /build/mediasoup_pull_bench mediasoup_pull_bench
COPY --from=builder /build/output output

CMD ["./mediasoup_pull_bench"]