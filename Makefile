all:
	gcc -pthread -o server server.c
	gcc -pthread -o client1 client1.c
server:
	gcc -o server server.c
client1:
	gcc -pthread -o client1 client1.c
clear:
	rm server client1