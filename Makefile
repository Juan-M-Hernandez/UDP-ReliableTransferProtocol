CC=gcc
CPPFLAGS=-g -Wall
USERID=123456789
CLASSES=

default:
	$(CC) -o server $(CPPFLAGS) server.c

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

dist: tarball

tarball: clean
	tar -cvzf 904476103.tar.gz server.c Makefile report.pdf README
