#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <stdio.h>
#include <pthread.h>
#include <arpa/inet.h>

#define MYPORT 16000
#define SERVPORT 32000
#define ALLOW_SEND "ALL"
#define DENY_SEND "DEN"
#define ALDENLEN 4
#define PORTRANGE 10
#define MAXMSGLEN 40
#define MINMSGLEN 10
#define SLEEP 5

struct {
    pthread_mutex_t mutex;
    int allow;
} tcp_send = { 
    PTHREAD_MUTEX_INITIALIZER
};

void *udp_listener(void*);

void *tcp_sender(void*);

char *packet_generator();

void error(const char *msg)
    {
        perror(msg);
        pthread_exit(0);
    }

int main(int argc, char *argv[])
    {
      printf("CLIENT 1 type: start\n");
      pthread_t udp_listener_thread, tcp_sender_thread;
      pthread_create(&udp_listener_thread, NULL, udp_listener, NULL);
      pthread_join(udp_listener_thread, NULL);
      return 0;
    }

void *udp_listener(void *arg)
    {
      int bytes_recv, mysocket, client_addr_size;
      char buf[ALDENLEN];
      char *addr;
      struct sockaddr_in sockaddr_in;
      struct sockaddr_in client_addr;
      struct sockaddr_in local_addr;
      struct hostent *hst;
      socklen_t fromlen;
      pthread_t thread;

      char *all = ALLOW_SEND;
      char *den = DENY_SEND;
      
      if ((mysocket = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
      {
        error("socket() failed");
      }

      local_addr.sin_family = AF_INET;
      local_addr.sin_port = htons(MYPORT);
      local_addr.sin_addr.s_addr = INADDR_ANY;

      if (bind(mysocket, (struct sockaddr *)&local_addr, sizeof(local_addr)) < 0)
      {
        printf("CLIENT 1 type: search for a free port\n");
        int i = 0;
        while(i < PORTRANGE)
        {
          i++;
          local_addr.sin_port = htons(MYPORT + i);
          if (!bind(mysocket, (struct sockaddr *)&local_addr, sizeof(local_addr)))
          {
               break;
          }
        }
      }

      client_addr_size = sizeof(struct sockaddr_in);

      printf("CLIENT 1 type: listening broadcast\n");

      tcp_send.allow = 0;

      while (1) 
      {
        if ((bytes_recv = recvfrom(mysocket, &buf[0], sizeof(buf) - 1, 0, (struct sockaddr *)&client_addr, &client_addr_size)) < 0)
        	{
        		error("recvfrom");
        	}
        hst = gethostbyaddr((char *)&client_addr.sin_addr, 4, AF_INET);
        addr = (char*)inet_ntoa(client_addr.sin_addr);
        buf[bytes_recv] = 0;
       
        if(!strcmp(buf, all))
        {
            if(tcp_send.allow == 0)
            {
            	pthread_mutex_lock(&tcp_send.mutex);
	            tcp_send.allow = 1;
	            pthread_mutex_unlock(&tcp_send.mutex);
	            pthread_create(&thread, NULL, tcp_sender, (void*)addr);
            }
        }
        if (!strcmp(buf, den))
        { 
            pthread_mutex_lock(&tcp_send.mutex);
            tcp_send.allow = 0;
            pthread_mutex_unlock(&tcp_send.mutex);
            pthread_cancel(thread);
        }
      }
    }

void *tcp_sender(void *arg)
   {
      int my_sock;
      int n, len, sleep_time;
      struct sockaddr_in serv_addr;
      struct hostent *server;
      char *msg;
      char *addr;

      addr = (char*)arg;

      printf("CLIENT 1 type: start.. connecting to %s:%d\n", addr, SERVPORT);

      my_sock = socket(AF_INET, SOCK_STREAM, 0);
      if (my_sock < 0)
         {
        	 error("socket() failed");
         }
      server = gethostbyname(addr);
      if (server == NULL) 
      {
          fprintf(stderr,"socket() failed\n");
          exit(0);
        }
      bzero((char *) &serv_addr, sizeof(serv_addr));

      serv_addr.sin_family = AF_INET;
      
      bcopy((char *)server->h_addr_list[0], (char *)&serv_addr.sin_addr.s_addr, server->h_length);
      serv_addr.sin_port = htons(SERVPORT);

      if (connect(my_sock,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
            {
            	error("connect() failed");
            }

      printf("CLIENT 1 type: connected\n");

      while (tcp_send.allow == 1)
         {
            fflush(stdout);
            msg = packet_generator();
            sleep_time = ((int*)msg)[0];
            len = ((int*)msg)[1];

            if ((n = send(my_sock, msg, len, 0))< 0)
            {
              error("ERROR send to socket");
            }

            printf("CLIENT 1 type: sent %d bytes: ", ((int*)msg)[1]);

            for(int i = 8;i<((int*)msg)[1]-1;i++)
            {
              printf("%c", msg[i]);
            }

            free(msg);
            printf("\nCLIENT 1 type: sleep %d sec\n", sleep_time);
            sleep(sleep_time);
         }
      pthread_mutex_lock(&tcp_send.mutex);
      tcp_send.allow = 0;
      pthread_mutex_unlock(&tcp_send.mutex);
      printf("CLIENT1: no cells in the queue\n");
      close(my_sock);  
      pthread_exit(0);
   }

char *packet_generator()
   {
      char *packet;
      srand(time(NULL)+getpid());
      static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
      int length = 2 * sizeof(int)+rand()%(MAXMSGLEN-2 * sizeof(int) - MINMSGLEN)+MINMSGLEN;
      packet = malloc(sizeof(char) * length);
      ((int*)packet)[0] = rand()%SLEEP + 1;
      ((int*)packet)[1] = length;

      for(int i = 8; i < length; i++) 
      {
        packet[i] = charset[rand()%(strlen(charset))];
      }
      packet[length-1] = '\0';
      return (char*)packet;
   }