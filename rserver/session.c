#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/param.h>
#include <netinet/in.h>
#include "rscommon.h"
#include "session.h"
#include "logger.h"

#define RSRV_STAT_INIT		0
#define RSRV_STAT_BOUND		1
#define RSRV_STAT_LISTENING	2
#define RSRV_STAT_ACCEPTING	3
#define RSRV_STAT_CONNECTING	4

typedef struct stbl_ent {
	int stat;	
	int sock;
	struct sockaddr	addr;
	socklen_t	alen;
} stbl_ent_t;

static stbl_ent_t session_table[FD_SETSIZE];

static void close_session(int so) {
	stbl_ent_t *ent = &session_table[so];

	if (ent->sock != 0) {
		close(ent->sock);
	}
	memset(ent, 0, sizeof *ent);
}

static int send_mesg(int so, int type, int value) {
	rsrv_mesg_t msg;
	int ret;

	msg.rm_typ = htonl(type);
	msg.rm_val = htonl(value);

	ret = write(so, &msg, sizeof msg);
	if (ret < 0) {
		logger(RSRV_LOG_ERROR, "send_mesg: write(): %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

static void *transfer(void *arg) {
	int from = ((int *)arg)[0];
	int to   = ((int *)arg)[1];
	char buf[BUFSIZ];
	int total, remain, len;
	int offset;

	logger(RSRV_LOG_DEBUG, "transfer(%d,%d): started\n", from, to);

	for (;;) {
		total = read(from, buf, sizeof buf);
		if (total < 0) {
			logger(RSRV_LOG_DEBUG, "transfer(%d,%d): read(): %s\n", from, to, strerror(errno));
			break;
		} else if (total == 0) {
			logger(RSRV_LOG_DEBUG, "transfer(%d,%d): connection closed by peer\n", from, to);
			break;
		}

		remain = total;
		offset = 0;
		do {
			len = write(to, buf + offset, remain);
			if (len < 0) {
				logger(RSRV_LOG_DEBUG, "transfer(%d,%d): write(): %s\n", from, to, strerror(errno));
				goto last;
			}
			remain -= len;
		} while (remain > 0);
	}

last:
	logger(RSRV_LOG_DEBUG, "transfer(%d,%d): finished\n", from, to);
	shutdown(to, SHUT_WR);
	return NULL;
}

static int do_bind(int lcsock, struct sockaddr *addr, socklen_t addrlen) {
	int rssock;
	int option;
	int ret;

	rssock = socket(AF_INET, SOCK_STREAM, 0);
	if (rssock < 0) {
		logger(RSRV_LOG_ERROR, "do_bind(%d): socket(): %s\n", lcsock, strerror(errno));
		send_mesg(lcsock, RSRV_NAK, errno);
		return -1;
	}
	logger(RSRV_LOG_DEBUG, "do_bind(%d): create socket for reception: %d\n", lcsock, rssock);
	option = 1;
	ret = setsockopt(rssock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);
	if (ret < 0) {
		logger(RSRV_LOG_ERROR, "do_bind(%d): setsockopt(): %s\n", lcsock, strerror(errno));
		send_mesg(lcsock, RSRV_NAK, errno);
		close(rssock);
		return -1;
	}

	ret = bind(rssock, addr, addrlen);
	if (ret < 0) {
		logger(RSRV_LOG_ERROR, "do_bind(%d): bind(): %s\n", lcsock, strerror(errno));
		send_mesg(lcsock, RSRV_NAK, errno);
		close(rssock);
		return -1;
	}
	session_table[lcsock].stat = RSRV_STAT_BOUND;
	session_table[lcsock].sock = rssock;

	return send_mesg(lcsock, RSRV_ACK_BIND, rssock);
}

static int do_listen(int lcsock, int backlog) {
	int stat = session_table[lcsock].stat;
	int rssock; 
	int ret;

	logger(RSRV_LOG_DEBUG, "do_listen(%d): started (backlog=%d)\n", lcsock, backlog);

	if (stat != RSRV_STAT_BOUND) {
		logger(RSRV_LOG_ERROR, "do_listen(%d): unexpected session status %d\n", lcsock, stat);
		send_mesg(lcsock, RSRV_NAK, -1);
		return -1;
	}

	rssock = session_table[lcsock].sock;
	ret = listen(rssock, backlog);
	if (ret < 0) {
		logger(RSRV_LOG_ERROR, "do_listen(%d): listen(): %s\n", lcsock, strerror(errno));
		send_mesg(lcsock, RSRV_NAK, errno);
		return -1;
	}
	session_table[lcsock].stat = RSRV_STAT_LISTENING;

	return send_mesg(lcsock, RSRV_ACK_LISTEN, 0);
}

static int do_accept(int lcsock) {
	struct sockaddr addr;
	socklen_t addrlen;
	fd_set rfds;
	int n;
	int stat = session_table[lcsock].stat;
	int rssock = session_table[lcsock].sock;
	int rcsock;
	int ret;

	logger(RSRV_LOG_DEBUG, "do_accept(%d): started\n", lcsock);

	if (stat != RSRV_STAT_LISTENING) {
		logger(RSRV_LOG_ERROR, "do_accept(%d): unexpected session status %d\n", lcsock, stat);
		send_mesg(lcsock, RSRV_NAK, -1);
		return -1;
	}

	FD_ZERO(&rfds);
	FD_SET(lcsock, &rfds);
	FD_SET(rssock, &rfds);
	n = MAX(lcsock, rssock) + 1;
	ret = select(n, &rfds, NULL, NULL, NULL);
	if (ret < 0) {
		logger(RSRV_LOG_ERROR, "do_accept(%d): select(): %s\n", lcsock, strerror(errno));
		send_mesg(lcsock, RSRV_NAK, -1);
		return -1;
	}
	if (FD_ISSET(lcsock, &rfds)) {
		logger(RSRV_LOG_INFO, "do_accept(%d): cancelled by remote server\n", lcsock);
		send_mesg(lcsock, RSRV_NAK, -1);
		return -1;
	}

	rcsock = accept(rssock, &addr, &addrlen);
	if (rcsock < 0) {
		logger(RSRV_LOG_ERROR, "do_accept(%d): accept(): %s\n", lcsock,  strerror(errno));
		send_mesg(lcsock, RSRV_NAK, errno);
		return -1;
	}

	logger(RSRV_LOG_DEBUG, "do_accept(%d): accepted for remote peer: %d\n", lcsock, rcsock);

	if (send_mesg(lcsock, RSRV_ACK_ACCEPT, rcsock) < 0) {
		logger(RSRV_LOG_ERROR, "do_accept(%d): failure to send reply message\n", lcsock);
		close(rcsock);
		return -1;
	}
	session_table[rcsock].stat = RSRV_STAT_ACCEPTING;
	session_table[rcsock].addr = addr;
	session_table[rcsock].alen = addrlen;

	return 0;
}

static int do_connect(int lcsock, int rcsock) {
	rsrv_mesg_t msg;
	struct iovec iov[2];
	int iovcnt;
	pthread_t thr_l2r, thr_r2l;
	int l2r[2], r2l[2];
	int stat;
	int ret;

	logger(RSRV_LOG_DEBUG, "do_connect(%d,%d): started\n", lcsock, rcsock);

	stat = session_table[lcsock].stat;
	if (stat != RSRV_STAT_INIT) {
		logger(RSRV_LOG_ERROR, "do_connect(%d,%d): session status is not initialized %d\n", lcsock, rcsock, stat);
		send_mesg(lcsock, RSRV_NAK, -1);
		ret = -1;
		goto last;
	}
	session_table[lcsock].stat = RSRV_STAT_ACCEPTING;

	stat = session_table[rcsock].stat;
	if (stat != RSRV_STAT_ACCEPTING) {
		logger(RSRV_LOG_ERROR, "do_connect(%d,%d): session status is not accepting %d\n", lcsock, rcsock, stat);
		send_mesg(lcsock, RSRV_NAK, -1);
		ret = -1;
		goto last;
	}
	session_table[lcsock].sock = rcsock;

	msg.rm_typ = htonl(RSRV_ACK_CONNECT);
	msg.rm_val = htonl(session_table[rcsock].alen);
	iov[0].iov_base = &msg;
	iov[0].iov_len	= sizeof msg;
	iov[1].iov_base = &session_table[rcsock].addr;
	iov[1].iov_len	= session_table[rcsock].alen;
	iovcnt = ARRAY_COUNT(iov);
	ret = writev(lcsock, iov, iovcnt);
	if (ret < 0) {
		logger(RSRV_LOG_ERROR, "do_connect(%d,%d): writev(): %s\n", lcsock, rcsock, strerror(errno));
		goto last;
	}

	l2r[0] = r2l[1] = lcsock;
	l2r[1] = r2l[0] = rcsock;
	ret = pthread_create(&thr_l2r, NULL, transfer, (void *)l2r);
	if (ret != 0) {
		logger(RSRV_LOG_ERROR, "do_connect(%d,%d): pthread_create(thr_l2r): %s\n",lcsock, rcsock, strerror(ret));
		ret = -1;
		goto last;
	}
	ret = pthread_create(&thr_r2l, NULL, transfer, (void *)r2l);
	if (ret != 0) {
		logger(RSRV_LOG_ERROR, "do_connect(%d,%d): pthread_create(thr_r2l): %s\n",lcsock, rcsock, strerror(ret));
		ret = -1;
		goto last;
	}

	ret = pthread_join(thr_l2r, NULL);
	if (ret != 0) {
		logger(RSRV_LOG_ERROR, "do_connect(%d,%d): pthread_join(l2r): %s\n",lcsock, rcsock, strerror(ret));
		goto last;
	}
	ret = pthread_join(thr_r2l, NULL);
	if (ret != 0) {
		logger(RSRV_LOG_ERROR, "do_connect(%d,%d): pthread_join(r2l): %s\n",lcsock, rcsock, strerror(ret));
		ret = -1;
		goto last;
	}

	ret = 0;
last:
	logger(RSRV_LOG_DEBUG, "do_connect(%d,%d): finished(%d)\n", lcsock, rcsock, ret);

	return ret;
}

static int do_select(int lcsock, fd_set *rfds) {
	rsrv_mesg_t msg;
	struct iovec iov[2];
	int iovcnt;
	int stat;
	int ret;

	logger(RSRV_LOG_DEBUG, "do_select(%d): started\n", lcsock);

	stat = session_table[lcsock].stat;
	if (stat != RSRV_STAT_INIT) {
		logger(RSRV_LOG_ERROR, "do_select(%d): session status is not initialized %d\n", lcsock, stat);
		send_mesg(lcsock, RSRV_NAK, -1);
		ret = -1;
		goto last;
	}

	ret = select(FD_SETSIZE, rfds, NULL, NULL, NULL);
	if (ret < 0) {
		logger(RSRV_LOG_ERROR, "do_select(%d): %s\n", lcsock, strerror(errno));	
		send_mesg(lcsock, RSRV_NAK, errno);
		goto last;
	}

	msg.rm_typ = htonl(RSRV_ACK_SELECT);
	msg.rm_val = htonl(sizeof(fd_set));
	iov[0].iov_base = &msg;
	iov[0].iov_len	= sizeof msg;
	iov[1].iov_base = rfds;
	iov[1].iov_len	= sizeof(fd_set);
	iovcnt = ARRAY_COUNT(iov);
	ret = writev(lcsock, iov, iovcnt);
	if (ret < 0) {
		logger(RSRV_LOG_ERROR, "do_select(%d): writev(): %s\n", lcsock, strerror(errno));
		goto last;
	}

last:
	return ret;
}

void *start_session(void *arg) {
	int lcsock = *(int *)arg;
	rsrv_mesg_t msg;
	struct iovec iov[2];
	int iovcnt;
	char buf[BUFSIZ];
	int req;
	int ret;
	logger(RSRV_LOG_DEBUG, "start_session(%d): thread started\n", lcsock);

	session_table[lcsock].stat = RSRV_STAT_INIT;
	session_table[lcsock].sock = 0;

	iov[0].iov_base = &msg;
	iov[0].iov_len	= sizeof msg;
	iov[1].iov_base = buf;
	iov[1].iov_len	= sizeof buf;
	iovcnt = ARRAY_COUNT(iov);

	for (;;) {
		ret = readv(lcsock, iov, iovcnt);
		if (ret < 0) {
			logger(RSRV_LOG_ERROR, "start_session(%d): readv(): %s\n", lcsock, strerror(errno));
			goto last;
		} else if (ret == 0) {
			logger(RSRV_LOG_DEBUG, "start_session(%d): connection closed by remote peer\n", lcsock);
			goto last;
		}
		
		req = ntohl(msg.rm_typ);
		switch (req) {
		case RSRV_REQ_BIND:
			do_bind(lcsock, (struct sockaddr *)buf, ntohl(msg.rm_val));
			break;
		case RSRV_REQ_LISTEN:
			do_listen(lcsock, ntohl(msg.rm_val));
			break;
		case RSRV_REQ_ACCEPT:
			do_accept(lcsock);
			break;
		case RSRV_REQ_CONNECT:
			do_connect(lcsock, ntohl(msg.rm_val));
			goto last;
		case RSRV_REQ_SELECT:
			do_select(lcsock, (fd_set *)buf); 
			goto last;
		default:
			logger(RSRV_LOG_ERROR, "start_session(%d): unexpected request received: %d\n", lcsock, req);
			break;;
		}
	}

last:
	close(lcsock);
	close_session(lcsock);

	logger(RSRV_LOG_DEBUG, "start_session(%d): finished\n", lcsock);

	
	return NULL;
}
