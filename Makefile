C=gcc
CCFLAGS=-Wall -Wextra -std=c99 -pedantic -O3 -g 
LDFLAGS=-g
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
TARGET=cerulean

all: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $(TARGET) $^

%.o: %.c %.h
	$(CC) $(CCFLAGS) -c $<

%.o: %.c
	$(CC) $(CCFLAGS) -c $<

clean:
	rm -f *.o $(TARGET)
