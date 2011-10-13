C=clang
CCFLAGS=-Wall -Wextra -std=c99 -pedantic -O3 -g 
LDFLAGS=-g
SOURCES=$(wildcard *.c)
OBJECTS=$(SOURCES:.c=.o)
TARGET=cerulean

all: $(OBJECTS)
	$(C) $(LDFLAGS) -o $(TARGET) $^

%.o: %.c %.h
	$(C) $(CCFLAGS) -c $<

%.o: %.c
	$(C) $(CCFLAGS) -c $<

clean:
	rm -f *.o $(TARGET)
