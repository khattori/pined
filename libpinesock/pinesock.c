#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>

#include "pinecommon.h"
#include "pinesock.h"

static int send_mesg(int sock, int type, int val) {
	pine_mesg_t msg;
	int ret;

	msg.pm_typ = htonl(type);
	msg.pm_val = htonl(val);

	ret = write(sock, &msg, sizeof msg);
	if (ret < 0) {
		perror("send_mesg: write()");
		return -1;
	}

	return 0;
}

static int expect_reply(int sock, int type, int *val) {
	pine_mesg_t msg;
	int ret;

	ret = read(sock, &msg, sizeof msg);
	if (ret < 0) {
		perror("expect_reply: read()");
		return -1;
	}
	if (ret == 0) {
		fprintf(stderr, "expect_reply: session closed by remote peer\n");
		return -1;
	}
	if (msg.pm_typ != htonl(type)) {
		return ntohl(msg.pm_val);
	}
	if (val != NULL) {
		*val = ntohl(msg.pm_val);
	}

	return 0;
}

int ps_bind(int sock, int port, struct sockaddr *ps_addr, socklen_t ps_addrlen) {
	int ret;

	ret = connect(sock, ps_addr, ps_addrlen);
	if (ret < 0) {
		perror("ps_bind: connect()");
		return -1;
	}
	ret = send_mesg(sock, PINE_REQ_BIND, port);
	if (ret < 0) {
		fprintf(stderr, "ps_bind: failure to send PINE_REQ_BIND\n");
		return -1;
	}
	ret = expect_reply(sock, PINE_ACK_BIND, NULL);
	if (ret != 0) {
		fprintf(stderr, "ps_bind: failure in remote bind: %d\n", ret);
		return -1;
	}

	return 0;
}

int ps_listen(int sock, int backlog) {
	int ret;

	ret = send_mesg(sock, PINE_REQ_LISTEN, backlog);
	if (ret < 0) {
		fprintf(stderr, "ps_listen: failure to send PINE_REQ_LISTEN\n");
		return -1;
	}
	ret = expect_reply(sock, PINE_ACK_LISTEN, NULL);
	if (ret != 0) {
		fprintf(stderr, "ps_listen: failure in remote listen: %d\n", ret);
		return -1;
	}

	return 0;
}

int ps_accept(int ssock, struct sockaddr *caddr, socklen_t *caddr_len) {
	pine_mesg_t msg;
	struct sockaddr_in rsaddr;
	struct sockaddr addr;
	socklen_t rsaddr_len;
	struct iovec iov[2];
	int iovcnt;
	int conn_id;
	int csock;
	int pmtyp, pmval;
	int ret;

	ret = send_mesg(ssock, PINE_REQ_ACCEPT, 0);
	if (ret < 0) {
		fprintf(stderr, "ps_accept: failure to send PINE_REQ_ACCEPT\n");
		return -1;
	}
	ret = expect_reply(ssock, PINE_ACK, NULL);
	if (ret != 0) {
		fprintf(stderr, "ps_accept: failure in remote accept: %d\n", ret);
		return -1;
	}
	ret = expect_reply(ssock, PINE_ACK_ACCEPT, &conn_id);
	if (ret != 0) {
		fprintf(stderr, "ps_accept: failure in remote accept: %d\n", ret);
		return -1;
	}
	csock = socket(AF_INET, SOCK_STREAM, 0);
	if (csock < 0) {
		perror("ps_accept: socket()");
		return -1;
	}

	rsaddr_len = sizeof rsaddr;
	ret = getpeername(ssock, (struct sockaddr *)&rsaddr, &rsaddr_len);
	if (ret < 0) {
		perror("ps_accept: getpeername()");
		return -1;
	}

	ret = connect(csock, (struct sockaddr *)&rsaddr, rsaddr_len);
	if (ret < 0) {
		perror("ps_accept: connect()");
		return -1;
	}

	ret = send_mesg(csock, PINE_REQ_CONNECT, conn_id);
	if (ret < 0) {
		fprintf(stderr, "ps_accept: failure to send PINE_REQ_CONNECT\n");
		return -1;
	}

	iov[0].iov_base = &msg;
	iov[0].iov_len	= sizeof msg;
	iov[1].iov_base = &addr;
	iov[1].iov_len  = sizeof addr;
	iovcnt = ARRAY_COUNT(iov);

	ret = readv(csock, iov, iovcnt);
	if (ret < 0) {
		perror("ps_accept: readv()");
		return -1;
	}
	pmtyp = ntohl(msg.pm_typ);
	pmval = ntohl(msg.pm_val);
	if (pmtyp != PINE_ACK_CONNECT) {
		fprintf(stderr, "ps_accept: failure to relay connection: %d\n", pmval);
		return -1;
	}
	if (caddr != NULL && caddr_len != NULL) {
		pmval = MIN(pmval, *caddr_len);
		memcpy(caddr, &addr, pmval);
		*caddr_len = pmval;
	}

	return csock;
}
