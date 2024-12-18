.PHONY: build-server
build-server:
	gcc server.c -o server

.PHONY: build-client
build-client:
	gcc client.c -o client
