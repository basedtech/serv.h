LDFLAGS=-lX11

window:
	$(CC) window.c $(LDFLAGS) -o window

clean:
	$(RM) window *.o
