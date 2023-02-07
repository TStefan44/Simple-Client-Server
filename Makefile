CFLAGS = -Wall -g

all: server subscriber

# Compileaza server.cpp
server: server.cpp

# Compileaza subscriber.cpp
subscriber: subscriber.cpp

.PHONY: clean run_server run_subscriber

# Ruleaza serverul
run_server:
	./server

# Ruleaza subscriberul
run_subscriber:
	./subscriber

clean:
	rm -f server subscriber