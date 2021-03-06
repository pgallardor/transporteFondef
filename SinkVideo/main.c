#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <mysql/mysql.h>
#include <sys/types.h>

#define SERVER_ADDRESS "localhost"
#define MYSQL_USERNAME "fondef"
#define MYSQL_PASSWORD "fondef123"
#define MYSQL_DATABASE "fondef"
#define MYSQL_TABLE_NAME "ON_BOARD_VIDEO"
#define MYSQL_QUERY_LENGTH 1000
#define GPS_INVALID_SPEED -1.0f
#define GPS_THRESHOLD_SPEED 0.3f
#define SINK_SERVER_PORT 5001
#define RECV_BUFFER_SIZE 1000

void connecttoserver();
void connecttodatabase();
void recvdata();
void inserttodatabase(double, double, int, int, unsigned long, int);
void signalHandler(int);
void clear_counters();

int sockfd;
int exit_flag = 1;
double currentlen, currentlat;
//so we can insert to database just when it stopped
unsigned long last_timestamp = -1;
float currentspeed;
MYSQL *MYSQLcon;
//int n_up, n_down;
int n_up[3], n_down[3];

int main()
{
    struct sigaction sigIntHandler;

    // Set signal handler
    sigIntHandler.sa_handler = signalHandler;
    sigemptyset(&sigIntHandler.sa_mask);
    sigIntHandler.sa_flags = 0;
    sigaction(SIGINT, &sigIntHandler, NULL);
    signal(SIGPIPE, SIG_IGN);

    connecttoserver();
    connecttodatabase();

    currentspeed = GPS_INVALID_SPEED;
    clear_counters();

    // Implement receive logic here
    while(exit_flag)
    {
        recvdata();
        if (currentspeed > GPS_THRESHOLD_SPEED){
            int i;
            for (i = 1; i < 3; i++){
                if (n_up[i] + n_down[i] > 0)
                    inserttodatabase(currentlen, currentlat, n_up[i], n_down[i], last_timestamp, i);
            }
            clear_counters();
        }
    }

    mysql_close(MYSQLcon);
    close(sockfd);

    return 0;
}

void connecttoserver()
{
    struct sockaddr_in serveraddr;

    if(!exit_flag)
        return;

    printf("Connecting to server\n");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    bzero(&serveraddr,sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr = *((struct in_addr *)gethostbyname(SERVER_ADDRESS)->h_addr);
    serveraddr.sin_port = htons(SINK_SERVER_PORT);

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

void clear_counters(){
    memset(n_up, 0, 3 * sizeof(int));
    memset(n_down, 0, 3 * sizeof(int));
}

void recvdata()
{
    int nrecv;
    char recvbuffer[RECV_BUFFER_SIZE];
    int i;
    unsigned int type;
    unsigned long timestamp;
    unsigned int length;
    char action;

    // Receive 11 bytes first from the header
    nrecv = recv(sockfd, recvbuffer, 11, 0);

    if(nrecv > 0)
    {
        //printf("**************************************************\n%d header bytes received\n", nrecv);

        type = 0;
        memcpy(&type, &recvbuffer[0], 1);
        //printf("Type: %u\n", type);

        timestamp = 0;
        memcpy(&timestamp, &recvbuffer[1], 8);
        //printf("Timestamp: %lu\n", timestamp);

        length = 0;
        memcpy(&length, &recvbuffer[9], 2);
       // printf("Length: %u\n", length);

        // Now receive the data
        nrecv = recv(sockfd, &recvbuffer[11], length, 0);
        //printf("%d data bytes received\n", nrecv);

        action = 0;
        memcpy(&action, &recvbuffer[11], 1);
	
        switch(type){
            case 2:
                if (currentspeed > GPS_THRESHOLD_SPEED) break;

                last_timestamp = timestamp;
                dev_id = -1;
                memcpy(&dev_id, &recvbuffer[11], 1);

                action = 0;
                memcpy(&action, &recvbuffer[12], 1);

                if (dev_id < 0 || dev_id > 2){
                    printf("Error: Unknown device id\n");
                    break;
                }

                printf("Action received: %d", (int)action);
                if (action == 1) n_up[dev_id] += 1;
                else if (action == 2) n_down[dev_id] += 1;
                break;

            case 128:
                memcpy(&currentlat, &recvbuffer[11], 8);
                memcpy(&currentlon, &recvbuffer[19], 8);
                memcpy(&currentspeed, &recvbuffer[31], 4);
                break;
            default:
                break;
        }
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

void connecttodatabase()
{
    while(exit_flag)
    {
        MYSQLcon = mysql_init(NULL);

        if(MYSQLcon == NULL)
        {
            printf("DB connection initialization failed, retrying in 5 seconds\n");
        }
        else
        {
            printf("DB connection initialization succeeded\n");

            if (mysql_real_connect(MYSQLcon, SERVER_ADDRESS, MYSQL_USERNAME, MYSQL_PASSWORD, MYSQL_DATABASE, 0, NULL, 0) == NULL)
            {
                mysql_close(MYSQLcon);

                printf("Connection to DB server failed: %s, retrying in 5 seconds\n", mysql_error(MYSQLcon));
            }
            else
            {
                printf("DB connection established\n");

                break;
            }
        }

        sleep(5);
    }
}

void inserttodatabase(double longitude, double latitude, int up, int down, unsigned long timestamp, int device_id){
    char query[MYSQL_QUERY_LENGTH];

    sprintf(query, "INSERT INTO %s VALUES (%i, '%.9f', '%.9f', %i, %i, FROM_UNIXTIME(%lu))",
            MYSQL_TABLE_NAME,
            device_id,
            latitude,
            longitude,
            up,
            down,
            timestamp/1000);

    printf("%s\n", query);

    if (mysql_query(MYSQLcon, query))
    {
        fprintf(stderr, "Query error: %s\n", mysql_error(MYSQLcon));

        mysql_close(MYSQLcon);

        connecttodatabase();
    }
}
