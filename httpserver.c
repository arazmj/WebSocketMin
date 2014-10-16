

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>

#include "base64.h"
#include "sha1.h" 

#define CONNMAX 1000
#define BYTES 1024
#define MAGIC_STRING "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"

char *ROOT;
int listenfd, clients[CONNMAX];
//void error(char *);
void startServer(char *);
void respond(int);

char *respond_key(char *key_in);

int main(int argc, char* argv[])
{

    struct sockaddr_in clientaddr;
    socklen_t addrlen;
   
    char c;

    //Default Values PATH = ~/ and PORT=10000
    char PORT[6];
    ROOT = getenv("PWD");
    strcpy(PORT,"10000");

    int slot=0;

    //Parsing the command line arguments
    while ((c = getopt (argc, argv, "p:r:")) != -1)
        switch (c)
        {
            case 'r':
                ROOT = (char *)malloc(strlen(optarg));
                strcpy(ROOT,optarg);
                break;
            case 'p':
                strcpy(PORT,optarg);
                break;
            case '?':
                fprintf(stderr,"Wrong arguments given!!!\n");
                exit(1);
            default:
                exit(1);
        }
    
    printf("Server started at port no. %s%s%s with root directory as %s%s%s\n","\033[92m",PORT,"\033[0m","\033[92m",ROOT,"\033[0m");
    // Setting all elements to -1: signifies there is no client connected
    int i;
    for (i=0; i<CONNMAX; i++)
        clients[i]=-1;
    startServer(PORT);

    // ACCEPT connections
    while (1)
    {
        addrlen = sizeof(clientaddr);
        clients[slot] = accept (listenfd, (struct sockaddr *) &clientaddr, &addrlen);

        if (clients[slot]<0)
            {
                perror ("accept() error");
            }
        else
        {
            if ( fork()==0 )
            {
                respond(slot);
                exit(0);
            }
        }

        while (clients[slot]!=-1) slot = (slot+1)%CONNMAX;
    }

    return 0;
}

//start server
void startServer(char *port)
{
    struct addrinfo hints, *res, *p;

    // getaddrinfo for host
    memset (&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if (getaddrinfo( NULL, port, &hints, &res) != 0)
    {
        perror ("getaddrinfo() error");
        exit(1);
    }
    // socket and bind
    for (p = res; p!=NULL; p=p->ai_next)
    {
        listenfd = socket (p->ai_family, p->ai_socktype, 0);
        if (listenfd == -1) continue;
        if (bind(listenfd, p->ai_addr, p->ai_addrlen) == 0) break;
    }
    if (p==NULL)
    {
        perror ("socket() or bind()");
        exit(1);
    }

    freeaddrinfo(res);

    // listen for incoming connections
    if ( listen (listenfd, 1000000) != 0 )
    {
        perror("listen() error");
        exit(1);
    }
}

//client connection
void respond(int n)
{
    char mesg[99999], *reqline[31], data_to_send[BYTES], path[99999];
    int rcvd, fd, bytes_read;
    char respond[150];

    memset( (void*)mesg, (int)'\0', 99999 );

    rcvd=recv(clients[n], mesg, 99999, 0);

    if (rcvd<0)    // receive error
        fprintf(stderr,("recv() error\n"));
    else if (rcvd==0)    // receive socket closed
        fprintf(stderr,"Client disconnected upexpectedly.\n");
    else    // message received
    {
        reqline[0] = strtok (mesg, " \t\n");
        if ( strncmp(reqline[0], "GET\0", 4)==0 )
        {
            reqline[1] = strtok (NULL, " \t");
            reqline[2] = strtok (NULL, " \t\n");
            if ( strncmp( reqline[2], "HTTP/1.0", 8)!=0 && strncmp( reqline[2], "HTTP/1.1", 8)!=0 )
            {
                write(clients[n], "HTTP/1.0 400 Bad Request\n", 25);
            }
            else
            {
                int sec_key = -1;
                for (int i = 3; i <= 30; i++) {
                    reqline[i] = strtok (NULL, " :\t\r\n"); 
                    if (strcmp (reqline[i], "Sec-WebSocket-Key") == 0)
                        sec_key = i + 1;
                }

                if (sec_key != -1)
                {
                    strcpy(respond, "HTTP/1.1 101 Switching Protocols\r\n"
                                    "Connection: Upgrade\r\n"
                                    "Upgrade: websocket\r\n"
                                    "Sec-WebSocket-Accept: ");
                    char *accept_key = respond_key(reqline[sec_key]);
                    strcat(respond, accept_key);
                    strcat(respond, "\r\n\r\n");
                    write(clients[n], respond, strlen(respond));

                    char test[] = "  Shotormotor!!!";
                    test[0] = 0x81;
                    test[1] = strlen(test) - 2;

                    write(clients[n], test, strlen(test));
   
                }

                if ( strncmp(reqline[1], "/\0", 2)==0 )
                    strcpy(reqline[1], "/index.html");        //Because if no file is specified, index.html will be opened by default (like it happens in APACHE...

                strcpy(path, ROOT);
                strcpy(&path[strlen(ROOT)], reqline[1]);

                if ( (fd=open(path, O_RDONLY))!=-1 )    //FILE FOUND
                {
                    send(clients[n], "HTTP/1.0 200 OK\n\n", 17, 0);
                    while ( (bytes_read=read(fd, data_to_send, BYTES))>0 )
                        write (clients[n], data_to_send, bytes_read);
                }
                else    write(clients[n], "HTTP/1.0 404 Not Found\n", 23); //FILE NOT FOUND
            }
        }
    }

    //Closing SOCKET
    shutdown (clients[n], SHUT_RDWR);         //All further send and recieve operations are DISABLED...
    close(clients[n]);
    clients[n]=-1;
}


char *respond_key(char *key_in) {
    char *key;
    char *base64;
    size_t t;
    SHA1Context sha1context;
    uint8_t Message_Digest[SHA1HashSize];



    key = malloc(strlen(MAGIC_STRING) + strlen(key_in));

    strcpy(key, key_in);
    strcat(key, MAGIC_STRING);
  
    SHA1Reset(&sha1context);
    SHA1Input(&sha1context, (uint8_t *)key, strlen(key));
    SHA1Result(&sha1context, Message_Digest);

    base64 = base64_encode(Message_Digest,
                    SHA1HashSize,
                    &t);

    return base64;
}




