CXX=g++
CXXFLAGS=-c -g -Wall
EXECUTABLE=qemu_manage
LDFLAGS=-lboost_regex
SOURCES=qemu_manage.cpp
OBJECTS=$(SOURCES:.cpp=.o) 

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(OBJECTS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@

clean:
	rm -f $(OBJECTS) $(EXECUTABLE)
