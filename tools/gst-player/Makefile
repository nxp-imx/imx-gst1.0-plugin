EXTRA_WARNINGS=-Wall -W -Wformat-nonliteral -Wcast-align -Wpointer-arith \
	-Wbad-function-cast -Wmissing-prototypes -Wstrict-prototypes \
	-Wmissing-declarations -Winline -Wundef -Wnested-externs -Wcast-qual \
	-Wshadow -Wwrite-strings -Wno-unused-parameter -Wfloat-equal -pedantic -ansi -std=c99

CFLAGS=-ggdb $(EXTRA_WARNINGS) `pkg-config --cflags gtk+-2.0 gstreamer-1.0`
LIBS=`pkg-config --libs gtk+-2.0 gstreamer-1.0`

all: player

player: 
	gcc -o gst-player ui.c gst-backend.c $(LIBS) $(CFLAGS) -l gstvideo-1.0

clean:
	rm *.o gst-player

