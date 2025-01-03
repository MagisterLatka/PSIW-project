#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct {
	long mtype;
	struct {
		int countA, countB, countC;
	} reqDet;
} ReqMsg;
typedef struct {
	long mtype;
	int gold;
} ResMsg;

int main(int argc, char* argv[]) {
	if (argc < 6) {
		printf("Invalid number of arguments\n");
		exit(1);
	}
	const char* key = argv[1];
	int countOfRequests = atoi(argv[2]), maxA = atoi(argv[3]),
		maxB = atoi(argv[4]), maxC = atoi(argv[5]);
	
	int msgQueue = msgget(ftok(key, 69), 0640 | IPC_CREAT);

	printf("Dysp: waiting for mags to connect\n");

	ReqMsg reqMsg;
	ResMsg resMsg;
	for (int i = 0; i < 3; ++i) {
		msgrcv(msgQueue, &resMsg, sizeof(int), 1, 0);
	}
	
	printf("Dysp: 3 mags connected, starting to send requests\n");
	usleep(100000);

	int workingMags = 3, goldSpent = 0;
	while (workingMags > 0) {
		while (msgrcv(msgQueue, &resMsg, sizeof(int), 3, IPC_NOWAIT) > 0) {
			--workingMags;
		}
		while (msgrcv(msgQueue, &resMsg, sizeof(int), 2, IPC_NOWAIT) > 0) {
			printf("Dysp: received confirmation, gold spent: %d\n", resMsg.gold);
			goldSpent += resMsg.gold;
		}
		if (countOfRequests > 0) {
			--countOfRequests;
			reqMsg.mtype = 4;
			reqMsg.reqDet.countA = rand() % (maxA + 1); //Function used\
				to pick random values is rand() from Standard C Lib
			reqMsg.reqDet.countB = rand() % (maxB + 1);
			reqMsg.reqDet.countC = rand() % (maxC + 1);
			msgsnd(msgQueue, &reqMsg, 3 * sizeof(int), 0);
			printf("Dysp: a request has been made:\n");
			printf("\tA: %d, B: %d, C: %d\n", reqMsg.reqDet.countA,
				reqMsg.reqDet.countB, reqMsg.reqDet.countC);
			usleep(100000);
		}
	}

	msgctl(msgQueue, IPC_RMID, 0);
	printf("Dysp: closing, spent %d gold\n", goldSpent);
	exit(0);
}
