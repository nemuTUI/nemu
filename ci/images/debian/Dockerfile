FROM debian:buster-slim

RUN apt-get update -y && \
    apt-get install -y apt-utils \
        git cmake ninja-build gcc cppcheck python3 python3-pip \
        libdbus-1-dev libncurses-dev libsqlite3-dev libusb-1.0-0-dev \
        libarchive-dev libgraphviz-dev libudev-dev libjson-c-dev libxml2-dev \
        libssl-dev gettext

RUN cd / && \
    curl -L https://github.com/github/codeql-action/releases/download/codeql-bundle-20210809/codeql-bundle-linux64.tar.gz > codeql.tar.gz && \
    tar -xzvf codeql.tar.gz && rm -rf codeql.tar.gz && \
    rm -rf /codeql/qlpacks/codeql-java/ && \
    rm -rf /codeql/qlpacks/codeql-ruby/ && \
    rm -rf /codeql/qlpacks/codeql-cpp-examples/ && \
    rm -rf /codeql/qlpacks/codeql-java-examples/ && \
    rm -rf /codeql/qlpacks/codeql-ruby-examples/ && \
    rm -rf /codeql/qlpacks/codeql-cpp-upgrades/ && \
    rm -rf /codeql/qlpacks/codeql-java-upgrades/ && \
    rm -rf /codeql/qlpacks/codeql-csharp/ && \
    rm -rf /codeql/qlpacks/codeql-javascript/ && \
    rm -rf /codeql/qlpacks/legacy-libraries-cpp/ && \
    rm -rf /codeql/qlpacks/codeql-csharp-examples/ && \
    rm -rf /codeql/qlpacks/codeql-javascript-examples/ && \
    rm -rf /codeql/qlpacks/legacy-libraries-csharp/ && \
    rm -rf /codeql/qlpacks/codeql-csharp-upgrades/ && \
    rm -rf /codeql/qlpacks/codeql-javascript-upgrades/ && \
    rm -rf /codeql/qlpacks/legacy-libraries-go/ && \
    rm -rf /codeql/qlpacks/codeql-go/ && \
    rm -rf /codeql/qlpacks/codeql-python/ && \
    rm -rf /codeql/qlpacks/legacy-libraries-java/ && \
    rm -rf /codeql/qlpacks/codeql-go-examples/ && \
    rm -rf /codeql/qlpacks/codeql-python-examples/ && \
    rm -rf /codeql/qlpacks/legacy-libraries-javascript/ && \
    rm -rf /codeql/qlpacks/codeql-go-upgrades/ && \
    rm -rf /codeql/qlpacks/codeql-python-upgrades/ && \
    rm -rf /codeql/qlpacks/legacy-libraries-python/ && \
    rm -rf /codeql/csv && \
    rm -rf /codeql/html && \
    rm -rf /codeql/legacy-upgrades && \
    rm -rf /codeql/experimental && \
    rm -rf /codeql/java && \
    rm -rf /codeql/csharp && \
    rm -rf /codeql/go && \
    rm -rf /codeql/javascript && \
    rm -rf /codeql/python && \
    rm -rf /codeql/xml

RUN pip3 install codespell
