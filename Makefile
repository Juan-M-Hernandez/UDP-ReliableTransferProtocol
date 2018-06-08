CC=gcc
CPPFLAGS=-g -Wall
USERID=123456789
CLASSES=

default:
	$(CC) -o server $(CPPFLAGS) server.c

sample:
	g++ -o server-sample $(CPPFLAGS) server-sample.cpp
	g++ -o client-sample $(CPPFLAGS) client-sample.cpp

clean:
	rm -rf *.o *~ *.gch *.swp *.dSYM server client *.tar.gz

dist: tarball

tarball: clean
	tar -cvzf 904476103.tar.gz server.c Makefile report.pdf README
