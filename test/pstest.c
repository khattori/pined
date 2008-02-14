#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <assert.h>
#include <pinesock.h>

#define TEST_PORT	10001


#define TEST_PSHOST	"127.0.0.1"
#define TEST_PSPORT	8888
#define TEST_PINED_PATH	"../pined/pined"
#define TEST_NUM	3

int *socks;
pid_t pid_pined;

void test_tearup(void) {
	int i;
	pid_pined = fork();
	if (pid_pined == 0) {
		if (execl(TEST_PINED_PATH, "pined", "-d", "-l", "4") < 0) {
			exit(-1);
		}
	}
	assert(pid_pined>0);
	printf("pined was started successfully(pid=%d)\n", pid_pined);

	socks = malloc(TEST_NUM * sizeof(int));
	assert(socks != NULL);

	for (i = 0; i < TEST_NUM; i++) { 
		socks[i] = socket(AF_INET, SOCK_STREAM, 0);
		assert(socks[i] > 0);
	}
}

void test_teardown(void) {
	int ret;
	int i;

	kill(pid_pined, SIGKILL);
	for (i = 0; i < TEST_NUM; i++) {
		ret = close(socks[i]);
		assert(ret == 0);
	}
	free(socks);
}

void test_psbind(void) {
	int ret;
	int i;
	struct sockaddr_in psaddr;

	for (i = 0; i < TEST_NUM; i++) {
		memset(&psaddr, 0, sizeof psaddr);
		psaddr.sin_family = AF_INET;
		psaddr.sin_addr.s_addr = inet_addr(TEST_PSHOST);
		psaddr.sin_port = htons(TEST_PSPORT);

		ret = ps_bind(socks[i], TEST_PORT+i, (struct sockaddr *)&psaddr, sizeof psaddr);
		assert(ret == 0);
	}
}

void test_pslisten(void) {
	int ret;
	int i;

	for (i = 0; i < TEST_NUM; i++) {
		ret = ps_listen(socks[i], SOMAXCONN);
		assert(ret == 0);
	}
}


static void *thread_psaccept(void *arg) {
	struct sockaddr_in addr;
	socklen_t addrlen = sizeof addr;
	int so = (int)arg;
	int ret;

	ret = ps_accept(so, (struct sockaddr *)&addr, &addrlen);
	assert(ret > 0);

	ret = write(ret, "hello\n", sizeof "hello\n");
	assert(ret == sizeof "hello\n");
	
	close(ret);

	return NULL;
}

void test_psaccept(void) {
	int ret;
	int i;
	pthread_t thread;

	for (i = 0; i < TEST_NUM; i++) {
		ret = pthread_create(&thread, NULL, thread_psaccept, (void *)socks[i]);
		assert(ret == 0);
	}
}

void test_connect(void) {
	int ret;
	int i;
	char buf[BUFSIZ];

	for (i = 0; i < TEST_NUM; i++) {
		int so;
		struct sockaddr_in addr = {0};
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(TEST_PSHOST);
		addr.sin_port = htons(TEST_PORT+i);
		so = socket(AF_INET, SOCK_STREAM, 0);
		assert(so != 0);
		ret = connect(so, (struct sockaddr *)&addr, sizeof addr);
		assert(ret == 0);
		ret = read(so, buf, sizeof buf);
		assert(ret == sizeof("hello\n"));
		assert(strcmp(buf, "hello\n") == 0);
	
	}
}

int main(void) {

	test_tearup();

	test_psbind();
	test_pslisten();
	test_psaccept();
	test_connect();

	test_teardown();

	return 0;
}


