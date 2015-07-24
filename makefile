gateway: gateway.o urlencode.o base64.o ssdv.o ssdv.h kml.o global.h
	cc -o gateway gateway.o urlencode.o base64.o ssdv.o kml.o -lm -lwiringPi -lwiringPiDev -lcurl -lncurses -lpthread

gateway.o: gateway.c global.h
	gcc -c gateway.c

ssdv.o: ssdv.c ssdv.h global.h
	gcc -c ssdv.c
	
urlencode.o: urlencode.c
	gcc -c urlencode.c

base64.o: base64.c
	gcc -c base64.c

kml.o: kml.c kml.h global.h
	gcc -c kml.c

