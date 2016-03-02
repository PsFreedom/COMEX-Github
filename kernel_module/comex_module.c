#include <linux/module.h>		/* Needed by all modules */
#include <linux/kernel.h>		/* Needed for KERN_INFO */
#include <linux/init.h>			/* Needed for the macros */

#include <linux/mm.h>			/* Needed for COMEX additional function */
#include <linux/mm_types.h>		/* Needed for page destcriptor */
#include <linux/pid.h>			/* Needed for find_vpid */
#include <linux/sched.h>		/* Needed for task_struct */

#include <net/sock.h>			// Netlink Socket
#include <linux/netlink.h>		// Netlink Socket
#include <linux/skbuff.h>		// Netlink Socket

unsigned int COMEX_PID = 0;
struct sock *nl_sk = NULL;
static struct netlink_kernel_cfg cfg = {0};

/////////////////////////////////////////	Message Struct

typedef struct{
	int	NodeID;
	int	N_Nodes;
	int	fileDesc;
	unsigned long COMEX_Address;
	unsigned long COMEX_Address_End;
	unsigned long Write_Buffer_Address;
	unsigned long Read_Buffer_Address;
	unsigned long MaxBuffer;
	unsigned long Comm_Buffer_Address;
}initStruct;

typedef struct{		//	Size8
	int target;		//	4
	int order;		//	4
}requestPageStruct;

typedef struct replyPagesQueue{		//	Size20
	int target, order, oriOrder;	//	4+4+4
	unsigned long offsetAddr;		//	8
} replyPagesDesc;

/////////////////////////////////////////

static void nl_recv_msg(struct sk_buff *skb)
{
	initStruct *myInit;
	requestPageStruct *myStruct11;
	replyPagesDesc *myStruct12;
	
	char *msgPointer;
	char cmdNumber;	
	struct nlmsghdr *nlh = (struct nlmsghdr *)skb->data;
	
	msgPointer = (char *)nlmsg_data(nlh);	
	cmdNumber = msgPointer[0];
//	printk(KERN_INFO "%s: cmdNumber %u\n", __FUNCTION__, cmdNumber);
	switch(cmdNumber){
		case 0:
			myInit = (initStruct *)(msgPointer+1);
			COMEX_PID = nlh->nlmsg_pid; 	/*pid of sending process, COMEX */
			
			printk(KERN_INFO "%s: NodeID %d N_Nodes %d \n", __FUNCTION__, myInit->NodeID, myInit->N_Nodes);
			printk(KERN_INFO "%s: COMEX_Adress %lu COMEX_End %lu \n", __FUNCTION__, myInit->COMEX_Address, myInit->COMEX_Address_End);
			printk(KERN_INFO "%s: Write_Buffer %lu Read_Buffer %lu MaxBuffer %lu \n", __FUNCTION__, myInit->Write_Buffer_Address, myInit->Read_Buffer_Address, myInit->MaxBuffer);
			printk(KERN_INFO "%s: Comm_Buffer %lu \n", __FUNCTION__, myInit->Comm_Buffer_Address);
			COMEX_init_ENV(	COMEX_PID, 
							myInit->NodeID, 
							myInit->N_Nodes, 
							myInit->COMEX_Address, 
							myInit->COMEX_Address_End, 
							myInit->Write_Buffer_Address, 
							myInit->Read_Buffer_Address,
							myInit->MaxBuffer,
							myInit->Comm_Buffer_Address,
							myInit->fileDesc);
			break;
			
		case 11:
			myStruct11 = (requestPageStruct *)(msgPointer+1);
//			printk(KERN_INFO "%s: requester %d Order %d\n", __FUNCTION__, myStruct11->target, myStruct11->order);
			COMEX_recv_asked(myStruct11->target, myStruct11->order);
			break;
			
		case 12:
			myStruct12 = (replyPagesDesc *)(msgPointer+1);
//			printk(KERN_INFO "%s: Source %d Offset %lu Order %d\n", __FUNCTION__, myStruct12->target, myStruct12->offsetAddr, myStruct12->order);
			COMEX_recv_fill(myStruct12->target, myStruct12->offsetAddr, myStruct12->order);
			break;
			
		case 127:			
			printk(KERN_INFO "%s: Test Message 127 !!!\n", __FUNCTION__);
			break;
			
		default:
			printk(KERN_INFO "%s: No case\n", __FUNCTION__);
	}
}

static int __init init_main(void)
{	
	printk(KERN_INFO "COMEX Kernel module V.0.1\n");
	COMEX_init_FS();
	cfg.input = nl_recv_msg;
//	nl_sk = netlink_kernel_create(&init_net, NETLINK_COMEX, 0, nl_recv_msg, NULL, THIS_MODULE);
	nl_sk = netlink_kernel_create(&init_net, NETLINK_COMEX, &cfg);
	if(nl_sk == NULL){
		printk(KERN_ALERT "Error creating socket.\n");
		return -1;
	}	
	return 0;
}

static void __exit cleanup_exit(void)
{
//	COMEX_Terminate();
	netlink_kernel_release(nl_sk);
	printk(KERN_INFO "Goodbye, world\n");
}

module_init(init_main);
module_exit(cleanup_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pisacha Srinuan");	/* Who wrote this module? */
MODULE_DESCRIPTION("COMEX Dev");	/* What does this module do */
MODULE_SUPPORTED_DEVICE("testdevice");
