.PHONY: build-server
build-server:
	gcc -Wall -Wextra -Wpedantic server.c -o server

.PHONY: run-server
run-server: build-server
	./server

.PHONY: build-client
build-client:
	gcc -Wall -Wextra -Wpedantic client.c -o client

.PHONY: run-client
run-client:build-client
	./client
