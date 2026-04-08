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

clean-nnue:
	rm -f data/*.bin
	rm -f data/*.bin.progress
	rm -f data/*.seed.bin
	rm -f data/*.shard*.bin
	rm -f data/*.shard*.bin.progress
	rm -f nets/*.nnue
	rm -f nets/*.checkpoint.*
	rm -f nets/*.sts-results.txt

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
	./$(TARGET) --datagen --games $(or $(GAMES),1000) --output $(or $(OUTPUT),data/gen.bin) --workers $(or $(WORKERS),0)

train: all
	./$(TARGET) --train --data $(or $(DATA),data/gen.bin) --net $(or $(NET),nets/default.nnue)

sts-eval: all
	./$(TARGET) --sts-eval --net $(or $(NET),nets/default.nnue)

nnue-cycle-fast: all
	./$(TARGET) --datagen --games $(or $(GAMES),1000) --output $(or $(DATA),data/fast.bin) --workers $(or $(WORKERS),0)
	./$(TARGET) --train --data $(or $(DATA),data/fast.bin) --net $(or $(NET),nets/fast.nnue)
	./$(TARGET) --sts-eval --net $(or $(NET),nets/fast.nnue)

nnue-cycle-full: all
	./$(TARGET) --datagen --games $(or $(GAMES),10000) --output $(or $(DATA),data/gen1.bin) --workers $(or $(WORKERS),0)
	./$(TARGET) --train --data $(or $(DATA),data/gen1.bin) --net $(or $(NET),nets/gen1.nnue)
	./$(TARGET) --sts-eval --net $(or $(NET),nets/gen1.nnue)

nnue-cycle-iter: all
	./$(TARGET) --nnue-loop --start-net $(or $(START_NET),nets/gen1.nnue) --start-round $(or $(START_ROUND),1) --rounds $(or $(ROUNDS),3) --games $(or $(GAMES),20000) --workers $(or $(WORKERS),0) --data-prefix $(or $(DATA_PREFIX),data/gen) --net-prefix $(or $(NET_PREFIX),nets/gen)

nnue-cycle-forever: all
	./$(TARGET) --nnue-forever --start-net $(or $(START_NET),nets/gen1.nnue) --start-round $(or $(START_ROUND),1) --games $(or $(GAMES),20000) --workers $(or $(WORKERS),0) --min-delta $(or $(MIN_DELTA),5) --patience $(or $(PATIENCE),2) --data-prefix $(or $(DATA_PREFIX),data/gen) --net-prefix $(or $(NET_PREFIX),nets/gen)

nnue-cycle: nnue-cycle-full

.PHONY: all clean clean-nnue perfttest searchtest sts sts-fast test datagen train sts-eval nnue-cycle nnue-cycle-fast nnue-cycle-full nnue-cycle-iter nnue-cycle-forever
