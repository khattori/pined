#ifndef __INC_RSSOCK_H__
#define __INC_RSSOCK_H__

int rs_bind(int sock, int port, struct sockaddr *rs_addr, socklen_t rs_addrlen);
int rs_listen(int sock, int backlog);
int rs_accept(int ssock, struct sockaddr *caddr, socklen_t *caddr_len);

#endif /* __INC_RSSOCK_H__ */
