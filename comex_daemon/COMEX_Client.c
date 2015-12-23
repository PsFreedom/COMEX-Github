#include "RDMA_COMEX_both_BETA2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>

#include <unistd.h>

#define NETLINK_COMEX_KERNEL	29
#define MAX_PAYLOAD 200 /* maximum payload size*/
#define NORMAL_MSG 5

int sock_fd;
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
struct msghdr msg;

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

void sendRDMA_CB_number(int NodeID, int imm){
	strcpy((cb_pointers[NodeID]->send_buffer).piggy, RDMAmsg);
	do_sendout(cb_pointers[NodeID], imm);
}

void sendRDMA_nodeID(int NodeID, int imm){
	struct rdma_cb *cb_pointer;
	
	cb_pointer = id2cb(NodeID);

	strcpy((cb_pointer->send_buffer).piggy, RDMAmsg);
	do_sendout(cb_pointer, imm);
}

int main(int argc, char *argv[])
{
	unsigned long totalMem;
	int totalCB, nodeID, i;
	int cmdNo, target, Order, oriOrder;
	unsigned int Size;
	unsigned long Offset, bufferOffset;
	
	totalMem = strtol(argv[1], NULL, 10);
	totalCB = atoi(argv[2]);
	nodeID = atoi(argv[3]);
	
	cb_pointers = startRDMA_Client(totalCB, nodeID, totalMem);	
	for(i=0; i<totalCB; i++){
		sprintf(RDMAmsg,"Hello msg from node %d", nodeID); sendRDMA_CB_number(i, 2000);
	}
	
	init_NetLink();
	recvmsg(sock_fd, &msg, 0);		
	printf("   Received message payload: %s\n", NLMSG_DATA(nlh));
    while(1){
		recvmsg(sock_fd, &msg, 0);
//		printf("   %s: %s\n", __FUNCTION__, NLMSG_DATA(nlh));
		cmdNo = (int)get_Param_from_Packet(NLMSG_DATA(nlh), 0);
		switch(cmdNo){
			case 1100:	// Request for pages
				printf("   %s: %s\n", __FUNCTION__, NLMSG_DATA(nlh));
				
				target = (int)get_Param_from_Packet(NLMSG_DATA(nlh), 1);
				Order = (int)get_Param_from_Packet(NLMSG_DATA(nlh), 2);
				
				sprintf(RDMAmsg,"1100 %d %d", nodeID, Order); sendRDMA_CB_number(target, 1000);
				break;
			case 1101:	// Reply with pages
				printf("   %s: %s\n", __FUNCTION__, NLMSG_DATA(nlh));
				
				target = (int)get_Param_from_Packet(NLMSG_DATA(nlh), 1);
				Offset = get_Param_from_Packet(NLMSG_DATA(nlh), 2);
				Order = (int)get_Param_from_Packet(NLMSG_DATA(nlh), 3);
				oriOrder = (int)get_Param_from_Packet(NLMSG_DATA(nlh), 4);
				
				if(Offset == 200){		// MAX unsigned long (-1)
					printf("	not enough pages");
				}
				else{
					sprintf(RDMAmsg,"1200 %d %lu %d", nodeID, Offset, Order); sendRDMA_nodeID(target, 1000);
				}
				break;
			case 2100:	// Write page to remote node
				printf("   %s: %s\n", __FUNCTION__, NLMSG_DATA(nlh));
				
				bufferOffset = get_Param_from_Packet(NLMSG_DATA(nlh), 1);
				target = (int)get_Param_from_Packet(NLMSG_DATA(nlh), 2);
				Offset = get_Param_from_Packet(NLMSG_DATA(nlh), 3);
				Size = (int)get_Param_from_Packet(NLMSG_DATA(nlh), 4);
				
				do_write(cb_pointers[target], bufferOffset, Offset, Size*4096);
//				sprintf(RDMAmsg,"9100 %lu", Offset); sendRDMA_CB_number(target, 1000);
				break;
			default:
				printf(">>> default: %s\n", NLMSG_DATA(nlh));
				break;
		}
    }
    close(sock_fd);		
	return 0;
}
