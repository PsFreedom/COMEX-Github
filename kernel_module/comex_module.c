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
static struct netlink_kernel_cfg cfg = {0};

unsigned long getParamFromPacketData(struct sk_buff *skb, int pos){
	struct nlmsghdr *nlh = (struct nlmsghdr *)skb->data;
	int start=0, end=0;
	char msgBuff[256];

	strcpy(msgBuff, (char *)nlmsg_data(nlh));
	while(pos > 0){
		if(msgBuff[start] == ' '){
			pos--;
		}
		start++;
	}
	end = start+1;
	while(msgBuff[end] != ' ' && msgBuff[end] != '\0'){
		end++;
	}
	end--;
	
	return simple_strtoul(&msgBuff[start], &msgBuff[end], 10);
}

static void nl_recv_msg(struct sk_buff *skb)
{
	unsigned long cmdNumber;
	unsigned long NodeID, N_Nodes, COMEX_Address, COMEX_Address_End, Write_Buffer_Address, Read_Buffer_Address, MaxBuffer;	// for case 0: 
	unsigned long RemoteID, Offset, nPages;		// for case 100:
	unsigned long requester, Order;	// for case 1100:
	
	struct nlmsghdr *nlh = (struct nlmsghdr *)skb->data;
	
	cmdNumber = getParamFromPacketData(skb, 0);	
	switch(cmdNumber){
		case 0:
			COMEX_PID = nlh->nlmsg_pid; 	/*pid of sending process, COMEX */
		
			NodeID = getParamFromPacketData(skb, 1);
			N_Nodes = getParamFromPacketData(skb, 2);
			COMEX_Address = getParamFromPacketData(skb, 3);
			COMEX_Address_End = getParamFromPacketData(skb, 4);
			Write_Buffer_Address = getParamFromPacketData(skb, 5);
			Read_Buffer_Address = getParamFromPacketData(skb, 6);
			MaxBuffer = getParamFromPacketData(skb, 7);
			
			printk(KERN_INFO "%s: NodeID %lu N_Nodes %lu \n", __FUNCTION__, NodeID, N_Nodes);
			printk(KERN_INFO "%s: COMEX_Adress %lu COMEX_End %lu \n", __FUNCTION__, COMEX_Address, COMEX_Address_End);
			printk(KERN_INFO "%s: Write_Buffer_Address %lu Read_Buffer_Address %lu MaxBuffer %lu \n", __FUNCTION__, Write_Buffer_Address, Read_Buffer_Address, MaxBuffer);
			COMEX_init_ENV((int)COMEX_PID, (int)NodeID, (int)N_Nodes, 
							COMEX_Address, COMEX_Address_End, Write_Buffer_Address, Read_Buffer_Address,(int)MaxBuffer);
			break;
			
		case 1100:
			requester = getParamFromPacketData(skb, 1);
			Order = getParamFromPacketData(skb, 2);
			
//			printk(KERN_INFO "%s: requester %lu Order %lu\n", __FUNCTION__, requester, Order);
			COMEX_recv_asked((int)requester, (int)Order);
			break;
			
		case 1200:
			RemoteID = getParamFromPacketData(skb, 1);
			Offset = getParamFromPacketData(skb, 2);
			nPages = getParamFromPacketData(skb, 3);
			
//			printk(KERN_INFO "%s: RemoteID %lu \n", __FUNCTION__, RemoteID);
//			printk(KERN_INFO "%s: Offset %lu nPages %lu\n", __FUNCTION__, Offset, nPages);
			COMEX_recv_fill((int)RemoteID, Offset, (int)nPages);
			break;
			
		default:
			printk(KERN_INFO "%s: No case\n", __FUNCTION__);
	}
}

static int __init init_main(void)
{	
	printk(KERN_INFO "COMEX Kernel module V.0.1\n");
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
