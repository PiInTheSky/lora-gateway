
CC=gcc
CFLAGS=-Wall -g -O2
LDFLAGS=-lm -lwiringPi -lwiringPiDev -lcurl -lncurses -lpthread

gateway: gateway.o urlencode.o base64.o habitat.o ssdv.o ftp.o network.o server.o
	$(CC) $(LDFLAGS) -o gateway gateway.o urlencode.o base64.o habitat.o ssdv.o ftp.o network.o server.o

gateway.o: gateway.c global.h
	$(CC) $(CFLAGS) -o gateway.o -c gateway.c

habitat.o: habitat.c habitat.h global.h
	$(CC) $(CFLAGS) -o habitat.o -c habitat.c
	
ssdv.o: ssdv.c ssdv.h global.h
	$(CC) $(CFLAGS) -o ssdv.o -c ssdv.c
	
ftp.o: ftp.c ftp.h global.h
	$(CC) $(CFLAGS) -o ftp.o -c ftp.c
	
server.o: server.c server.h global.h
	$(CC) $(CFLAGS) -o server.o -c server.c
	
network.o: network.c network.h global.h
	$(CC) $(CFLAGS) -o network.o -c network.c
	
urlencode.o: urlencode.c
	$(CC) $(CFLAGS) -o urlencode.o -c urlencode.c

base64.o: base64.c
	$(CC) $(CFLAGS) -o base64.o -c base64.c

