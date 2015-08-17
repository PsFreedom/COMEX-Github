/*  
 *  hello-4.c - Demonstrates module documentation.
 */
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

unsigned long getParamFromPacketData(struct sk_buff *skb, int Position){
	struct nlmsghdr *nlh = (struct nlmsghdr *)skb->data;
	char myMessage[256];
	int i=0, first=0, last=0;
	
	strcpy(myMessage, (char *)nlmsg_data(nlh));		
	while(Position > 0){
		if(myMessage[i] == ' '){
			Position--;
			if(Position == 0){
				first = i+1;
			}
		}
		i++;
	}	
	while(Position == 0){
		if((myMessage[i] == ' ') || (myMessage[i] == '\0')){
			Position--;
			last = i-1;
		}
		i++;
	}	
	return simple_strtoul(&myMessage[first], &myMessage[last], 10);
}

static void nl_recv_msg(struct sk_buff *skb)
{
	unsigned long cmdNumber;
	unsigned long NodeID, N_Nodes, COMEX_Address, COMEX_Address_End, Buffer_Address, MaxBuffer;
	unsigned long RemoteID, RemoteAddr, nPages;
	unsigned long requestOrder;
	
	struct nlmsghdr *nlh = (struct nlmsghdr *)skb->data;
	
	cmdNumber = getParamFromPacketData(skb, 0);	
	switch(cmdNumber){
		case 0:
			COMEX_PID = nlh->nlmsg_pid; 	/*pid of sending process, COMEX */
		
			NodeID = getParamFromPacketData(skb, 1);
			N_Nodes = getParamFromPacketData(skb, 2);
			COMEX_Address = getParamFromPacketData(skb, 3);
			COMEX_Address_End = getParamFromPacketData(skb, 4);
			Buffer_Address = getParamFromPacketData(skb, 5);
			MaxBuffer = getParamFromPacketData(skb, 6);
			
			printk(KERN_INFO "%s: NodeID %lu N_Nodes %lu", __FUNCTION__, NodeID, N_Nodes);
			printk(KERN_INFO "%s: COMEX_Adress %lu COMEX_End %lu", __FUNCTION__, COMEX_Address, COMEX_Address_End);
			printk(KERN_INFO "%s: Buffer_Address %lu MaxBuffer %lu", __FUNCTION__, Buffer_Address, MaxBuffer);
			COMEX_init_ENV((int)COMEX_PID, (int)NodeID, (int)N_Nodes, 
							COMEX_Address, COMEX_Address_End, Buffer_Address, (int)MaxBuffer);
			break;
			
		case 100:
			RemoteID = getParamFromPacketData(skb, 1);
			RemoteAddr = getParamFromPacketData(skb, 2);
			nPages = getParamFromPacketData(skb, 3);
			
			printk(KERN_INFO "%s: RemoteID %lu ", __FUNCTION__, RemoteID);
			printk(KERN_INFO "%s: RemoteAddr %lu nPages %lu", __FUNCTION__, RemoteAddr, nPages);
			
			COMEX_recv_fill((int)RemoteID, RemoteAddr, (int)nPages);
			break;
			
		case 200:
			requestOrder = getParamFromPacketData(skb, 1);
			
			COMEX_recv_asked((int)requestOrder);
			break;
			
		default:
			printk(KERN_INFO "%s: No case\n", __FUNCTION__);
	}
}

static int __init init_main(void)
{	
	printk(KERN_INFO "COMEX Kernel module V.0.1\n");
	nl_sk = netlink_kernel_create(&init_net, NETLINK_COMEX, 0, nl_recv_msg, NULL, THIS_MODULE);
	if(nl_sk == NULL){
		printk(KERN_ALERT "Error creating socket.\n");
		return -1;
	}	
	return 0;
}

static void __exit cleanup_exit(void)
{
	COMEX_Terminate();
	netlink_kernel_release(nl_sk);
	printk(KERN_INFO "Goodbye, world\n");
}

module_init(init_main);
module_exit(cleanup_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Pisacha Srinuan");	/* Who wrote this module? */
MODULE_DESCRIPTION("COMEX Dev");	/* What does this module do */
MODULE_SUPPORTED_DEVICE("testdevice");
