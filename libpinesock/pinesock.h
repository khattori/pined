#ifndef __INC_PINESOCK_H__
#define __INC_PINESOCK_H__

int ps_bind(int sock, int port, struct sockaddr *ps_addr, socklen_t ps_addrlen);
int ps_listen(int sock, int backlog);
int ps_accept(int ssock, struct sockaddr *caddr, socklen_t *caddr_len);

#endif /* __INC_PINESOCK_H__ */
