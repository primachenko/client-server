all:
	protoc-c --c_out=. amsg.proto
	gcc -pthread -std=c99 -o server server.c amsg.pb-c.c -lprotobuf-c
	gcc -pthread -std=c99 -o client1 client1.c amsg.pb-c.c -lprotobuf-c
	gcc -pthread -std=c99 -o client2 client2.c amsg.pb-c.c -lprotobuf-c
clear:
	rm server client1 client2