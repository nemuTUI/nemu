CXX=g++
CXXFLAGS=-c -g -Wall -std=c++11 -pthread -DENABLE_OPENVSWITCH
EXECUTABLE=qemu_manage
LDFLAGS=-lboost_regex -lform -lncursesw -lsqlite3 -lprocps
SOURCES=qemu_manage.cpp hostinfo.cpp windows.cpp \
	qemudb.cpp stuff.cpp guest.cpp
OBJECTS=$(SOURCES:.cpp=.o) 

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
