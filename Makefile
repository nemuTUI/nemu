CC=g++
CFLAGS=-c -g -Wall
EXECUTABLE=qemu_manage
LDFLAGS=-lboost_regex
SOURCES=qemu_manage.cpp
OBJECTS=$(SOURCES:.cpp=.o) 

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@

clean:
	rm *.o $(TARGET)
