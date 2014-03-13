CXX=g++
CXXFLAGS=-c -g -Wall -std=c++11 -DENABLE_OPENVSWITCH
EXECUTABLE=qemu_manage
LDFLAGS=-lboost_regex -lncurses -lsqlite3
SOURCES=qemu_manage.cpp qemu_hostinfo.cpp windows.cpp \
	qemudb.cpp
OBJECTS=$(SOURCES:.cpp=.o) 

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
