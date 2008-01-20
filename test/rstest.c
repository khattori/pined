#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <assert.h>
#include <rssock.h>

#define TEST_PORT	10001

#define TEST_RSHOST	"127.0.0.1"
#define TEST_RSPORT	8888

int *socks;

void test_setup(int n) {
	int i;

	socks = malloc(n * sizeof(int));
	assert(socks != NULL);

	for (i = 0; i < n; i++) { 
		socks[i] = socket(AF_INET, SOCK_STREAM, 0);
		assert(socks[i] > 0);
	}
}

void test_rsbind(int so) {
	int ret;
	struct sockaddr_in myaddr;
	struct sockaddr_in rsaddr;

	memset(&myaddr, 0, sizeof myaddr);
	myaddr.sin_family = AF_INET;
	myaddr.sin_addr.s_addr = INADDR_ANY;
	myaddr.sin_port = htons(TEST_PORT);

	memset(&rsaddr, 0, sizeof rsaddr);
	rsaddr.sin_family = AF_INET;
	rsaddr.sin_addr.s_addr = inet_addr(TEST_RSHOST);
	rsaddr.sin_port = htons(TEST_RSPORT);

	ret = rs_bind(so, (struct sockaddr *)&myaddr, sizeof myaddr,
			(struct sockaddr *)&rsaddr, sizeof rsaddr);
	assert(ret == 0);
}

void test_rslisten(int so) {
	int ret;

	ret = rs_listen(so, SOMAXCONN);
	assert(ret == 0);
}

void test_rsaccept(int so) {
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof addr;
	int ret;

	ret = rs_accept(so, (struct sockaddr *)&addr, &addrlen);
	assert(ret > 0);

	ret = write(ret, "hello\n", sizeof "hello\n");
	assert(ret == sizeof "hello\n");
	
	close(ret);
}

int main(void) {

	test_setup(3);

	test_rsbind(socks[0]);
	test_rslisten(socks[0]);
	test_rsaccept(socks[0]);
	sleep(5);

	return 0;
}


