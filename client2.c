#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <strings.h>

#define MYPORT 17000
#define SERVPORT 33000
#define ALLOW_RECIEV "RCV"
#define DENY_RECIEV "RDN"
#define ALDENLEN 4
#define PORTRANGE 10
#define MAXMSGLEN 40

struct {
    pthread_mutex_t mutex;
    int allow;
} tcp_recv = { 
    PTHREAD_MUTEX_INITIALIZER
};

void *udp_listener(void*);

void *tcp_reciever(void*);

char *packet_generator();

void error(const char *msg)
	 {
	    perror(msg);
	    tcp_recv.allow = 1;
	    pthread_exit(0);
	 }

int main(int argc, char *argv[])
	{
		printf("CLIENT 2 type: start\n");
		pthread_t udp_listener_thread, tcp_reciever_thread;
		pthread_create(&udp_listener_thread, NULL, udp_listener, NULL);
		pthread_join(udp_listener_thread, NULL);
		return 0;
	}

void *udp_listener(void *arg)
	{
		int bytes_recv, mysocket,client_addr_size;
		struct sockaddr_in sockaddr_in;
		struct sockaddr_in client_addr;
		struct sockaddr_in local_addr;
		struct hostent *hst;
		char buf[ALDENLEN];
		char *addr;
		socklen_t fromlen;
		pthread_t thread;

		char *all = ALLOW_RECIEV;
		char *den = DENY_RECIEV;

		if ((mysocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		{
			error("socket() failed");
		}

		local_addr.sin_family = AF_INET;
		local_addr.sin_port = htons(MYPORT);
		local_addr.sin_addr.s_addr = INADDR_ANY;

		if (bind(mysocket, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
		{
		printf("CLIENT 2 type: search for a free port\n");
		int i = 0;
		while(i < PORTRANGE)
		{
			i++;
			local_addr.sin_port = htons(MYPORT + i);
			if (!bind(mysocket, (struct sockaddr *)&local_addr, sizeof(local_addr)))
				break;
		}
		}

		client_addr_size = sizeof(struct sockaddr_in);

		printf("CLIENT 2 type: listening broadcast\n");

		tcp_recv.allow = 0;

		while (1) 
		{
			if ((bytes_recv = recvfrom(mysocket, &buf[0], sizeof(buf) - 1, 0, (struct sockaddr *)&client_addr, &client_addr_size)) < 0)
				error("recvfrom() failed");


			hst = gethostbyaddr((char *)&client_addr.sin_addr, 4, AF_INET);
			addr = (char*)inet_ntoa(client_addr.sin_addr);

			buf[bytes_recv] = 0;

			if(!strcmp(buf, all)){
			  if(tcp_recv.allow == 0){
			    pthread_mutex_lock(&tcp_recv.mutex);
			    tcp_recv.allow = 1;
			    pthread_mutex_unlock(&tcp_recv.mutex);
			    pthread_create(&thread, NULL, tcp_reciever, (void*)addr);
			  }
			 }
			 if (!strcmp(buf, den)){ 
			  	pthread_mutex_lock(&tcp_recv.mutex);
			    tcp_recv.allow = 0;
			    pthread_mutex_unlock(&tcp_recv.mutex);
			    pthread_cancel(thread);
			} 
		}
	}

void *tcp_reciever(void *arg)
	{
		int my_sock;
		int sleep_time, len;
		char buff[MAXMSGLEN];
		struct sockaddr_in serv_addr;
		int bytes_recv;
		struct hostent *server;
		char *addr;

		addr = (char*)arg;

		printf("CLIENT 2 type: start.. connecting to %s:%d\n", addr, SERVPORT);

		my_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (my_sock < 0)
			error("socket() failed");
		server = gethostbyname(addr);
		if (server == NULL) 
		{
			fprintf(stderr,"gethostbyname() failed\n");
			exit(0);
		}

		bzero((char *) &serv_addr, sizeof(serv_addr));

		serv_addr.sin_family = AF_INET;

		bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr, server->h_length);
		serv_addr.sin_port = htons(SERVPORT);

		if (connect(my_sock,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
		    error("connect() failed");
		printf("CLIENT 2 type: connected\n");

		while (tcp_recv.allow == 1)
		{
			fflush(stdout);
			bzero(&buff, MAXMSGLEN);
			bytes_recv = recv(my_sock,(char*)&buff[0], MAXMSGLEN, 0);
			if (bytes_recv <= 0) 
				error("recv() failed");
		
			buff[bytes_recv+1] = '\0';
			len = ((int*)buff)[1];
			sleep_time = ((int*)buff)[0];
			printf("CLIENT 2 type: recieved %d bytes: ",len);
			for(int i = 8; i < len-1-1; i++)
			printf("%c", buff[i]);
			printf("\nCLIENT 2 type: sleep %d sec\n", sleep_time);
			sleep(sleep_time);
		}
		printf("CLIENT 2 type: queue is empty\n");
		close(my_sock);
		pthread_exit(0);
	}