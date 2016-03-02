#include "COMEX_RDMA_both_BETA2.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>

#include <unistd.h>

#define NETLINK_COMEX_KERNEL	29
#define MAX_PAYLOAD 200 /* maximum payload size*/
#define NORMAL_MSG 5

int totalCB, nodeID;

int sock_fd;
struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
struct msghdr msg;

struct sigaction sig;

struct rdma_cb **cb_pointers;
//char RDMAmsg[64];

void sendRDMA_CB_number(int NodeID, char *RDMAmsg, int imm)
{
//	strcpy((cb_pointers[NodeID]->send_buffer).piggy, RDMAmsg);
	memcpy((cb_pointers[NodeID]->send_buffer).piggy, RDMAmsg, 64);
	do_sendout(cb_pointers[NodeID], imm);
}

void sendRDMA_nodeID(int NodeID, char *RDMAmsg, int imm)
{
	struct rdma_cb *cb_pointer;
	cb_pointer = id2cb(NodeID);

//	strcpy((cb_pointer->send_buffer).piggy, RDMAmsg);
	memcpy((cb_pointer->send_buffer).piggy, RDMAmsg, 64);
	do_sendout(cb_pointer, imm);
}

void receiveData(int n, siginfo_t *info, void *unused)
{
	int cmd_number = info->si_int;
	int configfd;
	
	if(cmd_number >= 0 && cmd_number < MAX_BUFFER){	//RDMA Write
		printf("received value %i\n", cmd_number);
	}
	else if(cmd_number >= 10000 && cmd_number < 10000+totalCB){
		int ask_target = cmd_number - 10000;
		requestPageStruct RDMAmsg;		
		
		RDMAmsg.target = nodeID;
		RDMAmsg.order = 10;
		
		printf("Ask node %d order %d\n", ask_target, RDMAmsg.order);
		sendRDMA_CB_number(ask_target, (char *)&RDMAmsg, 1001);
	}
	else if(cmd_number == 11000){
		int reply_target;
		replyPagesDesc *comexFS_givePages = NULL;
		
		configfd = open("/sys/kernel/debug/comex_dir/comex_givePages", O_RDWR);
		if(configfd < 0) {
			perror("open");
			return;
		}
		comexFS_givePages = (replyPagesDesc *)mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, configfd, 0);
		if (comexFS_givePages == MAP_FAILED) {
			perror("mmap");
			return;
		}	
		
		reply_target = comexFS_givePages->target;
		comexFS_givePages->target = nodeID;
		printf("To %d: offsetAddr %lu order %d\n", reply_target, comexFS_givePages->offsetAddr, comexFS_givePages->order);
		sendRDMA_nodeID(reply_target, (char *)comexFS_givePages, 1002);
		
		munmap(comexFS_givePages, PAGE_SIZE);
		close(configfd);
	}
	else if(cmd_number == 11001){
		printf("No available page\n");
	}
	else if(cmd_number == 12000){
//		printf("received value %i\n", cmd_number);
		BufferDescUser *myBufferDescUser;
	
		configfd = open("/sys/kernel/debug/comex_dir/comex_RDMA_write", O_RDWR);
		if(configfd < 0) {
			perror("open");
			return;
		}
		myBufferDescUser = (BufferDescUser *)mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, configfd, 0);
		if (myBufferDescUser == MAP_FAILED) {
			perror("mmap");
			return;
		}
		
		printf("To %d IDX %d R %lu\n", myBufferDescUser->nodeID, myBufferDescUser->buffIDX, myBufferDescUser->r_Offset);
		munmap(myBufferDescUser, PAGE_SIZE);
		close(configfd);
	}
}

void init_SignalHandler(){
	sig.sa_sigaction = receiveData;
	sig.sa_flags = SA_SIGINFO;
	sigaction(44, &sig, NULL);
}

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
	strcpy(NLMSG_DATA(nlh), "1");
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	msg.msg_name = (void *)&dest_addr;
	msg.msg_namelen = sizeof(dest_addr);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

    printf("Sending message to kernel\n");
    sendmsg(sock_fd, &msg, 0);
}

int main(int argc, char *argv[])
{
	char *SharedMem_Addr, RDMAmsg[64];
	unsigned long totalMem;
	int i;
	
	totalMem = strtol(argv[1], NULL, 10);
	totalCB = atoi(argv[2]);
	nodeID = atoi(argv[3]);
	
	cb_pointers = startRDMA_Client(totalCB, nodeID, totalMem, &SharedMem_Addr);	
	printf("totalMem - COMM_BUFFER = %lu\n", totalMem - COMM_BUFFER);
	SharedMem_Addr = SharedMem_Addr + totalMem - COMM_BUFFER;
	myCommStruct = (CommStruct *)SharedMem_Addr;
	printf("NodeID %d totalCB %d\n", myCommStruct->NodeID, myCommStruct->totalCB);
	
	for(i=0; i<totalCB; i++){
//		sprintf(RDMAmsg,"Hello msg from node %d", nodeID); sendRDMA_CB_number(i, 2000);
		sprintf(RDMAmsg,"Hello msg from node %d", nodeID);
		sendRDMA_CB_number(i, RDMAmsg, 2000);
	}
	
	init_SignalHandler();
	init_NetLink();
	while(1){
		recvmsg(sock_fd, &msg, 0);
//		printf("   %s: NetLink from Kernel \"%s\"\n", __FUNCTION__, NLMSG_DATA(nlh));
//		sprintf(NLMSG_DATA(nlh), "");
    }
    close(sock_fd);

	return 0;
}
