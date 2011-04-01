CC=g++
CFLAGS=-Wall -DXP_UNIX=1 -DMOZ_X11=1 -fPIC -O2
LDFLAGS=-shared -lwebp
SOURCES=webp-npapi.cpp CPlugin.cpp
OBJECTS=$(SOURCES:.cpp=.o)
LIBRARY=webp-npapi.so

all: $(SOURCES) $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@

.cpp.o:
	$(CC) `pkg-config --cflags gtk+-2.0` $(CFLAGS) -c $<

clean:
	rm -rf *.o *.so
