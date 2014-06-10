EXTRA_WARNINGS=-Wall -W -Wformat-nonliteral -Wcast-align -Wpointer-arith \
	-Wbad-function-cast -Wmissing-prototypes -Wstrict-prototypes \
	-Wmissing-declarations -Winline -Wundef -Wnested-externs -Wcast-qual \
	-Wshadow -Wwrite-strings -Wno-unused-parameter -Wfloat-equal -pedantic -ansi -std=c99

CFLAGS=-ggdb $(EXTRA_WARNINGS) `pkg-config --cflags gtk+-2.0 gstreamer-1.0`
LIBS=`pkg-config --libs gtk+-2.0 gstreamer-1.0`

LIBS += -l gstvideo-1.0

SRC = imxplayer.c playengine.c controlbar.c infobox.c menubar.c \
      playlistbox.c subtitle.c volumebar.c seektodialog.c speedbox.c
      
all: player

player: 
	gcc -o imxplayer $(SRC) $(CFLAGS) $(LIBS) 

clean:
	rm imxplayer

