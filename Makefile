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
	rm -f $(OBJECTS) $(EXECUTABLE) qemu-manager.mo

install:
	msgfmt lang/ru/qemu-manager.po -o qemu-manager.mo
	install -m 0644 qemu-manager.mo /usr/share/locale/ru/LC_MESSAGES/
	test -f /etc/qemu_manage.cfg || install -m 0644 qemu_manage.cfg.sample /etc/qemu_manage.cfg
	strip $(EXECUTABLE) && install -m 0755 $(EXECUTABLE) /usr/bin/

uninstall:
	rm -f /usr/share/locale/ru/LC_MESSAGES/qemu-manager.mo /usr/bin/$(EXECUTABLE)
