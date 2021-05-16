all: server subscriber 

server:
	gcc -Wall -Wextra -g server.c protocol.c -lm -o server

subscriber:
	gcc -Wall -Wextra -g subscriber.c protocol.c -lm -o subscriber

clean:
	rm -f subscriber server
