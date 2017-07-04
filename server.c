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
#include <strings.h>
#include "amsg.pb-c.h"

#define ALLOW_SEND "ALL"
#define DENY_SEND "DEN"
#define ALLOW_RECIEV "RCV"
#define DENY_RECIEV "RDN"
#define ALDENLEN 4  
#define MYRCVPORT 32000
#define CLIFTPORT 16000
#define MYSNDPORT 33000
#define CLISTPORT 17000
#define MAXSLEEPTIME 10
#define PORTRANGE 10
#define MAXMSGLEN 40
#define QUEUELEN 5
#define LSTQUEUELEN 5

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
	    pthread_exit(0);
	}

//отправка сообщений в очередь
int put_msg(char *pt) 
	{
		if (qpacket.elements == QUEUELEN)
		{
			printf("queue is full\n");
			return -1;
		}
		pthread_mutex_lock(&qpacket.mutex);
		bcopy(pt, qpacket.packet[qpacket.elements], MAXMSGLEN+12);
		qpacket.elements++;
		pthread_mutex_unlock(&qpacket.mutex);
		return 0;
	}

//вспомогательная функция для сдвига очереди после изъятия сообщения
void queue_sort() 
	{
		bzero(qpacket.packet[0], MAXMSGLEN);

		for(int i = 0; i < qpacket.elements-1; i++)
		{
			bcopy(qpacket.packet[i+1], qpacket.packet[i], MAXMSGLEN);
		}

		qpacket.elements--;
	}

//забрать строку из очереди не забывать ОСВОБОДИТЬ память
char *get_msg() 
	{
		char *pt = (char*) malloc(sizeof(char) * MAXMSGLEN);
		pthread_mutex_lock(&qpacket.mutex);
		if(qpacket.elements == 0)
		{
			return pt;
		}
		bcopy(qpacket.packet[0], pt, MAXMSGLEN);
		qpacket.elements; 
		queue_sort();
		pthread_mutex_unlock(&qpacket.mutex);
		return pt;
	}


int main(int argc, char *argv[])
	{
		if(argc != 3) 
		{
			printf("SERVER: to few arg\nSERVER: use ./server <IP> <K>\n");
			exit(-1);
		}

		qpacket.elements = 0;

		pthread_t udp_broadcast_thread, tcp_recieve_listener_thread, tcp_send_listener_thread;
		pthread_create(&udp_broadcast_thread, NULL, udp_broadcast, (void*)argv);
		pthread_create(&tcp_recieve_listener_thread, NULL, tcp_recieve_listener, NULL);
		pthread_create(&tcp_send_listener_thread, NULL, tcp_send_listener, NULL);
		pthread_join(udp_broadcast_thread, NULL);

		return 0;
	}

void *udp_broadcast(void *arg)
	{
	 	int sock, n, port, K;
		char *msgf, *msgs, *ip;
		unsigned int length;
		struct sockaddr_in server;
		struct hostent *hp;

		ip = ((char**)arg)[1];
		K = atoi(((char**)arg)[2]);

		printf("SERVER: start with arg: IP = %s, K = %d\n", ip, K);
		fflush(stdout);

		if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) 
		{
			error("socket() failed");
		}
		server.sin_family = AF_INET;
		hp = gethostbyname(ip);

		if (hp == 0) 
		{
			error("gethostbyname() failed");
		}

		bcopy((char *)hp->h_addr_list[0], (char *)&server.sin_addr, hp->h_length);
		length = sizeof(struct sockaddr_in);

		while(1)
		{	
			//вещание для клиенов первого типа
			port = CLIFTPORT;
			if(qpacket.elements < QUEUELEN)
			{
				msgf = ALLOW_SEND;
			} else {
				msgf = DENY_SEND;
			}
			server.sin_port = htons(port);
			for(int i = 0; i < PORTRANGE; i++)
			{
				if ((n = sendto(sock,msgf,ALDENLEN,0,(const struct sockaddr *)&server,length)) < 0) 
				{
					error("sendto() failed");
				}
				server.sin_port = htons(port + i);
			}
			//вещание для клиентов второго типа
			port = CLISTPORT;
			if(qpacket.elements > 0)
			{
				msgs = ALLOW_RECIEV;
			} else {
				msgs = DENY_RECIEV;
			}
			server.sin_port = htons(port);
			for(int i = 0; i < PORTRANGE; i++)
			{
				if ((n = sendto(sock,msgs,ALDENLEN,0,(const struct sockaddr *)&server,length)) < 0) 
				{
					error("sendto() failed");
				}
				server.sin_port = htons(port+i);
			}	
			sleep(K);
		}
	}

