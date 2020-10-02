#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <termios.h>
#include <errno.h>
#include <fcntl.h>

#define SERVER_ADDRESS "localhost"
#define SOURCE_SERVER_PORT 5000
#define SENSOR_SERVER_PORT 8080
#define SEND_BUFFER_SIZE 13
#define BAJADO 0
#define SUBIDO 1
#define DEVICE_BUFFER_SIZE 512

void connecttoserver();
void senddata(char);
void signalHandler(int);
int connecttosensorserver();
int configTTY(int, struct termios*);
char sanitizeInput(char*);

int sockfd, sensor_sockfd;
int exit_flag = 1;
int read_result;


int main(int argc, char* argv[])
{
    if (argc < 2){
        printf("Please specify sensor server LAN IP. Closing...\n");
        return -1;
    }

    struct sigaction sigIntHandler;

    // Set signal handler
    sigIntHandler.sa_handler = signalHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    signal(SIGPIPE, SIG_IGN);

    connecttosensorserver(argv[1]);
    printf("Successfully connected to sensor server. IP: %s\n", argv[1]);

    connecttoserver();


    struct termios tty;
    char action;

    char buffer[DEVICE_BUFFER_SIZE];

    // Implement send logic here
    while(exit_flag)
    {
        memset(buffer, 0, DEVICE_BUFFER_SIZE);
        read_result = read(sensor_sockfd, buffer, sizeof(buffer));

        if (read_result < 0){
            printf("Error on reading device. Closing...\n");
            break;
        }

        else if (read_result == 0){
            continue;
        }

        else{
            action = sanitizeInput(buffer);

            if (action != (char)1 && action != (char)2){
                printf("Invalid action\n");
		continue;
	    }

            senddata(action);
        }

        sleep(1);
    }

    close(sensor_sockfd);
    close(sockfd);

    return 0;
}

int connecttosensorserver(char *ip)
{
    struct sockaddr_in servaddr; 

	sensor_sockfd = socket(AF_INET, SOCK_STREAM, 0); 
	if (sockfd == -1) { 
		printf("Socket creation failed...\n"); 
		return -1; 
	} 
	else
		printf("Socket successfully created..\n"); 
	
    bzero(&servaddr, sizeof(servaddr)); 
	servaddr.sin_family = AF_INET; 
	servaddr.sin_addr.s_addr = inet_addr(ip); 
	servaddr.sin_port = htons(SENSOR_SERVER_PORT); 

	if (connect(sensor_sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) { 
		printf("connection with the server failed...\n"); 
		return -2; 
	} 
	else
		printf("connected to the server..\n"); 

    return 0;
}

void connecttoserver()
{
    struct sockaddr_in serveraddr;

    printf("Connecting to server\n");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&serveraddr,sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr = *((struct in_addr *)gethostbyname(SERVER_ADDRESS)->h_addr);
    serveraddr.sin_port = htons(SOURCE_SERVER_PORT);

    while(exit_flag)
    {
        if(connect(sockfd, (struct sockaddr*) &serveraddr, sizeof(struct sockaddr)))
        {
            printf("Connection refused, retrying in 5 seconds\n");
        }
        else
        {
            printf("Connected to server\n");

            break;
        }

        sleep(5);
    }
}

void senddata(char action)
{
    int nsend;
    char sendbuffer[SEND_BUFFER_SIZE];
    unsigned int type;
    unsigned int length;

    type = 4;
    memcpy(&sendbuffer[0], &type, 1);

    length = 1;
    memcpy(&sendbuffer[1], &length, 2);

    memcpy(&sendbuffer[3], &action, 1);

    nsend = send(sockfd, sendbuffer, SEND_BUFFER_SIZE, 0);

    if(nsend >= 0)
    {
        printf("Action: %d\n%d bytes sent\n", (int)action, nsend);
    }
    else
    {
        printf("Connection lost\n");

        connecttoserver();
    }
}

void signalHandler(int signal)
{
    printf("Signal caught\n");

    exit_flag = 0;
}

char sanitizeInput(char *input){
    char action = input[0];
    if (action <= '0' || action >= '9')
        return (char)127;

    return (char)(action - '0');
}
