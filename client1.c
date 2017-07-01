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

#define MYPORT 16000
#define SERVPORT 32000
#define ALLOW_SEND "ALL"
#define DENY_SEND "DEN"
#define ALDENLEN 4
#define PORTRANGE 10

#define MAXMSGLEN 50 //максимальная длина 
#define SLEEP 3

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
    exit(0);
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
  int bytes_recv;
  struct sockaddr_in sockaddr_in; // структура адреса сервера
  struct sockaddr_in client_addr; // структура адреса клиента
  char buf[ALDENLEN]; // буфер
  socklen_t fromlen;
  char *addr;
  tcp_send.allow = 0;
  printf("CLIENT 1 type: udp_listener: start\n");

  int mysocket,client_addr_size;
  if ((mysocket=socket(AF_INET,SOCK_DGRAM,0))<0)
  {error("Opening socket");}

  struct sockaddr_in local_addr;
  local_addr.sin_family = AF_INET;
  local_addr.sin_port = htons(MYPORT);
  local_addr.sin_addr.s_addr = INADDR_ANY;

  char *all = ALLOW_SEND;
  char *den = DENY_SEND;

  if (bind(mysocket, (struct sockaddr *)&local_addr, sizeof(local_addr))<0)
  {
    //шаманство с портами
    printf("CLIENT 1 type: udp_listener: search port\n");
    int i=0;
    while(i<PORTRANGE){
      i++;
      local_addr.sin_port = htons(MYPORT+i);
      if (!bind(mysocket, (struct sockaddr *)&local_addr, sizeof(local_addr))){
        printf("%d\n", i);
        break;
      }
    }
  }
  client_addr_size = sizeof(struct sockaddr_in);
  printf("CLIENT 1 type: udp_listener: listening\n");
  while (1) 
  {
    if ((bytes_recv = recvfrom(mysocket,&buf[0],sizeof(buf)-1,0,(struct sockaddr *)&client_addr, &client_addr_size)) < 0)
    {error("recvfrom");}

    struct hostent *hst;
    hst = gethostbyaddr((char *)&client_addr.sin_addr, 4, AF_INET);
    addr = (char*)inet_ntoa(client_addr.sin_addr);
    // printf("serv %s\n", (char*)inet_ntoa(client_addr.sin_addr)); // определение адреса сервера
    buf[bytes_recv] = 0;
    printf("CLIENT 1 type: udp_listener: recived %s packet from %s\n",  &buf[0], (char*)inet_ntoa(client_addr.sin_addr));
    //buf[0] = 'A'
    if(!strncmp(buf, all, ALDENLEN)){
      if(tcp_send.allow == 0){
        tcp_send.allow = 1;
        pthread_t thread;
        pthread_create(&thread, NULL, tcp_sender, (void*)addr);
      }
     } else if (!strncmp(buf, den, ALDENLEN)){ 
        tcp_send.allow = 0;
    }
  }
 }

void *tcp_sender(void *arg)
 {
  int my_sock, n;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  char *msg;
  char *addr;

  printf("CLIENT 1 type: tcp_sender: start\n");
  
  addr = (char*)arg;
  my_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (my_sock < 0) 
        error("ERROR opening socket");
    // извлечение хоста
  server = gethostbyname(addr);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host\n");
        exit(0);
    }
  bzero((char *) &serv_addr, sizeof(serv_addr)); //очистка

    serv_addr.sin_family = AF_INET;

    bcopy((char *)server->h_addr, 
         (char *)&serv_addr.sin_addr.s_addr,
         server->h_length);
  serv_addr.sin_port = htons(SERVPORT);
  if (connect(my_sock,(struct sockaddr *) &serv_addr,sizeof(serv_addr)) < 0) 
        error("ERROR connecting");

  while (tcp_send.allow == 1)
     {
        msg = packet_generator();
        int k = send(my_sock, msg, ((int*)msg)[1], 0);
        if (k < 0) error("ERROR send to socket");
        printf("CLIENT 1 type: tcp_sender: sent %d bytes: ", ((int*)msg)[1]);
        for(int i = 8;i<((int*)msg)[1]-1;i++)
          printf("%c", msg[i]);
        printf("\n");
        sleep(SLEEP);
     }
     close(my_sock);
     pthread_exit(0);
 }

char *packet_generator()
 {
  char *packet;
  srand(time(NULL));
  static char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  int length = 2 * sizeof(int)+rand()%(MAXMSGLEN-2 * sizeof(int));
  packet = malloc(sizeof(char) * length);
  ((int*)packet)[0] = 1;
  ((int*)packet)[1] = length; //длина всей посылки
  for(int i = 8; i < length; i++) 
    packet[i] = charset[rand()%(strlen(charset))];
  packet[length-1] = "/0";
  return (char*)packet;
 }