#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/mman.h>
#include <linux/netlink.h>
#include <unistd.h>

#define IS_SERVER 1
#include "RDMA_COMEX_both_BETA2.h"

#define LISTEN_PORT		7795
#define NETLINK_COMEX	28
#define MAX_PAYLOAD 	200 /* maximum payload size*/

int sock_fd;
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
struct msghdr msg;

struct sigaction sig;

char *myMessage;
char *COMEX_Area;
char *COMEX_Comm_Buffer;

void receiveData(int n, siginfo_t *info, void *unused) {
	int cmd_number = info->si_int;
	
	printf("received value %i\n", cmd_number);
}
void init_SignalHandler(){
	sig.sa_sigaction = receiveData;
	sig.sa_flags = SA_SIGINFO;
	sigaction(44, &sig, NULL);
}

void sendNLMssge(char* myMessage){
//	strncpy(NLMSG_DATA(nlh), myMessage, MAX_PAYLOAD);	//	This is Massg !!!	
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;	

	sendmsg(sock_fd, &msg, 0);
}

int init_Netlink(){
	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_COMEX);
	if (sock_fd < 0){
		printf("Fail to create sock_fd\n");
		return 0;
	}
	
	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid(); 		/* self pid */
	
	bind(sock_fd, (struct sockaddr *)&src_addr, sizeof(src_addr));
	
	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0; 		/* For Linux Kernel */
	dest_addr.nl_groups = 0; 	/* unicast */
	
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	
	printf("Success to create Socket\n");
	return 1;
}

void recv_request(int Requester, int Order){
	
	myMessage[0] = 1;
	
//	sprintf(myMessage, "1100 %d %d", Requester, Order);
//	printf("recv_request: %s\n", myMessage);
//	sendNLMssge(myMessage);
}

void fill_COMEX_freelist(int remoteID, unsigned long offset, int order){
	
	myMessage[0] = 2;
	
//	sprintf(myMessage, "1200 %d %lu %d", remoteID, offset, order);
//	printf("%s\n", myMessage);
//	sendNLMssge(myMessage);
}

int main(int argc, char *argv[])
{
	int shmid, totalCB, nodeID;
	struct rdma_cb **cb_pointers;
	unsigned long N_Pages, totalChar, i, j; //, Checksum=0;
	unsigned long totalCOMEX, totalWriteBuffer, totalReadBuffer, totalCommBuffer, totalMem;
	initStruct *myInitStruct;
	key_t key = 5683;
	
	N_Pages = strtol(argv[1], NULL, 10);	// Number of Pages as input.
	totalCB = atoi(argv[2]);
	nodeID = atoi(argv[3]);
	
	N_Pages = N_Pages / 1024;				// Make it multiple of 1024.
	N_Pages = N_Pages * 1024;				// Make it multiple of 1024.
	totalChar = N_Pages * 4096;				// char => 1 bytes * 4096 = 1 page
	printf("N_Pages %lu \n", N_Pages);
	
	totalCOMEX = sizeof(char)*totalChar;
	totalWriteBuffer = totalReadBuffer = sizeof(char)*4096*MAX_BUFFER;
	totalCommBuffer = sizeof(char)*COMM_BUFFER;
	
	totalMem = totalCOMEX + totalWriteBuffer + totalReadBuffer + totalCommBuffer;
	printf("totalMem: %lu\n", totalMem);
	
	if ((shmid = shmget(key, totalMem, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }
	if ((COMEX_Area = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        exit(1);
    }
//	COMEX_Area = (char *)malloc(sizeof(char)*totalMem);
	for(i=0, j=0; i < totalChar + (4096*MAX_BUFFER) + (4096*MAX_BUFFER) + COMM_BUFFER; i+=4096, j++){
		COMEX_Area[i] = j;
	}
	memset(COMEX_Area, 0, totalMem);
		
	init_Netlink();
	init_SignalHandler();

	myMessage = NLMSG_DATA(nlh);
	myMessage[0] = 0;
	myInitStruct = (initStruct *)&myMessage[1];
	myInitStruct->NodeID = nodeID;
	myInitStruct->N_Nodes = totalCB;
	myInitStruct->COMEX_Address = COMEX_Area;
	myInitStruct->COMEX_Address_End = &COMEX_Area[(N_Pages-1)*4096];
	myInitStruct->Write_Buffer_Address = COMEX_Area + totalChar;
	myInitStruct->Read_Buffer_Address = COMEX_Area + totalChar + 4096*MAX_BUFFER;
	myInitStruct->MaxBuffer = MAX_BUFFER;
	myInitStruct->Comm_Buffer_Address = COMEX_Area + totalChar + 4096*MAX_BUFFER + 4096*MAX_BUFFER;
	
	printf("\ntotalChar + 4096*MAX_BUFFER + 4096*MAX_BUFFER  = %lu\n", totalChar + 4096*MAX_BUFFER + 4096*MAX_BUFFER);
	myCommStruct = (CommStruct *)myInitStruct->Comm_Buffer_Address;
	myCommStruct->NodeID = nodeID;
	myCommStruct->totalCB = totalCB;
	printf("NodeID %d totalCB %d\n", myCommStruct->NodeID, myCommStruct->totalCB);
	
	sendNLMssge(myMessage);
	cb_pointers = startRDMA_Server(totalCB, nodeID, totalMem, COMEX_Area);
	close(sock_fd);
	
	return 0;
}