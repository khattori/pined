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

#include "pinecommon.h"
#include "session.h"
#include "logger.h"

#define PINE_STAT_INIT		0
#define PINE_STAT_BOUND		1
#define PINE_STAT_LISTENING	2
#define PINE_STAT_ACCEPTING	3
#define PINE_STAT_CONNECTING	4
#define PINE_STAT_ACCEPTABLE    5

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
    pine_mesg_t msg;
    int ret;

    msg.pm_typ = htonl(type);
    msg.pm_val = htonl(value);

    ret = write(so, &msg, sizeof msg);
    if (ret < 0) {
	logger(PINE_LOG_ERROR, "send_mesg: write(): %s", strerror(errno));
	return -1;
    }

    return 0;
}

static int wait_acceptable(int lcsock, int rssock) {
    fd_set rfds, efds;
    int n;
    int ret;

    logger(PINE_LOG_DEBUG, "wait_acceptable(%d,%d) started", lcsock, rssock);

    FD_ZERO(&rfds);
    FD_SET(lcsock, &rfds);
    FD_SET(rssock, &rfds);
    FD_ZERO(&efds);
    FD_SET(lcsock, &efds);
    n = MAX(lcsock, rssock) + 1;
    ret = select(n, &rfds, NULL, &efds, NULL);
    if (ret < 0) {
	logger(PINE_LOG_ERROR, "wait_acceptable(%d,%d): select(): %s", lcsock, rssock, strerror(errno));
	send_mesg(lcsock, PINE_NAK, -1);
	return -1;
    }
    if (FD_ISSET(rssock, &rfds)) {
	session_table[rssock].stat = PINE_STAT_ACCEPTABLE;
	send_mesg(lcsock, PINE_ACK, 0);
        logger(PINE_LOG_DEBUG, "wait_acceptable(%d,%d): become acceptabled", lcsock, rssock);
    }
    if (FD_ISSET(lcsock, &efds)) {
        logger(PINE_LOG_ERROR, "wait_acceptable(%d,%d): %d become error", lcsock);
	return -1;
    }

    logger(PINE_LOG_DEBUG, "wait_acceptable(%d,%d) completed", lcsock, rssock);

    return 0;
}

static void *transfer(void *arg) {
    int from = ((int *)arg)[0];
    int to   = ((int *)arg)[1];
    char buf[BUFSIZ];
    int total, remain, len;
    int offset;
  
    logger(PINE_LOG_DEBUG, "transfer(%d,%d): started", from, to);

    for (;;) {
	total = read(from, buf, sizeof buf);
	if (total < 0) {
	    logger(PINE_LOG_DEBUG, "transfer(%d,%d): read(): %s", from, to, strerror(errno));
	    break;
	} else if (total == 0) {
	    logger(PINE_LOG_DEBUG, "transfer(%d,%d): connection closed by peer", from, to);
	    break;
	}

	remain = total;
	offset = 0;
	do {
	    len = write(to, buf + offset, remain);
	    if (len < 0) {
		logger(PINE_LOG_DEBUG, "transfer(%d,%d): write(): %s", from, to, strerror(errno));
		goto last;
	    }
	    remain -= len;
	} while (remain > 0);
    }

 last:
    logger(PINE_LOG_DEBUG, "transfer(%d,%d): finished", from, to);
    shutdown(to, SHUT_WR);
    return NULL;
}

static int do_bind(int lcsock, int port) {
    struct sockaddr_in addr;
    int rssock;
    int option;
    int ret;

    rssock = socket(AF_INET, SOCK_STREAM, 0);
    if (rssock < 0) {
	logger(PINE_LOG_ERROR, "do_bind(%d): socket(): %s", lcsock, strerror(errno));
	send_mesg(lcsock, PINE_NAK, errno);
	return -1;
    }
    logger(PINE_LOG_DEBUG, "do_bind(%d): create socket for reception: %d for port %d", lcsock, rssock, port);
    option = 1;
    ret = setsockopt(rssock, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);
    if (ret < 0) {
	logger(PINE_LOG_ERROR, "do_bind(%d): setsockopt(SO_REUSEADDR): %s", lcsock, strerror(errno));
	send_mesg(lcsock, PINE_NAK, errno);
	close(rssock);
	return -1;
    }
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(rssock, (struct sockaddr *)&addr, sizeof addr);
    if (ret < 0) {
	logger(PINE_LOG_ERROR, "do_bind(%d): bind(): %s", lcsock, strerror(errno));
	send_mesg(lcsock, PINE_NAK, errno);
	close(rssock);
	return -1;
    }
    session_table[lcsock].stat = PINE_STAT_BOUND;
    session_table[lcsock].sock = rssock;
    session_table[rssock].stat = PINE_STAT_INIT;
    return send_mesg(lcsock, PINE_ACK_BIND, rssock);
}

