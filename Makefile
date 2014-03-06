CC=g++
CFLAGS=-c -g -O2
EXECUTABLE=qemu_manage
LDFLAGS=
SOURCES=qemu_manage.cpp
OBJECTS=$(SOURCES:.cpp=.o) 

all: $(SOURCES) $(EXECUTABLE)

$(EXECUTABLE): $(OBJECTS)
	$(CC) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) $< -o $@


#qemu_manage: qemu_manage.o
#	$(CC) -o $(TARGET) $(CFLAGS) qemu_manage.o

clean:
	rm *.o $(TARGET)
