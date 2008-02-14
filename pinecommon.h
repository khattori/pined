#ifndef __INC_PINECOMMON_H__
#define __INC_PINECOMMON_H__

#define PINE_REQ_BIND		0	/* REQ_BIND(port)      */
#define PINE_ACK_BIND		1	/* ACK_BIND            */
#define PINE_REQ_LISTEN		2	/* REQ_LISTEN(backlog) */
#define PINE_ACK_LISTEN		3	/* REQ_LISTEN          */
#define PINE_REQ_ACCEPT		4	/* REQ_ACCEPT          */
#define PINE_ACK_ACCEPT		5	/* ACK_ACCEPT(sock_id) */
#define PINE_REQ_CONNECT	6	/* REQ_CONN(sock_id)   */
#define PINE_ACK_CONNECT	7	/* ACK_CONN(peeraddr)  */
#define PINE_ACK		8	/* ACK                 */
#define PINE_NAK		9	/* NAK(errno)          */

#define PINE_DEFAULT_PORT	8888

typedef struct pine_mesg {
	int pm_typ;
	int pm_val;
} pine_mesg_t;

#define ARRAY_COUNT(array)	(sizeof(array)/sizeof(array[0]))

#endif /* __INC_PINECOMMON_H__ */
