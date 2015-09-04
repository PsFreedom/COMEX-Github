#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/mman.h>
#include <linux/netlink.h>
#include <unistd.h>

#define IS_SERVER 1
#include "RDMA_COMEX_both.h"

#define LISTEN_PORT		7795
#define NETLINK_COMEX	28
#define MAX_PAYLOAD 	200 /* maximum payload size*/

#define MAX_BUFFER 1024

int sock_fd;
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
struct msghdr msg;

struct sigaction sig;

char *COMEX_Area;
char *COMEX_Buffer;

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
	strcpy(NLMSG_DATA(nlh), myMessage);	//	This is Massg !!!	
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
	char myMessage[200];
	
	sprintf(myMessage, "1100 %d %d", Requester, Order);
//	printf("recv_request: %s\n", myMessage);
	sendNLMssge(myMessage);
}

void fill_COMEX_freelist(int remoteID, unsigned long offset, int order){
	char myMessage[200];
	
	sprintf(myMessage, "1200 %d %lu %d", remoteID, offset, order);
	printf("%s\n", myMessage);
	sendNLMssge(myMessage);
}

int main(int argc, char *argv[])
{
	int shmid, totalCB, nodeID;
	struct rdma_cb **cb_pointers;
	unsigned long N_Pages, totalChar, i, j; //, Checksum=0;
	unsigned long totalCOMEX, totalBuffer, totalMem;
	char myMessage[200];
	key_t key = 5683;
	
	N_Pages = strtol(argv[1], NULL, 10);	// Number of Pages as input.
	totalCB = atoi(argv[2]);
	nodeID = atoi(argv[3]);
	
	N_Pages = N_Pages / 1024;				// Make it multiple of 1024.
	N_Pages = N_Pages * 1024;				// Make it multiple of 1024.
	totalChar = N_Pages * 4096;				// char => 1 bytes * 4096 = 1 page
	printf("N_Pages %lu \n", N_Pages);
	
	totalCOMEX = sizeof(char)*totalChar;
	totalBuffer = sizeof(char)*4096*MAX_BUFFER;
	totalMem = totalCOMEX + totalBuffer;
	printf("totalMem: %lu\n", totalMem);
	
	if ((shmid = shmget(key, totalMem, IPC_CREAT | 0666)) < 0) {
        perror("shmget");
        exit(1);
    }
	if ((COMEX_Area = shmat(shmid, NULL, 0)) == (char *) -1) {
        perror("shmat");
        exit(1);
    }
	
	COMEX_Buffer = COMEX_Area + totalChar;
	for(i=0, j=0; i < totalChar + (4096*MAX_BUFFER); i+=4096, j++){
		COMEX_Area[i] = j;
	}
	memset(COMEX_Area, 0, totalMem);
	
	init_Netlink();
	init_SignalHandler();
	sprintf(myMessage, "%d %d %d %lu %lu %lu %d", 0, nodeID, totalCB, COMEX_Area, &COMEX_Area[(N_Pages-1)*4096], COMEX_Buffer, MAX_BUFFER);
	printf("%s\n", myMessage);
	sendNLMssge(myMessage);

	cb_pointers = startRDMA_Server(totalCB, nodeID, totalMem,COMEX_Area);
	close(sock_fd);
	return 0;
}











//	while(1){
//		Checksum = 0;
//		for(i=0; i < totalInt+(1024*MAX_BUFFER); i++){
//			Checksum = Checksum + COMEX_Area[i];
//		}
//		printf("Checksum = %ld \n", Checksum);
//		sleep(60);
//	}