#!/usr/bin/env bash

docker run \
	--name=broadcasterproxy \
	-v ${PWD}:/storage \
	--init \
	-it \
	--rm \
	cpp_streamer:latest /bin/sh