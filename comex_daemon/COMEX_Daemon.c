#include "RDMA_COMEX.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/mman.h>
#include <linux/netlink.h>

#include <unistd.h>

#define LISTEN_PORT		7795
#define NETLINK_COMEX	28
#define MAX_PAYLOAD 	200 /* maximum payload size*/

#define NodeID 0
#define N_Nodes 2
#define MAX_BUFFER 1024

int sock_fd;
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
struct msghdr msg;

struct sigaction sig;

int *COMEX_Area;
int *COMEX_Buffer;

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

void fill_COMEX_freelist(int remoteID, unsigned long remoteAddr, int nPages){
	char myMessage[200];
	
	sprintf(myMessage, "100 %d %lu %d", remoteID, remoteAddr, nPages);
	printf("%s\n", myMessage);
	sendNLMssge(myMessage);
}

void recv_request(int Order){
	char myMessage[50];
	
	sprintf(myMessage, "200 %d", Order);
	printf("%s\n", myMessage);
	sendNLMssge(myMessage);
}

int main(int argc, char *argv[]){
	int i;
	unsigned long N_Pages, totalInt, Checksum=0;
	unsigned long totalCOMEX, totalBuffer, totalMem;
	char myMessage[200];
	
	struct rdma_cb *RDMA_cb;

	N_Pages = strtol(argv[1], NULL, 10);	// Number of Pages as input.
	N_Pages = N_Pages / 1024;				// Make it multiple of 1024.
	N_Pages = N_Pages * 1024;				// Make it multiple of 1024.
	totalInt = N_Pages * 1024;				// int => 4 bytes * 1024 = 1 page
	printf("N_Pages %lu \n", N_Pages);
	
	totalCOMEX = sizeof(int)*totalInt;
	totalBuffer = sizeof(int)*1024*MAX_BUFFER;
	totalMem = totalCOMEX + totalBuffer;
	
	COMEX_Area = (int*)malloc(totalMem);	// Mem allocation
	COMEX_Buffer = COMEX_Area + totalInt;
//	mlock(COMEX_Area, totalMem);
	for(i=0; i<totalInt + (1024*MAX_BUFFER); i++){
		COMEX_Area[i] = i;
	}
	memset(COMEX_Area, 0, totalMem);
	
	init_Netlink();
	init_SignalHandler();

	sprintf(myMessage, "%d %d %d %lu %lu %lu %d", 0, NodeID, N_Nodes, COMEX_Area, &COMEX_Area[(N_Pages-1)*1024], COMEX_Buffer, MAX_BUFFER);
	printf("%s\n", myMessage);
	sendNLMssge(myMessage);
	
	RDMA_cb = create_control_block();
	RDMA_cb->rdma_buf_len = totalMem;
	RDMA_cb->rdma_buffer = COMEX_Buffer;
	RDMA_cb->sin.sin_port = LISTEN_PORT;
	startRDMA(RDMA_cb);

/*	while(1){
		printf("Checksum = %ld \n", Checksum);
		for(i=0; i < totalInt+(1024*MAX_BUFFER); i++){
			Checksum = Checksum + COMEX_Area[i];
		}
		sleep(60);
	}
*/	
	close(sock_fd);
	return 0;
}
