/*-------------------------------------------------------------------------------
 * rtl_sdr_tcp.c
 * rtl_tcp client for the RTL-SDR input: samples arrive on a TCP socket, and tuner commands are sent back on it
 *-----------------------------------------------------------------------------*/

#include "dsd.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

//MSG_NOSIGNAL is missing before macOS 11; SO_NOSIGPIPE in rtltcp_open keeps send from raising SIGPIPE there
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

//rtlsdr_read_async's default block size, in bytes
#define RTLTCP_BUF_LENGTH (16 * 32 * 512)

//rtl_tcp state: socket, tuner type, and a mutex that keeps commands from different threads from interleaving
//NOTE: Keeping these as static global is not great. They really should be in some kind of opaque handle
//      like rtl_tcp_dev_t or something. For applications that only use one device at a time, this is fine.
//      But if we ever move to supporting multiple simultaneous RTL tuners, this will need to change.
static int rtltcp_sock = -1;
static int rtltcp_tuner_type = RTLSDR_TUNER_UNKNOWN; //from the server's greeting
static pthread_mutex_t rtltcp_m = PTHREAD_MUTEX_INITIALIZER;

//rtl_tcp command bytes, each named after the librtlsdr function the server calls with the parameter
enum
{
	RTLTCP_SET_CENTER_FREQ = 0x01,
	RTLTCP_SET_SAMPLE_RATE = 0x02,
	RTLTCP_SET_TUNER_GAIN_MODE = 0x03,
	RTLTCP_SET_TUNER_GAIN = 0x04,
	RTLTCP_SET_FREQ_CORRECTION = 0x05,
	RTLTCP_SET_DIRECT_SAMPLING = 0x09,
	RTLTCP_SET_OFFSET_TUNING = 0x0a
};

//send one rtl_tcp command: a command byte followed by a 32-bit big-endian parameter.
//A partly sent command would misalign every later one, so a failed send ends the connection.
static int rtltcp_command(uint8_t cmd, uint32_t param)
{
	//encode command and parameter to send to rtl_tcp
	unsigned char buf[5] = {cmd, (unsigned char)(param >> 24), (unsigned char)(param >> 16), (unsigned char)(param >> 8), (unsigned char)param};

	size_t sent = 0;
	pthread_mutex_lock(&rtltcp_m);
	while (sent < 5) {
		ssize_t n = send(rtltcp_sock, buf + sent, 5 - sent, MSG_NOSIGNAL);
		if (n < 0 && errno == EINTR) {
			continue;
		}
		if (n <= 0) {
			break;
		}
		sent += n;
	}

	//a failed or partial send ends the connection
	if (sent < 5) {
		shutdown(rtltcp_sock, SHUT_RDWR);
	}
	pthread_mutex_unlock(&rtltcp_m);
	return sent < 5 ? -1 : 0;
}

//connect to an rtl_tcp server over IPv4 or IPv6, read its greeting, and store the socket in rtltcp_sock; prints the outcome and returns 0, or -1 on failure
int rtltcp_open(const char *hostname, int portno)
{
	struct addrinfo hints, *res, *ai;
	char portstr[16];
	unsigned char header[12];
	int sock = -1;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	snprintf(portstr, sizeof(portstr), "%d", portno);
	if (getaddrinfo(hostname, portstr, &hints, &res) != 0) {
		fprintf (stderr, "Failed to resolve rtl_tcp server %s.\n", hostname);
		return -1;
	}

	//try each resolved address in turn until one connects
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		//socket creation
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0) {
			continue;
		}

		//connect
		if (connect(sock, ai->ai_addr, ai->ai_addrlen) == 0) {
			break;
		}

		//close socket and retry with different address
		close(sock);
		sock = -1;
	}

	freeaddrinfo(res);
	if (sock < 0) {
		fprintf (stderr, "Failed to connect to rtl_tcp server %s:%d.\n", hostname, portno);
		return -1;
	}

#ifdef SO_NOSIGPIPE
	//macOS and the BSDs: a send to a closed connection returns an error instead of raising SIGPIPE
	int one = 1;
	setsockopt(sock, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif

	//the greeting is "RTL0", the tuner type, and the number of tuner gain steps
	if (recv(sock, header, 12, MSG_WAITALL) != 12 || memcmp(header, "RTL0", 4) != 0) {
		fprintf (stderr, "%s:%d did not send an rtl_tcp greeting.\n", hostname, portno);
		close(sock);
		return -1;
	}

	//socket is valid and ready to use
	rtltcp_sock = sock;

	//store tuner type for later use in rtltcp_get_tuner_gains
	rtltcp_tuner_type = ((uint32_t)header[4] << 24) | ((uint32_t)header[5] << 16) | ((uint32_t)header[6] << 8) | header[7];
	fprintf (stderr, "Using rtl_tcp server %s:%d. \n", hostname, portno);
	return 0;
}

