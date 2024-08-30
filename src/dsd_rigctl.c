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

struct sockaddr_in address;
struct sockaddr_in addressA;
struct sockaddr_in addressM17;

//
// Connect
//
int Connect (char *hostname, int portno)
{
    int sockfd;
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

    if (strcmp(buf, "RPRT 1\n") == 0 ) //sdr++ has a linebreak here, is that in all versions of the protocol?
        return false;

    return true;
}

bool SetModulation(int sockfd, int bandwidth) 
{
    char buf[BUFSIZE];
    //the bandwidth is now a user/system based configurable variable
    sprintf (buf, "M NFM %d\n", bandwidth); //SDR++ has changed the token from FM to NFM, even if Ryzerth fixes it later, users may still have an older version
    Send(sockfd, buf);
    Recv(sockfd, buf);

    //if it fails the first time, send the other token instead
    if (strcmp(buf, "RPRT 1\n") == 0 ) //sdr++ has a linebreak here, is that in all versions of the protocol?
    {
        sprintf (buf, "M FM %d\n", bandwidth); //anything not SDR++
        Send(sockfd, buf);
        Recv(sockfd, buf);
    }

    if (strcmp(buf, "RPRT 1\n") == 0 )
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
    UNUSED(hostname);

    int sockfd;
    struct sockaddr_in serveraddr;

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

    //set these for non blocking when no samples to read
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 10;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);

    return sockfd;
}

//going to leave this function available, even if completely switched over to rtl_dev_tune now, may be useful in the future
void rtl_udp_tune(dsd_opts * opts, dsd_state * state, long int frequency) 
{
    UNUSED(state);

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

    close (handle); //close socket after sending.
}

void udp_socket_blaster(dsd_opts * opts, dsd_state * state, size_t nsam, void * data)
{
    UNUSED(state);
    size_t err = 0;

    //listen with:

    //short 8k/2
    //socat stdio udp-listen:23456 | play --buffer 640 -q -b 16 -r 8000 -c2 -t s16 -

    //short 8k/1
    //socat stdio udp-listen:23456 | play --buffer 320 -q -b 16 -r 8000 -c1 -t s16 -

    //float 8k/2
    //socat stdio udp-listen:23456 | play --buffer 1280 -q -e float -b 32 -r 8000 -c2 -t f32 -

    //float 8k/1
    //socat stdio udp-listen:23456 | play --buffer 640 -q -e float -b 32 -r 8000 -c1 -t f32 -

    //send audio or data to socket
    err = sendto(opts->udp_sockfd, data, nsam, 0, (const struct sockaddr * ) & address, sizeof(struct sockaddr_in));
    if (err < 0) fprintf (stderr, "\n UDP SENDTO ERR %ld", err); //return value here is size_t number of characters sent, or -1 for failure
    if (err < nsam) fprintf (stderr, "\n UDP Underflow %ld", err); //I'm not even sure if this is possible
}

int m17_socket_receiver(dsd_opts * opts, void * data)
{
    size_t err = 0;
    struct sockaddr_in cliaddr; 
    socklen_t len = sizeof(cliaddr); 

    //receive data from socket
    err = recvfrom(opts->udp_sockfd, data, 1000, 0, (struct sockaddr * ) & address, &len); //was MSG_WAITALL, but that seems to be = 256

    return err;
}

//Analog UDP port on +2 of normal open socket
void udp_socket_blasterA(dsd_opts * opts, dsd_state * state, size_t nsam, void * data)
{
    UNUSED(state);
    size_t err = 0;

    //listen with:

    //short 48k/1
    //socat stdio udp-listen:23456 | play --buffer 1920 -q -b 16 -r 48000 -c1 -t s16 -

    //send audio or data to socket
    err = sendto(opts->udp_sockfdA, data, nsam, 0, (const struct sockaddr * ) & addressA, sizeof(struct sockaddr_in));
    if (err < 0) fprintf (stderr, "\n UDP SENDTO ERR %ld", err); //return value here is size_t number of characters sent, or -1 for failure
    if (err < nsam) fprintf (stderr, "\n UDP Underflow %ld", err); //I'm not even sure if this is possible
}

int m17_socket_blaster(dsd_opts * opts, dsd_state * state, size_t nsam, void * data)
{
    UNUSED(state);
    unsigned long long int err = 0;

    //See notes in m17.c on line ~3395 regarding usage

    //send audio or data to socket
    err = sendto(opts->m17_udp_sock, data, nsam, 0, (const struct sockaddr * ) & addressM17, sizeof(struct sockaddr_in));
    //RETURN Value should be ACKN or NACK, or PING, or PONG

    return (err);
}

