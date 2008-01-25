#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <rssock.h>

#define PORT 10007

#define DEFAULT_RSPORT 8888
#define DEFAULT_RSHOST "localhost"

int  rsport = DEFAULT_RSPORT;
char *rshost = DEFAULT_RSHOST;


static in_addr_t getaddrbyname(char *hostname) {
	struct hostent *host;

	host = gethostbyname(hostname);
	if (host == NULL) {
		perror("gethostbyname()");
		exit(EXIT_FAILURE);
	}
	return *(in_addr_t *)host->h_addr_list[0];
}
static void *echo(void *arg) {
	char buf[BUFSIZ];
	char *p;
	int so = *(int *)arg;

	for (;;) {
		int remain;
		int len;

		remain = read(so, buf, sizeof buf);
		if (remain <= 0) {
			break;
		}
		p = buf;
		do {
			len = write(so, p, remain);
			if (len < 0) {
				goto last;
			}
			p += len;
			remain -= len;
		} while (remain > 0);
	}
last:
	close(so);
	pthread_exit(NULL);
}

int main(int argc, char *argv[]) {
	int so;
	int ret;
	struct sockaddr_in rsaddr;

	signal(SIGPIPE, SIG_IGN);
	so = socket(AF_INET, SOCK_STREAM, 0);
	if (so < 0) {
		perror("socket()");
		exit(EXIT_FAILURE);
	}

	memset(&rsaddr, 0, sizeof rsaddr);
	rsaddr.sin_family = AF_INET;
	rsaddr.sin_port   = htons(rsport);
	rsaddr.sin_addr.s_addr   = getaddrbyname(rshost);
	ret = rs_bind(so, PORT, (struct sockaddr *)&rsaddr, sizeof rsaddr);
	if (ret < 0) {
		exit(EXIT_FAILURE);
	}

	ret = rs_listen(so, SOMAXCONN);
	if (ret < 0) {
		exit(EXIT_FAILURE);
	}

	for (;;) {
		pthread_t thread;
		fd_set rfds;
		int cso;

		FD_ZERO(&rfds);
		FD_SET(so, &rfds);
		ret = select(so+1, &rfds, NULL, NULL, NULL);
		if (ret < 0) {
			exit(EXIT_FAILURE);
		}

		cso = rs_accept(so, NULL, NULL);
		if (cso < 0) {
			exit(EXIT_FAILURE);
		}
		ret = pthread_create(&thread, NULL, echo, &cso);
		if (ret < 0) {
			exit(EXIT_FAILURE);
		}
		pthread_detach(thread);
	}

	return 0;
}

