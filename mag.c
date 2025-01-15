#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

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

void loadConfig(int configDesc, int* valTab[6]);
void courier(int id, int pdesk[2], int msgQueue);
int main(int argc, char* argv[]) {
	if (argc < 3) {
		printf("Invalid number of arguments\n");
		exit(1);
	}
	const char* configPath = argv[1];
	const char* key = argv[2];

	int configDesc = open(configPath, O_RDONLY);
	if (configDesc == -1) {
		printf("Failed to open config file\n");
		printf("%s\n", strerror(errno));
		close(configDesc);
		exit(1);
	}
	
	int countA = -1, valA = -1, countB = -1, valB = -1,
		countC = -1, valC = -1;
	int* valTab[6] = { &countA, &valA, &countB, &valB,
			&countC, &valC };
	loadConfig(configDesc, valTab);
	close(configDesc);
	
	printf("Mag: finished config read:\n");
	printf("\tA %d (cost %d), B: %d (cost %d), C: %d (cost %d)\n", countA, valA, countB, valB, countC, valC);
	int msgQueue = msgget(ftok(key, 69), 0);
	ReqMsg reqMsg;
	ResMsg resMsg;
	
	resMsg.mtype = 1;
	msgsnd(msgQueue, &resMsg, sizeof(int), 0);

	int pipeOut[3][2], pipeIn[3][2];
	for (int i = 0; i < 3; ++i) {
		pipe(pipeOut[i]);
		pipe(pipeIn[i]);
		if (!fork()) {
			close(pipeOut[i][1]);
			close(pipeIn[i][0]);
			int pdesk[2] = { pipeOut[i][0], pipeIn[i][1] };
			courier(i, pdesk, msgQueue);
			exit(0);
		}
		printf("Courier %d created\n", i);
		close(pipeOut[i][0]);
		close(pipeIn[i][1]);
		fcntl(pipeIn[i][0], F_SETFL, O_NONBLOCK);
	}

	int workingCourier[3] = { 1, 1, 1 }, 
		workingCouriers = 3, goldEarned = 0;
	while (workingCouriers) {
		for (int i = 0; i < 3; ++i) {
			if (workingCourier[i] == 0)
				continue;

			if (read(pipeIn[i][0], &reqMsg, sizeof(ReqMsg)) > 0) {
				if (reqMsg.mtype == 0) {
					workingCourier[i] = 0;
					--workingCouriers;
					continue;
				}
				//printf("Mag: got request from courier:\n");
				//printf("\tA: %d, B: %d, C: %d\n", reqMsg.reqDet.countA, reqMsg.reqDet.countB, reqMsg.reqDet.countC);
				resMsg.mtype = 2;
				if (reqMsg.reqDet.countA <= countA &&
					reqMsg.reqDet.countB <= countB &&
					reqMsg.reqDet.countC <= countC) {
					int gold = 0;
					countA -= reqMsg.reqDet.countA;
					gold += reqMsg.reqDet.countA * valA;
					countB -= reqMsg.reqDet.countB;
					gold += reqMsg.reqDet.countB * valB;
					countC -= reqMsg.reqDet.countC;
					gold += reqMsg.reqDet.countC * valC;

					resMsg.gold = gold;
					goldEarned += gold;
					//printf("Mag: will send courier answer with %d gold\n", gold);
					write(pipeOut[i][1], &resMsg, sizeof(ResMsg));
					//printf("Mag: sent courier answer with %d gold\n", gold);
				}
				else {
					workingCourier[i] = 0;
					--workingCouriers;

					resMsg.gold = 0;
					//printf("Mag: will send courier end answerl\n");
					write(pipeOut[i][1], &resMsg, sizeof(ResMsg));
					//printf("Mag: sent courier end answer\n");
				}
			}
		}
	}
	for (int i = 0; i < 3; ++i) {
		close(pipeIn[i][0]);
		close(pipeOut[i][1]);
	}
	usleep(300000);
	printf("Mag: finished work, items left:\n");
	printf("\tA: %d, B: %d, C: %d\n", countA, countB, countC);
	printf("Mag: Gold earned: %d\n", goldEarned);
	resMsg.mtype = 3;
	msgsnd(msgQueue, &resMsg, sizeof(int), 0);
	exit(0);
}

void loadConfig(int configDesc, int* valTab[6]) {
	char buf[20], currChar;
	int valIndex = 0, index = 0;
	while (read(configDesc, &currChar, 1) && valIndex < 6) {
		if (isdigit(currChar)) {
			if (index >= 19) {
				printf("Invalid syntax of config file\n");
				close(configDesc);
				exit(1);
			}
			buf[index++] = currChar;
		}
		else if (isspace(currChar)) {
			if (index > 0) {
				buf[index] = 0;
				index = 0;
				*valTab[valIndex++] = atoi(buf);
			}
		}
		else {
			printf("Invalid syntax of config file\n");
			close(configDesc);
			exit(1);
		}
	}
	if (valTab[0] <= 0 || valTab[1] <= 0 || valTab[2] <= 0 ||
		valTab[3] <= 0 || valTab[4] <= 0 || valTab[5] <= 0) {

		printf("Invalid config values\n");
		close(configDesc);
		exit(1);
	}
}
void courier(int id, int pdesk[2], int msgQueue) {
	int timeCounter = 0;
	ReqMsg reqMsg;
	ResMsg resMsg;

	while (timeCounter < 1500) {
		if (msgrcv(msgQueue, &reqMsg, 3 * sizeof(int), 4, IPC_NOWAIT) > 0) {
			printf("Mag: Courier %d received a request:\n", id);
			printf("\tA: %d, B: %d, C: %d\n",
				reqMsg.reqDet.countA, reqMsg.reqDet.countB,
				reqMsg.reqDet.countC);
			timeCounter = 0;
			write(pdesk[1], &reqMsg, sizeof(ReqMsg));
			read(pdesk[0], &resMsg, sizeof(ResMsg));
			if (resMsg.gold == 0) {
				printf("Mag: Courier %d", id);
				printf(" wasn't able to process the request\n");
				break;
			}
			printf("Mag: Courier %d", id);
			printf(" was able to process the request, gold spent: ");
			printf("%d\n", resMsg.gold);
			msgsnd(msgQueue, &resMsg, sizeof(int), 0);
			continue;
		}
		++timeCounter;
		usleep(100000);
	}
	printf("Mag: Couried %d ends work\n", id);
	close(pdesk[0]);
	reqMsg.mtype = 0;
	write(pdesk[1], &reqMsg, sizeof(ReqMsg));
	close(pdesk[1]);
}