//rtlsdr_get_tuner_gains for the server's tuner: copies its gain steps, in tenths of a dB, into gains unless gains is NULL, and returns the number of steps
static int rtltcp_get_tuner_gains(int *gains)
{
	//rtl_tcp advertises only the gain count, not the values; librtlsdr's gain query requires a local device.
	//Unfortunately this means we have to copy librtlsdr's tables here to select the nearest valid gain given the remote device.
	//It would be better if rtl_tcp told us the actual list of supported gains, but it doesn't.
	static const int e4k[] = { -10, 15, 40, 65, 90, 115, 140, 165, 190, 215, 240, 290, 340, 420 };
	static const int fc0012[] = { -99, -40, 71, 179, 192 };
	static const int fc0013[] = { -99, -73, -65, -63, -60, -58, -54, 58, 61, 63, 65, 67, 68, 70, 71, 179, 181, 182, 184, 186, 188, 191, 197 };
	static const int r82xx[] = { 0, 9, 14, 27, 37, 77, 87, 125, 144, 157, 166, 197, 207, 229, 254, 280, 297, 328, 338, 364, 372, 386, 402, 421, 434, 439, 445, 480, 496 };
	static const int none[] = { 0 };
	const int *table = none;
	int count = 1;

	//different tuners have different possible gains
	switch (rtltcp_tuner_type) {
	case RTLSDR_TUNER_E4000: table = e4k; count = sizeof(e4k) / sizeof(int); break;
	case RTLSDR_TUNER_FC0012: table = fc0012; count = sizeof(fc0012) / sizeof(int); break;
	case RTLSDR_TUNER_FC0013: table = fc0013; count = sizeof(fc0013) / sizeof(int); break;
	case RTLSDR_TUNER_R820T:
	case RTLSDR_TUNER_R828D: table = r82xx; count = sizeof(r82xx) / sizeof(int); break;
	}

	if (gains != NULL) {
		memcpy(gains, table, count * sizeof(int));
	}
	return count;
}

//close the rtl_tcp connection; returns close's result
static int rtltcp_close(void)
{
	int r = close(rtltcp_sock);
	rtltcp_sock = -1;
	return r;
}

//pass rtl_tcp samples to cb until the connection closes or exitflag is set, in the same block size that rtlsdr_read_async delivers by default
static int rtltcp_read_async(rtlsdr_read_async_cb_t cb, void *ctx)
{
	static unsigned char buf[RTLTCP_BUF_LENGTH];
	while (!exitflag && recv(rtltcp_sock, buf, RTLTCP_BUF_LENGTH, MSG_WAITALL) == RTLTCP_BUF_LENGTH) {
		cb(buf, RTLTCP_BUF_LENGTH, ctx);
	}

	//we are finished (for a variety of reasons). ensure exitflag is set
	if (!exitflag) {
		fprintf (stderr, "rtl_tcp server closed the connection.\n");
		exitflag = 1;
	}
	return 0;
}

//Each rtlsdr_or_tcp_* function takes the arguments of the librtlsdr function it is named after.
//While an rtl_tcp server is connected, the call goes to the server; otherwise it goes to librtlsdr.
int rtlsdr_or_tcp_close(rtlsdr_dev_t *dev)
{
	return rtltcp_sock >= 0 ? rtltcp_close() : rtlsdr_close(dev);
}

int rtlsdr_or_tcp_read_async(rtlsdr_dev_t *dev, rtlsdr_read_async_cb_t cb, void *ctx, uint32_t buf_num, uint32_t buf_len)
{
	return rtltcp_sock >= 0 ? rtltcp_read_async(cb, ctx) : rtlsdr_read_async(dev, cb, ctx, buf_num, buf_len);
}

int rtlsdr_or_tcp_cancel_async(rtlsdr_dev_t *dev)
{
	return rtltcp_sock >= 0 ? shutdown(rtltcp_sock, SHUT_RDWR) : rtlsdr_cancel_async(dev);
}

int rtlsdr_or_tcp_reset_buffer(rtlsdr_dev_t *dev)
{
	return rtltcp_sock >= 0 ? 0 : rtlsdr_reset_buffer(dev);
}

int rtlsdr_or_tcp_get_tuner_gains(rtlsdr_dev_t *dev, int *gains)
{
	return rtltcp_sock >= 0 ? rtltcp_get_tuner_gains(gains) : rtlsdr_get_tuner_gains(dev, gains);
}

int rtlsdr_or_tcp_set_center_freq(rtlsdr_dev_t *dev, uint32_t freq)
{
	return rtltcp_sock >= 0 ? rtltcp_command(RTLTCP_SET_CENTER_FREQ, freq) : rtlsdr_set_center_freq(dev, freq);
}

int rtlsdr_or_tcp_set_sample_rate(rtlsdr_dev_t *dev, uint32_t rate)
{
	return rtltcp_sock >= 0 ? rtltcp_command(RTLTCP_SET_SAMPLE_RATE, rate) : rtlsdr_set_sample_rate(dev, rate);
}

int rtlsdr_or_tcp_set_direct_sampling(rtlsdr_dev_t *dev, int on)
{
	return rtltcp_sock >= 0 ? rtltcp_command(RTLTCP_SET_DIRECT_SAMPLING, on) : rtlsdr_set_direct_sampling(dev, on);
}

int rtlsdr_or_tcp_set_offset_tuning(rtlsdr_dev_t *dev, int on)
{
	return rtltcp_sock >= 0 ? rtltcp_command(RTLTCP_SET_OFFSET_TUNING, on) : rtlsdr_set_offset_tuning(dev, on);
}

int rtlsdr_or_tcp_set_tuner_gain_mode(rtlsdr_dev_t *dev, int manual)
{
	return rtltcp_sock >= 0 ? rtltcp_command(RTLTCP_SET_TUNER_GAIN_MODE, manual) : rtlsdr_set_tuner_gain_mode(dev, manual);
}

int rtlsdr_or_tcp_set_tuner_gain(rtlsdr_dev_t *dev, int gain)
{
	return rtltcp_sock >= 0 ? rtltcp_command(RTLTCP_SET_TUNER_GAIN, gain) : rtlsdr_set_tuner_gain(dev, gain);
}

int rtlsdr_or_tcp_set_freq_correction(rtlsdr_dev_t *dev, int ppm)
{
	return rtltcp_sock >= 0 ? rtltcp_command(RTLTCP_SET_FREQ_CORRECTION, ppm) : rtlsdr_set_freq_correction(dev, ppm);
}