static int do_listen(int lcsock, int backlog) {
    int stat = session_table[lcsock].stat;
    int rssock; 
    int ret;

    logger(PINE_LOG_DEBUG, "do_listen(%d): started (backlog=%d)", lcsock, backlog);

    if (stat != PINE_STAT_BOUND) {
	logger(PINE_LOG_ERROR, "do_listen(%d): unexpected session status %d", lcsock, stat);
	send_mesg(lcsock, PINE_NAK, -1);
	return -1;
    }

    rssock = session_table[lcsock].sock;
    ret = listen(rssock, backlog);
    if (ret < 0) {
	logger(PINE_LOG_ERROR, "do_listen(%d): listen(): %s", lcsock, strerror(errno));
	send_mesg(lcsock, PINE_NAK, errno);
	return -1;
    }
    session_table[lcsock].stat = PINE_STAT_LISTENING;
    send_mesg(lcsock, PINE_ACK_LISTEN, 0);

    return wait_acceptable(lcsock, rssock);
}

static int do_accept(int lcsock) {
    struct sockaddr addr;
    socklen_t addrlen;
    int stat = session_table[lcsock].stat;
    int rssock = session_table[lcsock].sock;
    int rcsock;

    logger(PINE_LOG_DEBUG, "do_accept(%d): started", lcsock);

    if (stat != PINE_STAT_LISTENING) {
	logger(PINE_LOG_ERROR, "do_accept(%d): unexpected session status %d", lcsock, stat);
	send_mesg(lcsock, PINE_NAK, -1);
	return -1;
    }

    if (session_table[rssock].stat != PINE_STAT_ACCEPTABLE &&
	wait_acceptable(lcsock, rssock) < 0) {
	return -1;
    }
    if (session_table[rssock].stat != PINE_STAT_ACCEPTABLE) {
	return 0;
    }

    rcsock = accept(rssock, &addr, &addrlen);
    if (rcsock < 0) {
	logger(PINE_LOG_ERROR, "do_accept(%d): accept(): %s", lcsock,  strerror(errno));
	send_mesg(lcsock, PINE_NAK, errno);
	return -1;
    }
    session_table[rssock].stat = PINE_STAT_LISTENING;
    logger(PINE_LOG_DEBUG, "do_accept(%d): accepted for remote peer: %d", lcsock, rcsock);

    if (send_mesg(lcsock, PINE_ACK_ACCEPT, rcsock) < 0) {
	logger(PINE_LOG_ERROR, "do_accept(%d): failure to send reply message", lcsock);
	close(rcsock);
	return -1;
    }
    session_table[rcsock].stat = PINE_STAT_ACCEPTING;
    session_table[rcsock].addr = addr;
    session_table[rcsock].alen = addrlen;

    return wait_acceptable(lcsock, rssock);
}

