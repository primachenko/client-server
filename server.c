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
#define BRDIP "127.0.0.255" //перенести как аргумент при запуске
#define CLIFTPORT 16000
#define MYRCVPORT 32000
#define PORTRANGE 10
#define MAXMSGLEN 40 //максимальная длина 

#define QUEUELEN 100
#define ALLOW_RECIEV "RCV"
#define DENY_RECIEV "RDN"
#define MYSNDPORT 33000
#define CLISTPORT 17000

struct {
    pthread_mutex_t mutex;
    int allow;
} tcp_send = { 
    PTHREAD_MUTEX_INITIALIZER
};

struct {
    pthread_mutex_t mutex;
    int allow;
} tcp_recv = { 
    PTHREAD_MUTEX_INITIALIZER
};

struct queue
{
	pthread_mutex_t mutex;
	int elements;
	char packet[QUEUELEN][MAXMSGLEN];
} qpacket = {
	PTHREAD_MUTEX_INITIALIZER
};
//широковещание обеим типам клиентов
void *udp_broadcast(void*);

//приемник tcp подключений от клиентов первого типа
void *tcp_recieve_listener(void*);

//обработчик tcp подключений от клиентов первого типа
void *tcp_reciever(void*);

//приемник tcp подключений от клиентов второго типа
void *tcp_send_listener(void*);

//обработчик tcp подключений от клиентов первого типа
void *tcp_sender(void*);

//обработчик ошибок
void error(const char *msg)
 {
    perror(msg);
    exit(0);
 }

//отправка сообщений в очередь
int put_msg(char *pt) 
	{
		if (qpacket.elements == QUEUELEN){
			printf("queue is full\n");
			return -1; //queue is full
		}
		pthread_mutex_lock(&qpacket.mutex);
		bcopy(pt, qpacket.packet[qpacket.elements], MAXMSGLEN);
		qpacket.elements++;
		pthread_mutex_unlock(&qpacket.mutex);
		return 0; //элемент успешно лег в очередь
	}

//вспомогательная функция для сдвига очереди после изъятия сообщения
void queue_sort() 
	{
		bzero(qpacket.packet[0], MAXMSGLEN);
		for(int i = 0; i < qpacket.elements-1; i++) //или elem-2
			bcopy(qpacket.packet[i+1], qpacket.packet[i], MAXMSGLEN);
		qpacket.elements--;
	}

//забрать строку из очереди ОСВОБОДИТЬ память
char *get_msg() 
	{
		char *pt = (char*) malloc(sizeof(char) * MAXMSGLEN);
		pthread_mutex_lock(&qpacket.mutex);
		if(qpacket.elements == 0)
			return pt; //queue is empty
		bcopy(qpacket.packet[0], pt, MAXMSGLEN);
		qpacket.elements; 
		queue_sort();
		pthread_mutex_unlock(&qpacket.mutex);
		return pt;
	}


int main(int argc, char *argv[])
	{
		printf("SERVER: start\n");

		qpacket.elements = 0;
		pthread_t udp_broadcast_thread, tcp_recieve_listener_thread, tcp_send_listener_thread;
		pthread_create(&udp_broadcast_thread, NULL, udp_broadcast, NULL);
		pthread_create(&tcp_recieve_listener_thread, NULL, tcp_recieve_listener, NULL);
		pthread_create(&tcp_send_listener_thread, NULL, tcp_send_listener, NULL);
		pthread_join(udp_broadcast_thread, NULL);

		return 0;
	}

void *udp_broadcast(void *arg)
	{
	 	printf("SERVER: start broadcasting\n");

		int sock, n, port; // дескрипторы
		char *msgf, *msgs; //отправляемые строки
		unsigned int length; // размер структуры адреса
		struct sockaddr_in server; // структура адреса клиента
		struct hostent *hp; // структура хоста
		tcp_send.allow = 1; //мютексы вообще по ходу не нужны
		tcp_recv.allow = 0; //ВЫСТАВИТЬ 0
		sock = socket(AF_INET, SOCK_DGRAM, 0);

		if (sock < 0) 
			error("socket");
		server.sin_family = AF_INET; // указание адресации
		hp = gethostbyname(BRDIP); // извлечение хоста
		if (hp == 0) 
			error("Unknown host");
		bcopy((char *)hp->h_addr, (char *)&server.sin_addr, hp->h_length);
		length = sizeof(struct sockaddr_in);
		
		while(1)
			{	
				//вещание для клиенов первого типа
				port = CLIFTPORT;
				if(qpacket.elements < QUEUELEN){
					msgf = ALLOW_SEND;
				} else {
					msgf = DENY_SEND;
				}
				server.sin_port = htons(port);
				for(int i = 0; i < PORTRANGE; i++){
					if ((n = sendto(sock,msgf,ALDENLEN,0,(const struct sockaddr *)&server,length))<0) 
						error("sendto()");
					server.sin_port = htons(port+i);
				}
				//вещание для клиентов второго типа
				port = CLISTPORT;
				if(qpacket.elements > 0){
					msgs = ALLOW_RECIEV;
				} else {
					msgs = DENY_RECIEV;
				}
				server.sin_port = htons(port);
				for(int i = 0; i < PORTRANGE; i++){
					if ((n = sendto(sock,msgs,ALDENLEN,0,(const struct sockaddr *)&server,length))<0) 
						error("sendto()");
					server.sin_port = htons(port+i);
				}	
			}
	}

