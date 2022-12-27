/*-------------------------------------------------------------------------------
 * dsd_rigctl.c
 * Simple RIGCTL Client for DSD (remote control of GQRX, SDR++, etc)
 *
 * Portions from https://github.com/neural75/gqrx-scanner
 *
 * LWVMOBILE
 * 2022-10 DSD-FME Florida Man Edition
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>

//UDP Specific
#include <arpa/inet.h>

#define BUFSIZE         1024
#define FREQ_MAX        4096
#define SAVED_FREQ_MAX  1000
#define TAG_MAX         100

//
// error - wrapper for perror
//
void error(char *msg) {
    perror(msg);
    exit(0);
}

//
// Connect
//
int Connect (char *hostname, int portno)
{
    int sockfd, n;
    struct sockaddr_in serveraddr;
    struct hostent *server;


    /* socket: create the socket */
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
      fprintf(stderr,"ERROR opening socket\n");
      error("ERROR opening socket");
    }
        

    /* gethostbyname: get the server's DNS entry */
    server = gethostbyname(hostname);
    if (server == NULL) {
        fprintf(stderr,"ERROR, no such host as %s\n", hostname);
        //exit(0);
        return (0); //return 0, check on other end and configure pulse input 
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
	  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
    serveraddr.sin_port = htons(portno);

    /* connect: create a connection with the server */
    if (connect(sockfd, (const struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0)
    {
        fprintf(stderr,"ERROR opening socket\n");
        return (0);
    }      

    return sockfd;
}
//
// Send
//
bool Send(int sockfd, char *buf)
{
    int n;

    n = write(sockfd, buf, strlen(buf));
    if (n < 0)
      error("ERROR writing to socket");
    return true;
}

//
// Recv
//
bool Recv(int sockfd, char *buf)
{
    int n;

    n = read(sockfd, buf, BUFSIZE);
    if (n < 0)
      error("ERROR reading from socket");
    buf[n]= '\0';
    return true;
}


//
// GQRX Protocol
//
long int GetCurrentFreq(int sockfd) 
{
    long int freq = 0;
    char buf[BUFSIZE];
    char * ptr;
    char * token;

    Send(sockfd, "f\n");
    Recv(sockfd, buf); 

    if (strcmp(buf, "RPRT 1") == 0 ) 
        return freq;

    token = strtok (buf, "\n"); 
    freq = strtol (token, &ptr, 10); 
    // fprintf (stderr, "\nRIGCTL VFO Freq: [%ld]\n", freq);
    return freq;
}

bool SetFreq(int sockfd, long int freq)
{
    char buf[BUFSIZE];

    sprintf (buf, "F %ld\n", freq); 
    Send(sockfd, buf);
    Recv(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0 )
        return false;

    return true;
}

bool SetModulation(int sockfd, int bandwidth) 
{
    char buf[BUFSIZE];
    //the bandwidth is now a user/system based configurable variable
    sprintf (buf, "M FM %d\n", bandwidth); 
    Send(sockfd, buf);
    Recv(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0 )
        return false;

    return true;
}

bool GetSignalLevel(int sockfd, double *dBFS)
{
    char buf[BUFSIZE];

    Send(sockfd, "l\n");
    Recv(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0 )
        return false;

    sscanf(buf, "%lf", dBFS);
    *dBFS = round((*dBFS) * 10)/10;

    if (*dBFS == 0.0)
        return false;
    return true;
}

bool GetSquelchLevel(int sockfd, double *dBFS)
{
    char buf[BUFSIZE];

    Send(sockfd, "l SQL\n");
    Recv(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0 )
        return false;

    sscanf(buf, "%lf", dBFS);
    *dBFS = round((*dBFS) * 10)/10;

    return true;
}

bool SetSquelchLevel(int sockfd, double dBFS)
{
    char buf[BUFSIZE];

    sprintf (buf, "L SQL %f\n", dBFS);
    Send(sockfd, buf);
    Recv(sockfd, buf);

    if (strcmp(buf, "RPRT 1") == 0 )
        return false;

    return true;
}
//
// GetSignalLevelEx
// Get a bunch of sample with some delay and calculate the mean value
//
bool GetSignalLevelEx(int sockfd, double *dBFS, int n_samp)
{
    double temp_level;
    *dBFS = 0;
    int errors = 0;
    for (int i = 0; i < n_samp; i++)
    {
        if ( GetSignalLevel(sockfd, &temp_level) )
            *dBFS = *dBFS + temp_level;
        else
            errors++;
        usleep(1000);
    }
    *dBFS = *dBFS / (n_samp - errors);
    return true;
}

//shoe in UDP input connection here...still having issues that I don't know how to resolve
int UDPBind (char *hostname, int portno)
{
    int sockfd, n;
    struct sockaddr_in serveraddr, client_addr;
    struct hostent *server;

    /* socket: create the socket */
    //UDP socket
    sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    if (sockfd < 0)
    {
      fprintf(stderr,"ERROR opening UDP socket\n");
      error("ERROR opening UDP socket");
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY
    serveraddr.sin_port = htons(portno);

    //Bind socket to listening
    if (bind(sockfd, (struct sockaddr *) &serveraddr,  sizeof(serveraddr)) < 0) { 
		perror("ERROR on binding UDP Port");
	}

    return sockfd;
}

void rtl_udp_tune(dsd_opts * opts, dsd_state * state, long int frequency) 
{
    int handle; 
    unsigned short udp_port = opts->rtl_udp_port; 
    char data[5] = {0}; //data buffer size is 5 for UDP frequency tuning
    struct sockaddr_in address;

    uint32_t new_freq = frequency;
    opts->rtlsdr_center_freq = new_freq; //for ncurses terminal display after rtl is started up

    handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    data[0] = 0;
    data[1] = new_freq & 0xFF;
    data[2] = (new_freq >> 8) & 0xFF;
    data[3] = (new_freq >> 16) & 0xFF;
    data[4] = (new_freq >> 24) & 0xFF;

    memset((char * ) & address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("127.0.0.1"); //make user configurable later
    address.sin_port = htons(udp_port);
    sendto(handle, data, 5, 0, (const struct sockaddr * ) & address, sizeof(struct sockaddr_in));
}