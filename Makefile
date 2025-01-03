.PHONY: build-server
build-server:
	gcc server.c -o server

.PHONY: run-server
run-server: build-server
	./server

.PHONY: build-client
build-client:
	gcc client.c -o client

.PHONY: run-client
run-client:build-client
	./client
