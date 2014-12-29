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

#define NETLINK_USER 31

struct sock *nl_sk = NULL;
unsigned int COMEX_PID = 0;

static void prepare_COMEX_shrink_page_list(unsigned long COMEX_Address,unsigned long COMEX_Address_End){
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *ptep, pte;
	spinlock_t *ptl;
	struct page *page;
	struct mm_struct *mm;
	struct vm_area_struct *vma;
	struct task_struct *COMEX_task_struct;
	int foundVMA = 0;
	
	LIST_HEAD(page_list);
	
	COMEX_task_struct = pid_task(find_vpid(COMEX_PID), PIDTYPE_PID);
	mm = COMEX_task_struct->mm;
	vma = mm->mmap;
	
	do{
		printk(KERN_INFO "VMA %lu - %lu", vma->vm_start, vma->vm_end);
		if(COMEX_Address >= vma->vm_start && COMEX_Address <= vma->vm_end){
			foundVMA = 1;
			printk(KERN_INFO "This VMA\n");
		}
		else{
			vma = vma->vm_next;
		}
	}
	while(vma != NULL && foundVMA == 0);
	
	
	while(COMEX_Address <=  COMEX_Address_End){	
		page = NULL;
		
		pgd = pgd_offset(mm, COMEX_Address);
		if (pgd_none(*pgd) || pgd_bad(*pgd)){
			printk(KERN_INFO "PGD bug\n");
		}
	
		pud = pud_offset(pgd, COMEX_Address);
		if (pud_none(*pud) || pud_bad(*pud)){
			printk(KERN_INFO "PUD bug\n");
		}

		pmd = pmd_offset(pud, COMEX_Address);
		if (pmd_none(*pmd) || pmd_bad(*pmd)){
			printk(KERN_INFO "PMD bug\n");
		}
		
		ptep = pte_offset_map_lock(mm, pmd, COMEX_Address, &ptl);
		pte = *ptep;
		page = pte_page(pte);
		pte_unmap_unlock(ptep, ptl);
		
		if(pte_present(pte) == 1){
			list_move(&page->lru, &page_list);
			printk(KERN_INFO "COMEX kernel module: pte_present(pte) = %d\n", pte_present(pte));
		}
		else{
			printk(KERN_INFO "COMEX kernel module: pte_present(pte) = %d\n", pte_present(pte));
		}		
		COMEX_Address += 4096;
	}
	COMEX_shrink_page_list(&page_list, vma);
}

unsigned long getParamFromPacketData(struct sk_buff *skb, int Position){
	struct nlmsghdr *nlh = (struct nlmsghdr *)skb->data;
	char myMessage[50];
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
		if(myMessage[i] == ' '){
			Position--;
			if(Position == 0){
				last = i-1;
			}
		}
		i++;
	}	
	return simple_strtoul(&myMessage[first], &myMessage[last], 10);
}

static void nl_recv_msg(struct sk_buff *skb)
{
	int Order;
	unsigned long cmdNumber, COMEX_Address ,COMEX_Address_End;
	struct nlmsghdr *nlh = (struct nlmsghdr *)skb->data;
	
	printk(KERN_INFO "%s: %s\n", __FUNCTION__, (char *)nlmsg_data(nlh));
	cmdNumber = getParamFromPacketData(skb, 0);	
	switch(cmdNumber){
		case 0:
			COMEX_PID = nlh->nlmsg_pid; 								/*pid of sending process, COMEX */
			COMEX_init_ENV((unsigned int)COMEX_PID);
			printk(KERN_INFO "%s: Finish Initialize\n", __FUNCTION__);
			break;
			
		case 100:
			COMEX_Address = getParamFromPacketData(skb, 1);
			COMEX_Address_End = getParamFromPacketData(skb, 2);
			
			printk(KERN_INFO "%s: Command %lu Start = %lu End = %lu\n", __FUNCTION__, cmdNumber, COMEX_Address, COMEX_Address_End);
			prepare_COMEX_shrink_page_list(COMEX_Address, COMEX_Address_End);
			break;
			
		case 200:
			COMEX_Address = getParamFromPacketData(skb, 1);
			Order = getParamFromPacketData(skb, 2);
			
			COMEX_write_to_COMEX_area(COMEX_Address, Order);
			break;
			
		default:
			printk(KERN_INFO "%s: No case\n", __FUNCTION__);
	}
}

static int __init init_main(void)
{	
	printk(KERN_INFO "COMEX Kernel module V.0.1\n");
	nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, 0, nl_recv_msg, NULL, THIS_MODULE);
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
