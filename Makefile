C=clang
CCFLAGS=-Wall -Wextra -std=c99 -pedantic -O3 -g
LDFLAGS=-g -lm
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

# Test targets
perfttest: all
	./$(TARGET) <<< "perfttest"

searchtest: all
	./$(TARGET) <<< "searchtest"

sts: all
	./$(TARGET) <<< "sts 1"

sts-fast: all
	./$(TARGET) <<< "sts 0.1"

test: all
	@echo "Running perft test suite..."
	echo "perfttest" | ./$(TARGET)
	@echo ""
	@echo "Running search test suite..."
	echo "searchtest" | ./$(TARGET)

.PHONY: all clean perfttest searchtest sts sts-fast test
