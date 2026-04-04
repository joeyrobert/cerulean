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

datagen: all
	./$(TARGET) --datagen --games $(or $(GAMES),1000) --output $(or $(OUTPUT),data/gen.bin)

train: all
	./$(TARGET) --train --data $(or $(DATA),data/gen.bin) --net $(or $(NET),nets/default.nnue)

sts-eval: all
	./$(TARGET) --sts-eval --net $(or $(NET),nets/default.nnue)

.PHONY: all clean perfttest searchtest sts sts-fast test datagen train sts-eval
