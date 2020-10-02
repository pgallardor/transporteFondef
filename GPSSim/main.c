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
#define SEND_BUFFER_SIZE 13
#define BAJADO 0
#define SUBIDO 1
#define DEVICE_BUFFER_SIZE 512

void connecttoserver();
void senddata(double, double, float);
void signalHandler(int);

int sockfd;
int exit_flag = 1;
int read_result;


int main(int argc, char* argv[])
{
    struct sigaction sigIntHandler;

    // Set signal handler
    sigIntHandler.sa_handler = signalHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    signal(SIGPIPE, SIG_IGN);
    FILE *f;
    connecttoserver();


    // Implement send logic here
    while(exit_flag)
    {
        double lat, lon;
        float speed;
        f = fopen('route.txt', 'r');

        while(fscanf(f, "%lf %lf %f", &lat, &lon, %speed) != EOF){
            printf("%.3f %.3f %.3f\n", lat, lon, speed);
            senddata(lat, lon, speed);
            sleep(2);
        }

        fclose(f);
        sleep(1);
    }

    close(sockfd);

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

void senddata(double lat, double lon, float speed)
{
    int nsend;
    char sendbuffer[SEND_BUFFER_SIZE];
    unsigned int type;
    unsigned int length;

    type = 128;
    memcpy(&sendbuffer[0], &type, 1);

    length = 20;
    memcpy(&sendbuffer[1], &length, 2);

    memcpy(&sendbuffer[3], &lat, 8);
    memcpy(&sendbuffer[11], &lon, 8);
    memcpy(&sendbuffer[19], &speed, 4);

    nsend = send(sockfd, sendbuffer, SEND_BUFFER_SIZE, 0);

    if(nsend >= 0)
    {
        printf("**************************************************\n%d bytes sent\n", nsend);
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