void *tcp_recieve_listener(void *arg)
 {
	printf("SERVER: tcp_recieve_listener: start\n");
	
	int sockfd, newsockfd; // дескрипторы сокетов
    socklen_t clilen; // размер адреса клиента типа socklen_t
    struct sockaddr_in serv_addr, cli_addr; // структура сокета сервера и клиента
	
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // ошибка при создании сокета
	if (sockfd < 0) 
       error("ERROR opening socket");
	 
    bzero((char *) &serv_addr, sizeof(serv_addr)); //зануляем структуру

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(MYRCVPORT);

	if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
              error("ERROR on binding");

	listen(sockfd,5);
    clilen = sizeof(cli_addr);
	while (1) 
	{
		newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr, &clilen);
		if (newsockfd < 0) 
			error("ERROR on accept");
		printf("SERVER: tcp_recieve_listener: new connection\n");
		pthread_t thread;
       	pthread_create(&thread, NULL, tcp_reciever, (void*)&newsockfd);
    }
    close(newsockfd);
    close(sockfd);
 }

void *tcp_reciever(void *arg)
 {
 	printf("SERVER: tcp_reciever: start\n");
 	char buff[MAXMSGLEN];
 	char *msg = (char*) malloc(sizeof(char) * MAXMSGLEN);
 	int bytes_recv;
 	int sock;
 	int *pt = (int*)arg;
 	sock = *pt;
 	fflush(stdout);
	while(qpacket.elements < QUEUELEN){
		bzero(&buff, MAXMSGLEN);
		bytes_recv = recv(sock,(char*)&buff[0], MAXMSGLEN, 0);
		if (bytes_recv < 0) error("ERROR reading from socket");
		if(bytes_recv == 0)
			break;
		buff[bytes_recv+1] = '\0'; //илил не нужно
		printf("SERVER: tcp_reciever: recieved %d bytes\n",((int*)buff)[1]);

	put_msg(&buff[0]); //отправка сообщения в очередь
	printf("SERVER: tcp_reciever: add in queue: ");
	for(int i = 8; i < ((int*)buff)[1]-1; i++)
			printf("%c", buff[i]);
	printf("\n");
	}
	printf("SERVER: tcp_reciever: connection dropped\n");
	close(sock);
	free(msg); //ОСВОБОДИЛ
	pthread_exit(0);
 }

void *tcp_send_listener(void *arg)
	{
		printf("SERVER: tcp_send_listener: start\n");
		
		int sockfd, newsockfd; // дескрипторы сокетов
	    socklen_t clilen; // размер адреса клиента типа socklen_t
	    struct sockaddr_in serv_addr, cli_addr; // структура сокета сервера и клиента
		
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
	    // ошибка при создании сокета
		if (sockfd < 0) 
	       error("ERROR opening socket");
		 
	    bzero((char *) &serv_addr, sizeof(serv_addr)); //зануляем структуру

	    serv_addr.sin_family = AF_INET;
	    serv_addr.sin_addr.s_addr = INADDR_ANY;
	    serv_addr.sin_port = htons(MYSNDPORT);

		if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
			error("ERROR on binding");

		listen(sockfd,5);
	    clilen = sizeof(cli_addr);
		while (1) 
		{
			newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr, &clilen);
			if (newsockfd < 0) 
				error("ERROR on accept");
			printf("SERVER: tcp_send_listener: new connection\n");
			pthread_t thread;
	       	pthread_create(&thread, NULL, tcp_sender, (void*)&newsockfd);
	    }
	    close(newsockfd);
	    close(sockfd);
	}

void *tcp_sender(void *arg)	
 	{
		int my_sock;
		int n, len;
		char *msg;

		printf("SERVER: tcp_sender: start\n");

		my_sock = *((int*)arg);
		msg = get_msg();
		len = ((int*)msg)[1];

		if ((n = send(my_sock, msg, len, 0))< 0)
			error("ERROR send to socket");

		printf("SERVER: tcp_sender: sent %d bytes: ", len);
		for(int i = 8;i < len;i++) //len-1?
			printf("%c", msg[i]);
		printf("\n");
		free(msg);
		close(my_sock);
		printf("exit\n");
		pthread_exit(0);
	}

