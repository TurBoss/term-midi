CC = gcc
CLAGS = -Wall -Wextra
LIBS = -lncurses -lcsound64 -lportmidi

sequencer: midi_sequencer.c
	$(CC) $(CFLAGS) -o midi_sequencer midi_sequencer.c $(LIBS)
