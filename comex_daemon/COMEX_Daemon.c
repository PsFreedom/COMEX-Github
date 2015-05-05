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
//	printf("Sending message to kernel module\n");
    sendmsg(sock_fd, &msg, 0);
}

int getOrder(int number_of_pages){
	int size=1, Order=0;
	
	while(size <= number_of_pages && Order <= 10){
		size = size*2;
		Order++;
	}
	return Order;
}

void receiveData(int n, siginfo_t *info, void *unused) {
	int cmd_number = info->si_int;
	int *startAddr, *endAddr, Order, pageNOtoFree;
	char myMessage[50];
	struct page *pPage;
	
	if(cmd_number < -15){
		pageNOtoFree = cmd_number/(-16);
		pageNOtoFree = pageNOtoFree - 1;
		
		__free_one_page(pageNOtoFree, 0);
		printf("Free page %i\n", pageNOtoFree);
	}
	else if(cmd_number < 0){
//		printf("received value %i\n", cmd_number);
	}
	else{
//		printf("received value %i\n", cmd_number);
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
	unsigned long N_Pages, totalInt, SumContent;
	char strArea_start[30], strArea_end[30], myMessage[50];
	struct page *testPage, *testPage2;
	
	N_Pages = strtol(argv[1], NULL, 10);	// Number of Pages as input.
	totalInt = N_Pages*1024;				// int => 4 bytes * 1024 = 1 page
	
	COMEX_Area = (int*)malloc(sizeof(int)*totalInt);	// Mem allocation
	if(!mlock(COMEX_Area, sizeof(int)*totalInt)){		// Pin down
		printf("Mlock success\n");
	}
	else{
		printf("Mlock fails\n");
		return -1;
	}
	
	for(i=0; i<totalInt; i++){		// Init all page to 0
		COMEX_Area[i] = 0;
	}
	
	printf("%s\n", __FUNCTION__);	
	initBuddy(N_Pages);
	init_Netlink();
	init_SignalHandler();
	
	sprintf(strArea_start, "%lu", COMEX_Area);
	sprintf(strArea_end, "%lu", &COMEX_Area[totalInt-1]);
	sprintf(myMessage, "%d %s %s", 0, strArea_start, strArea_end);
	printf("%s\n", myMessage);
	sendNLMssge(myMessage);
	
//	testPage = __rmqueue_smallest(3);
//	testPage2 = __rmqueue_smallest(2);
//	print_free_list();
//	__free_one_page(testPage, 3);
//	print_free_list();
//	__free_one_page(testPage2, 2);
//	print_free_list();
	
//	for(i=0; i<32; i++){
//		printf("%d > %d \n", i, page_order(testPage+i));
//	}
	
	while(1){
		SumContent = 0;
//		for(i=0; i<totalInt; i++){
//			SumContent = SumContent + COMEX_Area[i];
//		}	
		printf("SumContent = %lu\n", SumContent);
		sleep(6000);
	}
	
	close(sock_fd);
	return 0;
}
