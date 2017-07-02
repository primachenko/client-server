#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>


#define ALLOW_SEND "ALL"
#define DENY_SEND "DEN"
#define ALDENLEN 4  
#define SLEEPTIME 1
#define BRDIP "127.0.0.255"
#define SERVPORT 16000
#define MYPORT 32000
#define PORTRANGE 10

#define MAXMSGLEN 50 //максимальная длина 

struct {
    pthread_mutex_t mutex;
    int allow;
} tcp_send = { 
    PTHREAD_MUTEX_INITIALIZER
};

void *udp_broadcast(void*);

void *tcp_listener(void*);

void *tcp_reviever(void*);

void error(const char *msg)
 {
    perror(msg);
    exit(0);
 }

int main(int argc, char *argv[])
{
	printf("SERVER: start\n");
	pthread_t udp_broadcast_thread, tcp_listener_thread;
	pthread_create(&udp_broadcast_thread, NULL, udp_broadcast, NULL);
	pthread_create(&tcp_listener_thread, NULL, tcp_listener, NULL);
	pthread_join(tcp_listener_thread, NULL);
	pthread_join(udp_broadcast_thread, NULL);
	return 0;
}

void *udp_broadcast(void *arg)
 {
 	printf("SERVER: udp_broadcast: start\n");
	int sock, n; // дескрипторы
	unsigned int length; // размер структуры адреса
	struct sockaddr_in server; // структуры адресов сервера и клиента соответсвенно
	struct hostent *hp; // структура хоста
	tcp_send.allow = 1;
	sock = socket(AF_INET, SOCK_DGRAM, 0);
	if (sock < 0) error("socket");
	server.sin_family = AF_INET; // указание адресации
	hp = gethostbyname(BRDIP); // извлечение хоста
	if (hp==0) error("Unknown host");
	bcopy((char *)hp->h_addr, 
		(char *)&server.sin_addr,
		 hp->h_length);
	server.sin_port = htons(SERVPORT); // извлечение порта
	length=sizeof(struct sockaddr_in);
	while(1)
	{	
		char *msg;
		if(tcp_send.allow == 1){
			msg = ALLOW_SEND;
		} else {
			msg = ALLOW_SEND;
		}
		for(int i = 0; i < PORTRANGE; i++){
			if ((n = sendto(sock,msg,ALDENLEN,0,(const struct sockaddr *)&server,length))<0) 
				error("Sendto");
			server.sin_port = htons(SERVPORT+i);
		}	
		// printf("SERVER: udp_broadcast: packet %s sent\n", msg);
		// sleep(SLEEPTIME); //иначе много трафика гоняет
	}
 }

void *tcp_listener(void *arg)
 {
	printf("SERVER: tcp_listener: start\n");
	
	int sockfd, newsockfd; // дескрипторы сокетов
	char buff[1024];
    socklen_t clilen; // размер адреса клиента типа socklen_t
    struct sockaddr_in serv_addr, cli_addr; // структура сокета сервера и клиента
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // ошибка при создании сокета
	if (sockfd < 0) 
       error("ERROR opening socket");
	 
    bzero((char *) &serv_addr, sizeof(serv_addr)); //зануляем структуру

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(MYPORT);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
              error("ERROR on binding");

	listen(sockfd,5);
    clilen = sizeof(cli_addr);
    int bytes_recv;
	while (1) 
	{
		printf("SERVER: tcp_listener: accept\n");
		newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0) 
			error("ERROR on accept");
		printf("SERVER: tcp_listener: new connection\n");
		pthread_t thread;
       	pthread_create(&thread, NULL, tcp_reviever, (void*)&newsockfd);
    }
    close(newsockfd);
    close(sockfd);
 }

void *tcp_reviever(void *arg)
 {
 	printf("SERVER: tcp_reviever: start\n");
 	char buff[MAXMSGLEN];
 	int bytes_recv;
 	int sock;
 	int *pt = (int*)arg;
 	sock = *pt;
 	fflush(stdout);
	while(1){
		bzero(&buff, MAXMSGLEN);
		// bzero((char*)&bytes_recv, sizeof(int))
		bytes_recv = recv(sock,(char*)&buff[0], MAXMSGLEN, 0);
		if (bytes_recv < 0) error("ERROR reading from socket");
		if(bytes_recv == 0)
			break;
		buff[bytes_recv+1] = 0;
		printf("SERVER: tcp_reviever: recieved %d bytes: ",((int*)buff)[1]);
		// printf("%d %d ", ((int*)buff)[1], ((int*)buff)[0]);
		for(int i = 8; i < ((int*)buff)[1]-1; i++)
		printf("%c", buff[i]);
	printf("\n");
	}
	printf("SERVER: tcp_reviever: connection dropped\n");
	close(sock);
	pthread_exit(0);
 }