static int do_connect(int lcsock, int rcsock) {
    pine_mesg_t msg;
    struct iovec iov[2];
    int iovcnt;
    pthread_t thr_l2r, thr_r2l;
    int l2r[2], r2l[2];
    int stat;
    int ret;

    logger(PINE_LOG_DEBUG, "do_connect(%d,%d): started", lcsock, rcsock);

    stat = session_table[lcsock].stat;
    if (stat != PINE_STAT_INIT) {
	logger(PINE_LOG_ERROR, "do_connect(%d,%d): session status is not initialized %d", lcsock, rcsock, stat);
	send_mesg(lcsock, PINE_NAK, -1);
	ret = -1;
	goto last;
    }
    session_table[lcsock].stat = PINE_STAT_ACCEPTING;

    stat = session_table[rcsock].stat;
    if (stat != PINE_STAT_ACCEPTING) {
	logger(PINE_LOG_ERROR, "do_connect(%d,%d): session status is not accepting %d", lcsock, rcsock, stat);
	send_mesg(lcsock, PINE_NAK, -1);
	ret = -1;
	goto last;
    }
    session_table[lcsock].sock = rcsock;

    msg.pm_typ = htonl(PINE_ACK_CONNECT);
    msg.pm_val = htonl(session_table[rcsock].alen);
    iov[0].iov_base = &msg;
    iov[0].iov_len  = sizeof msg;
    iov[1].iov_base = &session_table[rcsock].addr;
    iov[1].iov_len  = session_table[rcsock].alen;
    iovcnt = ARRAY_COUNT(iov);
    ret = writev(lcsock, iov, iovcnt);
    if (ret < 0) {
	logger(PINE_LOG_ERROR, "do_connect(%d,%d): writev(): %s", lcsock, rcsock, strerror(errno));
	goto last;
    }

    l2r[0] = r2l[1] = lcsock;
    l2r[1] = r2l[0] = rcsock;
    ret = pthread_create(&thr_l2r, NULL, transfer, (void *)l2r);
    if (ret != 0) {
	logger(PINE_LOG_ERROR, "do_connect(%d,%d): pthread_create(thr_l2r): %s",lcsock, rcsock, strerror(ret));
	ret = -1;
	goto last;
    }
    ret = pthread_create(&thr_r2l, NULL, transfer, (void *)r2l);
    if (ret != 0) {
	logger(PINE_LOG_ERROR, "do_connect(%d,%d): pthread_create(thr_r2l): %s",lcsock, rcsock, strerror(ret));
	ret = -1;
	goto last;
    }

    ret = pthread_join(thr_l2r, NULL);
    if (ret != 0) {
	logger(PINE_LOG_ERROR, "do_connect(%d,%d): pthread_join(l2r): %s",lcsock, rcsock, strerror(ret));
	goto last;
    }
    ret = pthread_join(thr_r2l, NULL);
    if (ret != 0) {
	logger(PINE_LOG_ERROR, "do_connect(%d,%d): pthread_join(r2l): %s",lcsock, rcsock, strerror(ret));
	ret = -1;
	goto last;
    }

    ret = 0;
 last:
    logger(PINE_LOG_DEBUG, "do_connect(%d,%d): finished(%d)", lcsock, rcsock, ret);

    return ret;
}

void *start_session(void *arg) {
    int lcsock = *(int *)arg;
    pine_mesg_t msg;
    struct iovec iov[2];
    int iovcnt;
    char buf[BUFSIZ];
    int req;
    int ret;
    logger(PINE_LOG_DEBUG, "start_session(%d): thread started", lcsock);

    session_table[lcsock].stat = PINE_STAT_INIT;
    session_table[lcsock].sock = 0;

    iov[0].iov_base = &msg;
    iov[0].iov_len	= sizeof msg;
    iov[1].iov_base = buf;
    iov[1].iov_len	= sizeof buf;
    iovcnt = ARRAY_COUNT(iov);

    for (;;) {
	ret = readv(lcsock, iov, iovcnt);
	if (ret < 0) {
	    logger(PINE_LOG_ERROR, "start_session(%d): readv(): %s", lcsock, strerror(errno));
	    goto last;
	} else if (ret == 0) {
	    logger(PINE_LOG_DEBUG, "start_session(%d): connection closed by remote peer", lcsock);
	    goto last;
	}
	req = ntohl(msg.pm_typ);
	switch (req) {
	case PINE_REQ_BIND:
 	    logger(PINE_LOG_DEBUG, "start_session(%d): REQ_BIND received", lcsock);
	    ret = do_bind(lcsock, ntohl(msg.pm_val));
	    if (ret < 0) {
		logger(PINE_LOG_ERROR, "do_bind(%d): error returned", lcsock);
		goto last;
	    }
	    break;
	case PINE_REQ_LISTEN:
 	    logger(PINE_LOG_DEBUG, "start_session(%d): REQ_LISTEN received", lcsock);
	    ret = do_listen(lcsock, ntohl(msg.pm_val));
 	    if (ret < 0) {
		logger(PINE_LOG_ERROR, "do_listen(%d): error returned", lcsock);
		goto last;
	    }
	    break;
	case PINE_REQ_ACCEPT:
 	    logger(PINE_LOG_DEBUG, "start_session(%d): REQ_ACCEPT received", lcsock);
	    ret = do_accept(lcsock);
 	    if (ret < 0) {
		logger(PINE_LOG_ERROR, "do_accept(%d): error returned", lcsock);
		goto last;
	    }
	    break;
	case PINE_REQ_CONNECT:
 	    logger(PINE_LOG_DEBUG, "start_session(%d): REQ_CONNECT received", lcsock);
	    ret = do_connect(lcsock, ntohl(msg.pm_val));
 	    if (ret < 0) {
		logger(PINE_LOG_ERROR, "do_connect(%d): error returned", lcsock);
		goto last;
	    }
	    goto last;
	default:
	    logger(PINE_LOG_ERROR, "start_session(%d): unexpected request received: %d", lcsock, req);
	    break;
	}
    }

 last:
    close(lcsock);
    close_session(lcsock);

    logger(PINE_LOG_DEBUG, "start_session(%d): finished", lcsock);
    return NULL;
}
