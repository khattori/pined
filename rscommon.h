#ifndef __INC_RSCOMMON_H__
#define __INC_RSCOMMON_H__

#define RSRV_REQ_BIND		0	/* REQ_BIND(myaddr) */
#define RSRV_ACK_BIND		1	/* ACK_BIND */
#define RSRV_REQ_LISTEN		2	/* REQ_LISTEN(backlog) */
#define RSRV_ACK_LISTEN		3	/* REQ_LISTEN */
#define RSRV_REQ_ACCEPT		4	/* REQ_ACCEPT */
#define RSRV_ACK_ACCEPT		5	/* ACK_ACCEPT(sock_num) */
#define RSRV_REQ_CONNECT	6	/* REQ_CONN(sock_num) */
#define RSRV_ACK_CONNECT	7	/* ACK_CONN(peeraddr) */
#define RSRV_REQ_SELECT		8	/* REQ_SELECT(sock list) */
#define RSRV_ACK_SELECT		9	/* ACK_SELECT(sock list) */
#define RSRV_NAK		10	/* NAK(errno) */

#define RSRV_DEFAULT_PORT	8888

typedef struct rsrv_mesg {
	int rm_typ;
	int rm_val;
} rsrv_mesg_t;

#define ARRAY_COUNT(array)	(sizeof(array)/sizeof(array[0]))

#endif /* __INC_RSCOMMON_H__ */
