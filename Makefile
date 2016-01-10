IDIR =../include
CC=gcc
CFLAGS=-I$(IDIR) -Wall

ODIR=./
LDIR =../lib

LIBS=-lm -lpthread -lusb-1.0 -lwiringPi -lmpdclient -lasound

_DEPS = led.h manager.h btn.h pcm.h mcp3008reader.h
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = led.o manager.o btn.o pcm.o mcp3008reader.o main.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

zmp3: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) $(LIBS)

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~ 

push:
	# git remote add origin https://github.com/zaruba/zmp3.git
	# git commit --allow-empty-message
	git push -u origin master
