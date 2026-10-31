CC     = gcc
CFLAGS = -Wall -Wextra -O2 $(shell pkg-config --cflags gtk+-3.0)
LIBS   = $(shell pkg-config --libs gtk+-3.0)

atm_gtk: atm_gtk.c
	$(CC) $(CFLAGS) atm_gtk.c -o atm_gtk $(LIBS)

run: atm_gtk
	./atm_gtk

clean:
	rm -f atm_gtk atm_gtk.exe
