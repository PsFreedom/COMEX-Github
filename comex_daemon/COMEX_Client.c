#include "RDMA_COMEX_both.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#include <unistd.h>

#define NETLINK_COMEX_KERNEL	29
#define MAX_PAYLOAD 200 /* maximum payload size*/
#define NORMAL_MSG 5

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
struct msghdr msg;
int sock_fd;

struct rdma_cb **cb_pointers;
char RDMAmsg[64];

int init_NetLink(){

	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_COMEX_KERNEL);
	if(sock_fd < 0)
		return -1;

	memset(&src_addr, 0, sizeof(src_addr));
	src_addr.nl_family = AF_NETLINK;
	src_addr.nl_pid = getpid();  /* self pid */
	
    /* interested in group 1<<0 */
    bind(sock_fd, (struct sockaddr*)&src_addr, sizeof(src_addr));

	memset(&dest_addr, 0, sizeof(dest_addr));
	dest_addr.nl_family = AF_NETLINK;
	dest_addr.nl_pid = 0;   /* For Linux Kernel */
	dest_addr.nl_groups = 0; /* unicast */
    
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_PAYLOAD));
	nlh->nlmsg_len = NLMSG_SPACE(MAX_PAYLOAD);
	nlh->nlmsg_pid = getpid();
	nlh->nlmsg_flags = 0;
	strcpy(NLMSG_DATA(nlh), "Hello");
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

    printf("Sending message to kernel\n");
    sendmsg(sock_fd, &msg, 0);
}

void sendRDMA(int NodeID, int imm){
	strcpy((cb_pointers[NodeID]->send_buffer).piggy, RDMAmsg);
	do_sendout(cb_pointers[NodeID], imm);
}

int main(int argc, char *argv[])
{
	int totalCB, nodeID, i;
	unsigned long totalMem;
	
	totalMem = strtol(argv[1], NULL, 10);
	totalCB = atoi(argv[2]);
	nodeID = atoi(argv[3]);	
	
	cb_pointers = startRDMA_Client(totalCB, nodeID, totalMem);	

	init_NetLink();
    while(1){
		for(i=0; i<totalCB; i++){
			sprintf(RDMAmsg,"Hello msg from node %d\n", nodeID); sendRDMA(i, 1000);
		}
		printf("Waiting for message from kernel...\n");
		recvmsg(sock_fd, &msg, 0);		
		printf(" Received message payload: %s\n", NLMSG_DATA(nlh));				
    }
    close(sock_fd);		
	return 0;
}
