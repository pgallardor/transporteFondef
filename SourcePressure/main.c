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
#define SEND_BUFFER_SIZE 4
#define BAJADO 0
#define SUBIDO 1
#define DEVICE_BUFFER_SIZE 512

void connecttoserver();
void senddata(char);
void signalHandler(int);
int configTTY(int, struct termios*);
char *sanitizeInput(char*);
char *countActions(char*);

int sockfd;
int exit_flag = 1;
int read_result;


int main(int argc, char* argv[])
{
    if (argc < 2){
        printf("Please specify device directory '/dev/XXX'. Closing...\n");
        return -1;
    }

    struct sigaction sigIntHandler;

    // Set signal handler
    sigIntHandler.sa_handler = signalHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    signal(SIGPIPE, SIG_IGN);

    connecttoserver();

    printf("Trying to connect to device %s\n", argv[1]);
    int serialportfd = open(argv[1], O_RDWR | O_NOCTTY );

    if (serialportfd < 0){
        printf("Error %d opening device %s.\n", errno, argv[1]);
        printf("%s\n", strerror(errno));
        return -2;
    }
    senddata((char)1);
    senddata((char)-1);

    printf("Successfully connected to device.\n");

    struct termios tty;
    char *snt;
    char *actns;

    if (configTTY(serialportfd, &tty) < 0){
        printf("Error configuring device.\n");
        return -3;
    }

    char buffer[DEVICE_BUFFER_SIZE];

    // Implement send logic here
    while(exit_flag)
    {
        memset(buffer, 0, DEVICE_BUFFER_SIZE);
        read_result = read(serialportfd, &buffer, sizeof(buffer));

        if (read_result < 0){
            printf("Error on reading device. Closing...\n");
            break;
        }

        else if (read_result == 0){
            //printf("Nothing has happened.\n");
            continue;
        }

        else{
            snt = sanitizeInput(buffer);

            if (strlen(snt) == 0)
                continue;

            actns = countActions(snt);
	    printf("Up: %d, down: %d\n", actns[SUBIDO], actns[BAJADO]);
            
	    int i;
            for (i = 0; i < actns[SUBIDO]; i++) senddata((char)1);
            for (i = 0; i < actns[BAJADO]; i++) senddata((char)-1);

            free(snt);
            free(actns);
        }

        sleep(1);
    }

    close(serialportfd);
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

void senddata(char action)
{
    int nsend;
    char sendbuffer[SEND_BUFFER_SIZE];
    unsigned int type;
    unsigned int length;

    type = 3;
    memcpy(&sendbuffer[0], &type, 1);

    length = 1;
    memcpy(&sendbuffer[1], &length, 2);

    memcpy(&sendbuffer[3], &action, 1);

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

int configTTY(int spfd, struct termios *tty){
    
    if ( tcgetattr ( spfd, tty ) != 0 ){
        printf("Error: %s\n", strerror(errno));
        return -1;
    }

    cfsetospeed (tty, B115200);
    cfsetispeed (tty, B115200);

    /* Setting other Port Stuff */
    tty->c_cflag     &=  ~PARENB;        // Make 8n1
    tty->c_cflag     &=  ~CSTOPB;
    tty->c_cflag     &=  ~CSIZE;
    tty->c_cflag     |=  CS8;
    tty->c_cflag     &=  ~CRTSCTS;       // no flow control
    tty->c_lflag     =   0;          // no signaling chars, no echo, no canonical processing
    tty->c_oflag     =   0;                  // no remapping, no delays
    tty->c_cc[VMIN]      =   0;                  // read doesn't block
    tty->c_cc[VTIME]     =   5;                  // 0.5 seconds read timeout

    tty->c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines
    tty->c_iflag     &=  ~(IXON | IXOFF | IXANY);// turn off s/w flow ctrl
    tty->c_lflag     &=  ~(ICANON | ECHO | ECHOE | ISIG); // make raw
    tty->c_oflag     &=  ~OPOST;              // make raw

    /* Flush Port, then applies attributes */
    tcflush( spfd, TCIFLUSH );

    if ( tcsetattr ( spfd, TCSANOW, tty ) != 0){
        printf("Error: %s\n", strerror(errno));
        return -2;    
    }

    return 0;
}

char *sanitizeInput(char *input){
    int i, cnt = 0, input_length = strlen(input);
    char *sanitized = (char*) malloc(input_length);
    
    for (i = 0; i < input_length; i++){
        if (input[i] == '1' || input[i] == '2'){
            sanitized[cnt++] = input[i];
        }
    }

    sanitized[cnt] = '\0';
    sanitized = realloc(sanitized, cnt + 1);
    return sanitized;
}

char *countActions(char *actions_array){
    char *actions_counter = (char*) calloc(2, sizeof(char));
    int i, actions_length = strlen(actions_array);

    for (i = 0; i < actions_length; i++){
        if (actions_array[i] == '1')
            actions_counter[SUBIDO] += (char)1;

        else if (actions_array[i] == '2') 
            actions_counter[BAJADO] += (char)1;
    }

    return actions_counter;
}