int udp_socket_connect(dsd_opts * opts, dsd_state * state)
{
    UNUSED(state);

    long int err = 0;
    err = opts->udp_sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (err < 0)
    {
        fprintf (stderr, " UDP Socket Error %ld\n", err);
        return (err);
    }

    // Don't think this is needed, but doesn't seem to hurt to keep it here either
    int broadcastEnable = 1;
    err = setsockopt(opts->udp_sockfd, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    if (err < 0)
    {
        fprintf (stderr, " UDP Broadcast Set Error %ld\n", err);
        return (err);
    }

    memset((char * ) & address, 0, sizeof(address));
    address.sin_family = AF_INET;
    err = address.sin_addr.s_addr = inet_addr(opts->udp_hostname);
    if (err < 0) //error in this context reports back 32-bit inet_addr reversed order byte pairs
    {
        fprintf (stderr, " UDP inet_addr Error %ld\n", err);
        return (err);
    }

    address.sin_port = htons(opts->udp_portno);
    if (err < 0)
    {
        fprintf (stderr, " UDP htons Error %ld\n", err);
        return (err);
    }

    return 0;
}

int udp_socket_connectA(dsd_opts * opts, dsd_state * state)
{
    UNUSED(state);

    long int err = 0;
    err = opts->udp_sockfdA = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (err < 0)
    {
        fprintf (stderr, " UDP Socket Error %ld\n", err);
        return (err);
    }

    // Don't think this is needed, but doesn't seem to hurt to keep it here either
    int broadcastEnable = 1;
    err = setsockopt(opts->udp_sockfdA, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    if (err < 0)
    {
        fprintf (stderr, " UDP Broadcast Set Error %ld\n", err);
        return (err);
    }

    memset((char * ) & addressA, 0, sizeof(addressA));
    addressA.sin_family = AF_INET;
    err = addressA.sin_addr.s_addr = inet_addr(opts->udp_hostname);
    if (err < 0) //error in this context reports back 32-bit inet_addr reversed order byte pairs
    {
        fprintf (stderr, " UDP inet_addr Error %ld\n", err);
        return (err);
    }

    addressA.sin_port = htons(opts->udp_portno+2); //plus 2 to current port assignment for the analog port value
    if (err < 0)
    {
        fprintf (stderr, " UDP htons Error %ld\n", err);
        return (err);
    }

    return 0;
}

int udp_socket_connectM17(dsd_opts * opts, dsd_state * state)
{
    UNUSED(state);

    long int err = 0;
    err = opts->m17_udp_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (err < 0)
    {
        fprintf (stderr, " UDP Socket Error %ld\n", err);
        return (err);
    }

    // Don't think this is needed, but doesn't seem to hurt to keep it here either
    int broadcastEnable = 1;
    err = setsockopt(opts->m17_udp_sock, SOL_SOCKET, SO_BROADCAST, &broadcastEnable, sizeof(broadcastEnable));
    if (err < 0)
    {
        fprintf (stderr, " UDP Broadcast Set Error %ld\n", err);
        return (err);
    }

    memset((char * ) & addressM17, 0, sizeof(addressM17));
    addressM17.sin_family = AF_INET;
    err = addressM17.sin_addr.s_addr = inet_addr(opts->m17_hostname);
    if (err < 0) //error in this context reports back 32-bit inet_addr reversed order byte pairs
    {
        fprintf (stderr, " UDP inet_addr Error %ld\n", err);
        return (err);
    }

    addressM17.sin_port = htons(opts->m17_portno);
    if (err < 0)
    {
        fprintf (stderr, " UDP htons Error %ld\n", err);
        return (err);
    }

    return 0;
}

void return_to_cc (dsd_opts * opts, dsd_state * state)
{
    //extra safeguards due to sync issues with NXDN
    memset (state->nxdn_sacch_frame_segment, 1, sizeof(state->nxdn_sacch_frame_segment));
    memset (state->nxdn_sacch_frame_segcrc, 1, sizeof(state->nxdn_sacch_frame_segcrc));

    memset (state->active_channel, 0, sizeof(state->active_channel));

    //reset dmr blocks
    dmr_reset_blocks (opts, state);

    //zero out additional items
    state->lasttg = 0;
    state->lasttgR = 0;
    state->lastsrc = 0;
    state->lastsrcR = 0;
    state->payload_algid = 0;
    state->payload_algidR = 0;
    state->payload_keyid = 0;
    state->payload_keyidR = 0;
    state->payload_mi = 0;
    state->payload_miR = 0;
    state->payload_miP = 0;
    state->payload_miN = 0;
    opts->p25_is_tuned = 0;
    state->p25_vc_freq[0] = state->p25_vc_freq[1] = 0;

    //tune back to the control channel -- NOTE: Doesn't work correctly on EDACS Analog Voice
    //RIGCTL
    if (opts->p25_trunk == 1 && opts->use_rigctl == 1)
    {
        if (opts->setmod_bw != 0 )  SetModulation(opts->rigctl_sockfd, opts->setmod_bw);
        SetFreq(opts->rigctl_sockfd, state->p25_cc_freq);
    }

    //rtl
    #ifdef USE_RTLSDR
    if (opts->p25_trunk == 1 && opts->audio_in_type == 3) rtl_dev_tune (opts, state->p25_cc_freq);
    #endif

    state->last_cc_sync_time = time(NULL);

    //if P25p2 VCH and going back to P25p1 CC, flip symbolrate
    if (state->p25_cc_is_tdma == 0)
    {
        state->samplesPerSymbol = 10;
        state->symbolCenter = 4;
    }

    //if P25p1 Data Revert on P25p2 TDMA CC, flip symbolrate
    if (state->p25_cc_is_tdma == 1)
    {
        state->samplesPerSymbol = 8;
        state->symbolCenter = 3;
    }

    // fprintf (stderr, "\n User Activated Return to CC; \n ");
}
