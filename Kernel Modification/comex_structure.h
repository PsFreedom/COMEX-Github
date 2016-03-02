////////// COMEX //////////

#include <linux/netlink.h>	// add for COMEX
#include <linux/hugetlb.h> 	// add for COMEX
#include <linux/kernel.h>	// add for COMEX
#include <linux/fs.h>		// add for COMEX
#include <linux/debugfs.h>	// add for COMEX
#include <linux/slab.h>		// add for COMEX
#include <asm/siginfo.h>	//siginfo
#include <linux/rcupdate.h>	//rcu_read_lock
#include <linux/sched.h>	//find_task_by_pid_type
#include <linux/uaccess.h>	// add for COMEX
#include <net/sock.h>			// Netlink Socket
#include <linux/skbuff.h>		// Netlink Socket

#define SIG_TEST 44 /* we define our own signal, hard coded since SIGRTMIN is different in user and in kernel space */ 
#define X86PageSize 4096
#define COMEX_MAX_ORDER 11
#define Biggest_Group 1024
#define JumpThreshold 5
#define MAX_RQ	16

//////////////////// COMEX Global Variables ////////////////////

int COMEX_PID = 0;
int Listener_PID = 0;
unsigned int COMEX_Ready = 0;
unsigned int Daemon_Ready = 0;

struct task_struct *COMEX_task_struct;
struct mm_struct *COMEX_mm;
struct vm_area_struct *COMEX_vma;

static struct netlink_kernel_cfg cfg = {0};
struct sock *nl_sk = NULL;
struct nlmsghdr *nlh;

unsigned int totalLookUPEntry = 0;
unsigned long COMEX_start_addr;
unsigned long COMEX_Comm_addr;
unsigned long *comexLookUP;

static spinlock_t COMEX_Buddy_lock;
static spinlock_t COMEX_Remote_lock;

atomic_t ShrinkPL_counter;
struct semaphore COMEX_Remote_MUTEX;
struct semaphore COMEX_ReadBack_MUTEX;

void print_free_blocks(void);

//////////////////// COMEX Remote ////////////////////

int COMEX_Node_ID = 0;
int COMEX_Total_Nodes = 0;
int COMEX_MAX_Buffer = 0;

unsigned long COMEX_Write_buffer_addr;
unsigned long COMEX_Read_buffer_addr;

unsigned long Head_R_offset;
unsigned long Prev_R_offset = 0;
unsigned long RDMA_jiffies = 0;
unsigned long Request_jiffies = 0;

void NL_send_message(char *NetlinkMSG);
int powOrder(int Order);
int get_Frist_PID(struct page *page);
unsigned long COMEX_get_from_Buddy(int order);

//////////////////// COMEX Buddy system Structure ////////////////////

struct COMEX_free_area {
	struct list_head	free_list[1];
	unsigned long		nr_free;
};

typedef struct Dummy_zone {
	struct COMEX_free_area free_area[COMEX_MAX_ORDER];
} COMEX_Zone;
COMEX_Zone *COMEX_Buddy_Zone;

typedef struct Dummy_page {
	unsigned long pageNO;
	atomic_t _count;
	atomic_t _mapcount;
	unsigned long private;
	struct list_head lru;
	
	bool isRemote;
	struct page *pageDesc;
	
} COMEX_page;
COMEX_page *COMEX_Buddy_page;

//////////////////// COMEX Remote list Structure ////////////////////

typedef struct COMEX_remote_list_header{
	int RQcounter;
	int totalGroup;
} COMEX_R_Header;
COMEX_R_Header *COMEX_R_Freelist;

typedef struct COMEX_remote_page_desc{
//	int R_NodeID;
	unsigned long Offset_start;
	unsigned long Offset_end;
} COMEX_R_page_group;
COMEX_R_page_group **page_groups;
int *page_groupsIDX_Fill;
int *page_groupsIDX_Use;

typedef struct Remote_buffer_descriptor{	//For user
	int nodeID, buffIDX;
	unsigned long l_Offset;
	unsigned long r_Offset;
} BufferDescUser;
BufferDescUser *RDMA_writeQ;

int bufferIDX = 0;
int bufferIDXUser = 0;

typedef struct replyPagesQueue{		//For user
	int target, order, oriOrder;
	unsigned long offsetAddr;
} replyPagesDesc;
replyPagesDesc *replyPagesQ;
int replyPagesQCounter = 0;
int replyPagesQReader = 0;

typedef struct COMEX_buffer_descriptor{
	int isFree;
	struct page *pageDesc;
	unsigned long Offset;
} COMEXbuffer;
COMEXbuffer *bufferDesc;