void *tcp_recieve_listener(void *arg)
	{

		int sockfd, newsockfd;
	    socklen_t clilen;
	    struct sockaddr_in serv_addr, cli_addr;
		
		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) 
	    {
	       error("socket() failed");
	    }
		 
	    bzero((char *) &serv_addr, sizeof(serv_addr));

	    serv_addr.sin_family = AF_INET;
	    serv_addr.sin_addr.s_addr = INADDR_ANY;
	    serv_addr.sin_port = htons(MYRCVPORT);

		if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
		{
			error("bind() failed");
		}

		listen(sockfd,5);
	    clilen = sizeof(cli_addr);

		while (1) 
		{
			newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr, &clilen);
			if (newsockfd < 0) 
			{
				error("accept() failed");
			}
			printf("SERVER: new connection fist type\n");
			pthread_t thread;
	       	pthread_create(&thread, NULL, tcp_reciever, (void*)&newsockfd);
	    }
	    close(newsockfd);
	    close(sockfd);
	}
void *tcp_reciever(void *arg)
	{
	 	char *buff = (char*) malloc(MAXMSGLEN);
	 	int bytes_recv;
	 	int sock;
	 	int *pt = (int*)arg;
	 	sock = *pt;

		while(qpacket.elements < QUEUELEN)
		{
			bzero(buff, MAXMSGLEN);
			bytes_recv = recv(sock,(char*)buff, MAXMSGLEN, 0);

			if (bytes_recv <= 0) 
			{
				error("recv() failed");
			}

			AMessage *pmsg;
			pmsg = amessage__unpack (NULL, bytes_recv, buff);
			if (pmsg == NULL)
			{
				fprintf(stderr, "error unpacking incoming message\n");
				return (void*)1;
			}

			printf("SERVER: revieved %d bytes: ", bytes_recv);
			printf("<=== [%d][%d][%s]\n", pmsg->a, pmsg->b, pmsg->c);

			/*так как неожиданно оказалось что протобуф сжимает данные 
			придется либо переписывать очередь и все что с ней связано,
			либо лепить костыль. Конечно же бдуем лепить костыль :)*/

			buff[bytes_recv+1] = '\0'; //или +1
            put_msg(buff);
			amessage__free_unpacked(pmsg,NULL);
			printf("SERVER: сells in the queue: %d\n\n", QUEUELEN - qpacket.elements);
		}

		printf("SERVER: connection dropped\n");
		// sleep(MAXSLEEPTIME);
		close(sock);
		free(buff);
		pthread_exit(0);
	}

void *tcp_send_listener(void *arg)
	{
		int sockfd, newsockfd;
	    socklen_t clilen;
	    struct sockaddr_in serv_addr, cli_addr;

		sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd < 0) 
	    {
	       error("socket() failed");
	    }
		 
	    bzero((char *) &serv_addr, sizeof(serv_addr));

	    serv_addr.sin_family = AF_INET;
	    serv_addr.sin_addr.s_addr = INADDR_ANY;
	    serv_addr.sin_port = htons(MYSNDPORT);

		if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) 
		{
			error("bind() failed");
		}

		listen(sockfd, LSTQUEUELEN);
	    clilen = sizeof(cli_addr);

		while (1) 
		{
			newsockfd = accept(sockfd,(struct sockaddr *) &cli_addr, &clilen);
			if (newsockfd < 0) 
			{
				error("accept() failed");
			}
			printf("SERVER: new connection second type\n");
			pthread_t thread;
	       	pthread_create(&thread, NULL, tcp_sender, (void*)&newsockfd);
	    }

	    close(newsockfd);
	    close(sockfd);
	}

void *tcp_sender(void *arg)	
 	{
		int my_sock;
		int n, len, sleep_time;
		char *msg;

		my_sock = *((int*)arg);
		while(qpacket.elements > 0)
			{
				msg = get_msg();
				for (int i = 0; i < MAXMSGLEN+1; i++)
				{
					if(msg[i]=='\0'){
						len = i;
						break;
					}
				}
				AMessage *pmsg;
				pmsg = amessage__unpack (NULL, len, msg);
				if (pmsg == NULL)
				{
					fprintf(stderr, "error unpacking incoming message\n");
					return (void*)1;
				}
				sleep_time = pmsg->a;
				if ((n = send(my_sock, msg, len, 0))< 0)
				{
					error("send() failed");
				}

				printf("SERVER: sent %d bytes: ===> [%d][%d][%s]\n", len, pmsg->a, pmsg->b, pmsg->c);

				printf("SERVER: сells in the queue: %d\n\n", QUEUELEN - qpacket.elements);
				amessage__free_unpacked(pmsg,NULL);
				sleep(sleep_time);

			}

			free(msg);
			printf("SERVER: no cell for sending\n");
			// sleep(MAXSLEEPTIME);
			close(my_sock);
			pthread_exit(0);
	}

