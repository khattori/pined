#ifndef __INC_RSSOCK_H__
#define __INC_RSSOCK_H__

int rs_bind(int sock, struct sockaddr *my_addr, socklen_t myaddr_len, struct sockaddr *rs_addr, socklen_t rs_addrlen);
int rs_listen(int sock, int backlog);
int rs_accept(int ssock, struct sockaddr *caddr, socklen_t *caddr_len);
int rs_select(int n, fd_set *rfds, struct sockaddr *rs_addr, socklen_t rs_addrlen); 

#endif /* __INC_RSSOCK_H__ */
