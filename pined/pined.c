#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include "pinecommon.h"
#include "session.h"
#include "logger.h"
#include "version.h"

#define PINE_KEEP_INTERVAL 300

static char program_string[] = "pined";
static char *log_file = "/var/log/pinedlog";

int g_rs_port = PINE_DEFAULT_PORT;
int g_debug_mode = 0;

static void print_usage(void) {
	fprintf(stderr, "Usage: %s [OPTION]... \n\n", program_string);
	fprintf(stderr, "  -p PORT\tspecify port number to bind\n");
	fprintf(stderr, "  -l LEVEL\tset log level\n");
	fprintf(stderr, "  -d     \texecute in debug mode\n");
	fprintf(stderr, "  -h     \tdisplay this help and exit\n");
	fprintf(stderr, "  -v     \toutput version information and exit\n");
}
static void print_version(void) {
	fprintf(stderr, "%s version %s\n", program_string, version_string);
}

static int create_socket(void) {
	struct sockaddr_in addr;
	int sock;
	int option = 1;
	int ret;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		logger(PINE_LOG_ERROR, "create_socket: socket(): %s", strerror(errno));
		return -1;
	}
	ret = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);
	if (ret < 0) {
		logger(PINE_LOG_ERROR, "create_socket: setsockopt(SO_REUSEADDR): %s", strerror(errno));
		return -1;
	}
	ret = setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &option, sizeof option);
	if (ret < 0) {
		logger(PINE_LOG_ERROR, "create_socket: setsockopt(SO_KEEPALIVE): %s", strerror(errno));
		return -1;
	}
	option = PINE_KEEP_INTERVAL;
	ret = setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &option, sizeof option);
	if (ret < 0) {
		logger(PINE_LOG_ERROR, "create_socket: setsockopt(TCP_KEEPIDLE): %s", strerror(errno));
		return -1;
	}
	memset(&addr, 0, sizeof addr);
	addr.sin_family      = AF_INET;
	addr.sin_port        = htons(g_rs_port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ret = bind(sock, (struct sockaddr *)&addr, sizeof addr);
	if (ret < 0) {
		logger(PINE_LOG_ERROR, "create_socket: bind(): %s", strerror(errno));
		return -1;
	}
	ret = listen(sock, SOMAXCONN);
	if (ret < 0) {
		logger(PINE_LOG_ERROR, "create_socket: listen(): %s", strerror(errno));
		return -1;
	}
	logger(PINE_LOG_DEBUG, "create_socket: socket(%d) started to listen at port %d",  sock, g_rs_port);

	return sock;
}

static int do_pined(int ssock) {
	pthread_t thread;
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof addr;
	int rsock;
	int err;

	for (;;) {
		rsock = accept(ssock, (struct sockaddr *)&addr, &addrlen);
		if (rsock < 0) {
			logger(PINE_LOG_ERROR, "do_pined: accept(): %s", strerror(errno));
			return -1;
		}
		logger(PINE_LOG_INFO, "connection accepted from server: %s", inet_ntoa(addr.sin_addr));
		err = pthread_create(&thread, NULL, start_session, &rsock);
		if (err != 0) {
			logger(PINE_LOG_ERROR, "do_pined: pthread_create(): %s", strerror(err));
			return -1;
		}
		err = pthread_detach(thread);
		if (err != 0) {
			logger(PINE_LOG_ERROR, "do_pined: pthread_detach(): %s", strerror(err));	
			return -1;
		}
	}
	return 0;
}

int main(int argc, char *argv[]) {
	int level;
	int opt;
	int sock;

	while ((opt = getopt(argc, argv, "vhdp:l:")) != -1) {
		switch (opt) {
		case 'v':
			print_version();
			return 0;
		case 'h':
			print_usage();
			return 0;
		case 'd':
			g_debug_mode = !0;
			log_file = NULL;
			break;
		case 'p':
			g_rs_port = atoi(optarg);
			break;
		case 'l':
			level = atoi(optarg);
			if (level < 0 || level >= PINE_LOG_MAX) {
				exit(EXIT_FAILURE);
			}
			log_setlevel(level);
			break;
		default: /* ? */
			print_usage();
			exit(EXIT_FAILURE);
		}
	}

	if (log_init(log_file) < 0) {
		exit(EXIT_FAILURE);
	}
	if (!g_debug_mode) {
		if (daemon(0, 0) < 0) {
			perror("daemon()");
			exit(EXIT_FAILURE);
		}
	}

	logger(PINE_LOG_INFO, "start pined");

	sock = create_socket();
	if (sock < 0) {
		exit(EXIT_FAILURE);
	}

	signal(SIGPIPE, SIG_IGN);

	return do_pined(sock);
}

