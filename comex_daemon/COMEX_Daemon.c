#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <signal.h>

#include <sys/socket.h>
#include <linux/netlink.h>

#include "mm_Buddy/page_alloc_COMEX.h"

#define NETLINK_USER 31
#define MAX_PAYLOAD 128 /* maximum payload size*/

struct sockaddr_nl src_addr, dest_addr;
struct nlmsghdr *nlh = NULL;
struct iovec iov;
int sock_fd;
struct msghdr msg;
struct sigaction sig;

int *COMEX_Area;

void sendNLMssge(char* myMessage){
	strcpy(NLMSG_DATA(nlh), myMessage);	//	This is Massg !!!	
	iov.iov_base = (void *)nlh;
    iov.iov_len = nlh->nlmsg_len;
    msg.msg_name = (void *)&dest_addr;
    msg.msg_namelen = sizeof(dest_addr);
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;	
	printf("Sending message to kernel module\n");
    sendmsg(sock_fd, &msg, 0);
}

int getOrder(int number_of_pages){
	int size=1, Order=0;
	
	while(size <= number_of_pages){
		size = size*2;
		Order++;
	}
	return Order;
}

void receiveData(int n, siginfo_t *info, void *unused) {
	int cmd_number = info->si_int;
	int *startAddr, *endAddr, Order;
	char myMessage[50];
	struct page *pPage;
	
	if(cmd_number < 0){
		printf("received value %i\n", cmd_number);
	}
	else{
		printf("received value %i\n", cmd_number);
		Order = getOrder(cmd_number);
		pPage = __rmqueue_smallest(Order);
		sprintf(myMessage, "%d %lu %d", 200, &COMEX_Area[(pPage->pageNO)*1024], Order);
		sendNLMssge(myMessage);
	}
}

void init_SignalHandler(){
	sig.sa_sigaction = receiveData;
	sig.sa_flags = SA_SIGINFO;
	sigaction(44, &sig, NULL);
}

int init_Netlink(){

	sock_fd = socket(PF_NETLINK, SOCK_RAW, NETLINK_USER);
	if (sock_fd < 0){
		printf("Fail to create Socket\n");
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
	
	return 1;
}

int main(int argc, char *argv[]){	
	int i;
	unsigned long N_Pages,totalInt;
	char strArea_start[30], strArea_end[30], myMessage[50];
	
	N_Pages = strtol(argv[1], NULL, 10);
	totalInt = N_Pages*1024;
	
	COMEX_Area = (int*)malloc(sizeof(int)*totalInt);
	if(!mlock(COMEX_Area, sizeof(int)*totalInt)){
		printf("Mlock success\n");

	}
	else{
		printf("Mlock fails\n");
		return -1;
	}
	
	printf("%s\n", __FUNCTION__);
	initBuddy(N_Pages);
	init_Netlink();
	init_SignalHandler();
	
	sendNLMssge("0");
	
/*	sprintf(strArea_start, "%lu", COMEX_Area);
	sprintf(strArea_end, "%lu", &COMEX_Area[totalInt-1]);
	sprintf(myMessage, "%d %s %s", 100, strArea_start, strArea_end);
	printf("%s\n", myMessage);
	sendNLMssge(myMessage);
*/	
/*	print_free_list();
	__rmqueue_smallest(3);
	print_free_list();
	__rmqueue_smallest(2);
	print_free_list();
*/
	
	while(1){
		for(i=0; i<totalInt; i++){
			COMEX_Area[i] = totalInt - i;
		}
		sleep(60);
	}
	
	close(sock_fd);
	return 0;
}
