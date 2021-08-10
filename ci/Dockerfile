FROM debian:buster-slim

RUN apt-get update
RUN apt-get install -y apt-utils
RUN apt-get install -y git cmake gcc cppcheck python3 python3-pip
RUN apt-get install -y libdbus-1-dev libncurses-dev libsqlite3-dev libusb-1.0-0-dev
RUN apt-get install -y libarchive-dev libgraphviz-dev libudev-dev libjson-c-dev libxml2-dev
RUN apt-get install -y gettext
RUN pip3 install codespell

RUN mkdir -p /build
WORKDIR /build
