all:
	gcc -pthread -std=gnu99 -o server server.c
	gcc -pthread -std=gnu99 -o client1 client1.c
	gcc -pthread -std=gnu99 -o client2 client2.c
server:
	gcc -pthread -std=c99 -o server server.c
client1:
	gcc -pthread -std=c99 -o client1 client1.c
clear:
	rm server client1 client2