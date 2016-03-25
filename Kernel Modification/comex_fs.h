#include "comex_structure.h"	// add for COMEX

#ifndef VM_RESERVED
# define  VM_RESERVED   (VM_DONTEXPAND | VM_DONTDUMP)
#endif

struct dentry  *file1, *file2, *file3, *dir;

struct mmap_info {
	char *data;			/* the data */
	int reference;       /* how many times it is mmapped */  	
};

/* keep track of how many times it is mmapped */
void mmap_open(struct vm_area_struct *vma)
{
	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
	info->reference++;
}

void mmap_close(struct vm_area_struct *vma)
{
	struct mmap_info *info = (struct mmap_info *)vma->vm_private_data;
	info->reference--;
}

/* nopage is called the first time a memory area is accessed which is not in memory,
 * it does the actual mapping between kernel and user space memory
 */
static int mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	struct mmap_info *info;
	
	/* the data is in vma->vm_private_data */
	info = (struct mmap_info *)vma->vm_private_data;
	if (!info->data) {
		printk("no data\n");
		return NULL;	
	}

	/* get the page */
	page = virt_to_page(info->data);
	
	/* increment the reference count of this page */
	get_page(page);
	vmf->page = page;
	return 0;
}


struct vm_operations_struct mmap_vm_ops = {
	.open =     mmap_open,
	.close =    mmap_close,
	.fault =    mmap_fault,
};

int my_mmap(struct file *filp, struct vm_area_struct *vma)
{
	vma->vm_ops = &mmap_vm_ops;
	vma->vm_flags |= VM_RESERVED;
	/* assign the file private data to the vm private data */
	vma->vm_private_data = filp->private_data;
	mmap_open(vma);
	return 0;
}

int my_close(struct inode *inode, struct file *filp)
{
	struct mmap_info *info = filp->private_data;
	
	/* obtain new memory */
	free_page((unsigned long)info->data);
    kfree(info);
	filp->private_data = NULL;
	return 0;
}

int my_close_pageFault(struct inode *inode, struct file *filp)
{
	struct mmap_info *info = filp->private_data;
	
	up(&COMEX_ReadBack_FlowLock);
	
	/* obtain new memory */
	free_page((unsigned long)info->data);
    kfree(info);
	filp->private_data = NULL;
	return 0;
}

int my_open_givePages(struct inode *inode, struct file *filp)
{
	struct mmap_info *info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);
	
	/* obtain new memory */
    info->data = (char *)get_zeroed_page(GFP_KERNEL);
		
	memcpy(info->data, &replyPagesQ[replyPagesQReader], sizeof(replyPagesDesc));
	replyPagesQReader = (replyPagesQReader+1)%(COMEX_Total_Nodes*MAX_RQ);
	
	/* assign this info struct to the file */
	filp->private_data = info;
	return 0;
}

int my_open_RDMA_write(struct inode *inode, struct file *filp)
{
	struct mmap_info *info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);
	
	int i;
	BufferDescUser *myBufferDescUser;

	/* obtain new memory */
    info->data = (char *)get_zeroed_page(GFP_KERNEL);
	
	down_interruptible(&COMEX_Remote_MUTEX);

	while(list_empty(&RDMA_qHead[i])){
		i++;
	}	
	myBufferDescUser = list_first_entry(&RDMA_qHead[i], BufferDescUser, link);
	memcpy(info->data, myBufferDescUser, sizeof(BufferDescUser));
	
//	printk(KERN_INFO "%d %d \n", i, myBufferDescUser->buffIDX);
	bufferDesc[i][myBufferDescUser->buffIDX].isFree = 1;
	list_del(&myBufferDescUser->link);
//	RDMA_write_signal = 1;	

	up(&COMEX_Remote_MUTEX);
	
	/* assign this info struct to the file */
	filp->private_data = info;
	return 0;
}

int my_open_pageFault(struct inode *inode, struct file *filp)
{
	struct mmap_info *info = kmalloc(sizeof(struct mmap_info), GFP_KERNEL);
	
	PF_Desc *myPF_Desc;
	int i=0;
	
	/* obtain new memory */
    info->data = (char *)get_zeroed_page(GFP_KERNEL);

	while(list_empty(&PF_head[i]))
		i++;	
	myPF_Desc = list_first_entry(&PF_head[i], PF_Desc, link);
	memcpy(info->data, myPF_Desc, sizeof(PF_Desc));
	
	list_del(&myPF_Desc->link);
	
	/* assign this info struct to the file */
	filp->private_data = info;
	return 0;
}

static const struct file_operations my_fops_givePages = {
	.open = my_open_givePages,
	.release = my_close,
	.mmap = my_mmap,
};
static const struct file_operations my_fops_RDMA_write = {
	.open = my_open_RDMA_write,
	.release = my_close,
	.mmap = my_mmap,
};
static const struct file_operations my_fops_pageFault = {
	.open = my_open_pageFault,
	.release = my_close_pageFault,
	.mmap = my_mmap,
};
