# RJH Generic makefile

SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o) # replaces the .c from SRC with .o
EXE=gateway

CC=gcc
CFLAGS=-Wall -O3 
LDFLAGS= -lm -lwiringPi -lwiringPiDev -lcurl -lncurses -lpthread
RM=rm

%.o: %.c         # combined w/ next line will compile recently changed .c files
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY : all     # .PHONY ignores files named all
all: $(EXE)      # all is dependent on $(EXE) to be complete

$(EXE): $(OBJ)   # $(EXE) is dependent on all of the files in $(OBJ) to exist
	$(CC) $(OBJ) $(LDFLAGS) -o $@

.PHONY : clean   # .PHONY ignores files named clean
clean:
	-$(RM) $(OBJ) 
