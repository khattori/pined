#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>

#include "rscommon.h"
#include "rssock.h"

static int l2r_table[FD_SETSIZE];
static int r2l_table[FD_SETSIZE];

static int send_mesg(int sock, int type, int val) {
	rsrv_mesg_t msg;
	int ret;

	msg.rm_typ = htonl(type);
	msg.rm_val = htonl(val);

	ret = write(sock, &msg, sizeof msg);
	if (ret < 0) {
		perror("send_mesg: write()");
		return -1;
	}

	return 0;
}

static int expect_reply(int sock, int type, int *val) {
	rsrv_mesg_t msg;
	int ret;

	ret = read(sock, &msg, sizeof msg);
	if (ret < 0) {
		perror("expect_reply: read()");
		return -1;
	}
	if (msg.rm_typ != htonl(type)) {
		return ntohl(msg.rm_val);
	}
	if (val != NULL) {
		*val = ntohl(msg.rm_val);
	}

	return 0;
}

int rs_bind(int sock, struct sockaddr *my_addr, socklen_t myaddr_len, struct sockaddr *rs_addr, socklen_t rs_addrlen) {
	rsrv_mesg_t msg;
	struct iovec iov[2];
	int rsock;
	int iovcnt;
	int ret;

	ret = connect(sock, rs_addr, rs_addrlen);
	if (ret < 0) {
		perror("rs_bind: connect()");
		return -1;
	}
	msg.rm_typ = htonl(RSRV_REQ_BIND);
	msg.rm_val = htonl(myaddr_len);

	iov[0].iov_base = &msg;
	iov[0].iov_len  = sizeof msg;	
	iov[1].iov_base = my_addr;
	iov[1].iov_len	= myaddr_len;
	iovcnt = ARRAY_COUNT(iov);

	ret = writev(sock, iov, iovcnt);
	if (ret < 0) {
		perror("rs_bind: writev()");
		return -1;
	}
	ret = expect_reply(sock, RSRV_ACK_BIND, &rsock);
	if (ret != 0) {
		fprintf(stderr, "rs_bind: failure in remote bind: %d\n", ret);
		return -1;
	}
	l2r_table[sock]  = rsock;
	r2l_table[rsock] = sock;

	return 0;
}

int rs_listen(int sock, int backlog) {
	int ret;

	ret = send_mesg(sock, RSRV_REQ_LISTEN, backlog);
	if (ret < 0) {
		fprintf(stderr, "rs_listen: failure to send RSRV_REQ_LISTEN\n");
		return -1;
	}
	ret = expect_reply(sock, RSRV_ACK_LISTEN, NULL);
	if (ret != 0) {
		fprintf(stderr, "rs_listen: failure in remote listen: %d\n", ret);
		return -1;
	}

	return 0;
}

int rs_accept(int ssock, struct sockaddr *caddr, socklen_t *caddr_len) {
	rsrv_mesg_t msg;
	struct sockaddr_in rsaddr;
	struct sockaddr addr;
	socklen_t rsaddr_len;
	struct iovec iov[2];
	int iovcnt;
	int conn_id;
	int csock;
	int rmtyp, rmval;
	int ret;

	ret = send_mesg(ssock, RSRV_REQ_ACCEPT, 0);
	if (ret < 0) {
		fprintf(stderr, "rs_accept: failure to send RSRV_REQ_ACCEPT\n");
		return -1;
	}
	ret = expect_reply(ssock, RSRV_ACK_ACCEPT, &conn_id);
	if (ret != 0) {
		fprintf(stderr, "rs_accept: failure in remote accept: %d\n", ret);
		return -1;
	}

	csock = socket(AF_INET, SOCK_STREAM, 0);
	if (csock < 0) {
		perror("rs_accept: socket()");
		return -1;
	}

	rsaddr_len = sizeof rsaddr;
	ret = getpeername(ssock, (struct sockaddr *)&rsaddr, &rsaddr_len);
	if (ret < 0) {
		perror("rs_accept: getpeername()");
		return -1;
	}

	ret = connect(csock, (struct sockaddr *)&rsaddr, rsaddr_len);
	if (ret < 0) {
		perror("rs_accept: connect()");
		return -1;
	}

	ret = send_mesg(csock, RSRV_REQ_CONNECT, conn_id);
	if (ret < 0) {
		fprintf(stderr, "rs_accept: failure to send RSRV_REQ_CONNECT\n");
		return -1;
	}

	iov[0].iov_base = &msg;
	iov[0].iov_len	= sizeof msg;
	iov[1].iov_base = &addr;
	iov[1].iov_len  = sizeof addr;
	iovcnt = ARRAY_COUNT(iov);

	ret = readv(csock, iov, iovcnt);
	if (ret < 0) {
		perror("rs_accept: readv()");
		return -1;
	}
	rmtyp = ntohl(msg.rm_typ);
	rmval = ntohl(msg.rm_val);
	if (rmtyp != RSRV_ACK_CONNECT) {
		fprintf(stderr, "rs_accept: failure to relay connection: %d\n", rmval);
		return -1;
	}
	if (caddr != NULL && caddr_len != NULL) {
		rmval = MIN(rmval, *caddr_len);
		memcpy(caddr, &addr, rmval);
		*caddr_len = rmval;
	}

	return csock;
}

int rs_select(int n, fd_set *rfds, struct sockaddr *rs_addr, socklen_t rs_addrlen) {
	int sock;
	rsrv_mesg_t msg;
	fd_set remote_rfds;
	struct iovec iov[2];
	int iovcnt;
	int rmtyp, rmval;
	int i;
	int ret;

	sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("rs_select: socket()");
		return -1;
	}
	ret = connect(sock, rs_addr, rs_addrlen);
	if (ret < 0) {
		perror("rs_select: connect()");
		goto last;
	}
	FD_ZERO(&remote_rfds);
	for (i = 0; i < n; i++) {
		if (FD_ISSET(i, rfds)) {
			FD_SET(l2r_table[i], &remote_rfds);
		}
	}
	msg.rm_typ = htonl(RSRV_REQ_SELECT);
	msg.rm_val = htonl(sizeof remote_rfds);
	iov[0].iov_base = &msg;
	iov[0].iov_len  = sizeof msg;
	iov[1].iov_base = &remote_rfds;
	iov[1].iov_len  = sizeof remote_rfds;
	iovcnt = ARRAY_COUNT(iov);
	ret = writev(sock, iov, iovcnt);
	if (ret < 0) {
		perror("rs_select: writev()");
		goto last;
	}
	//
	ret = readv(sock, iov, iovcnt);
	if (ret < 0) {
		perror("rs_select: readv()");
		goto last;
	}
	rmtyp = ntohl(msg.rm_typ);
	rmval = ntohl(msg.rm_val);
	if (rmtyp != RSRV_ACK_SELECT) {
		fprintf(stderr, "rs_select: failure to remote select: %d\n", rmval);
		goto last;
	}

	FD_ZERO(rfds);
	for (i = 0; i < FD_SETSIZE; i++) {
		if (FD_ISSET(i, &remote_rfds)) {
			FD_SET(r2l_table[i], rfds);
		}
	}
last:
	close(sock);
	return ret;
}
