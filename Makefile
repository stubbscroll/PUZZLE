CC=gcc
CFLAGS=-Wall -O2
LIBS = $(shell sdl-config --libs)

puzzle: puzzle.o dir.o menu.o graphic.o akari.o nurikabe.o heyawake.o hitori.o picross.o slither.o masyu.o hashi.o yajilin.o mine.o kuromasu.o sdlfont.o
	$(CC) puzzle.o dir.o menu.o graphic.o akari.o nurikabe.o heyawake.o hitori.o picross.o slither.o masyu.o hashi.o yajilin.o mine.o kuromasu.o sdlfont.o -o puzzle $(LIBS)

puzzle.o: puzzle.c
	$(CC) $(CFLAGS) -c puzzle.c

dir.o: dir.c
	$(CC) $(CFLAGS) -c dir.c

menu.o: menu.c
	$(CC) $(CFLAGS) -c menu.c

graphic.o: graphic.c
	$(CC) $(CFLAGS) -c graphic.c

akari.o: akari.c
	$(CC) $(CFLAGS) -c akari.c

heyawake.o: heyawake.c
	$(CC) $(CFLAGS) -c heyawake.c

hitori.o: hitori.c
	$(CC) $(CFLAGS) -c hitori.c

nurikabe.o: nurikabe.c
	$(CC) $(CFLAGS) -c nurikabe.c

picross.o: picross.c
	$(CC) $(CFLAGS) -c picross.c

slither.o: slither.c
	$(CC) $(CFLAGS) -c slither.c

masyu.o: masyu.c
	$(CC) $(CFLAGS) -c masyu.c

hashi.o: hashi.c
	$(CC) $(CFLAGS) -c hashi.c

yajilin.o: yajilin.c
	$(CC) $(CFLAGS) -c yajilin.c

mine.o: mine.c
	$(CC) $(CFLAGS) -c mine.c

kuromasu.o: kuromasu.c
	$(CC) $(CFLAGS) -c kuromasu.c

sdlfont.o: sdlfont.c
	$(CC) $(CFLAGS) -c sdlfont.c
