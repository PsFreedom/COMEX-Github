#define PAGE_SIZE 4096
#define MAX_BUFFER (256*totalCB)
#define COMM_BUFFER 4096

typedef struct{
	int	NodeID;
	int	totalCB;
	unsigned long Write_Buffer_Offset;
	unsigned long Read_Buffer_Offset;
}CommStruct;
CommStruct *myCommStruct = NULL;

int totalCB;
unsigned long BUF_LEN;
char *COMEX_Area = NULL;

/////////////////////////////////////////	Message Struct

struct list_head {
	struct list_head *next, *prev;
};

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

typedef struct replyPagesQueue{		//	Size28
	int target, order, oriOrder;	//	4+4+4
	unsigned long offsetAddr;		//	8
} replyPagesDesc;

typedef struct Remote_buffer_descriptor{	//For user
	struct list_head link;
	int nodeID, buffIDX;
	unsigned long l_Offset;
	unsigned long r_Offset;
} BufferDescUser;

typedef struct pageFaultDescriptor{
	struct list_head link;
	int nodeID;
	unsigned long R_offset;
	unsigned long L_offset;
} PF_Desc;

/////////////////////////////////////////

void checkSumPage(char *startAddr){
	unsigned long checkSum=0, start, end;
	
	start = (startAddr - COMEX_Area)/sizeof(char);
	end   = (startAddr - COMEX_Area + 4096)/sizeof(char);
	
	while(start < end){
		checkSum += COMEX_Area[start];
		start++;
	}
	
	printf("	Page checkSum = %lu\n", checkSum);
}

void checkSumArea(){
	unsigned long i, checkSum=0;
	
	for(i=0; i<BUF_LEN - (4096*MAX_BUFFER); i++){
		checkSum += COMEX_Area[i];
	}
	printf("	Area checkSum = %lu\n", checkSum);
}

void checkSumBuffer(){
	unsigned long i, checkSum=0;
	
	for(i = BUF_LEN-(4096*MAX_BUFFER); i<BUF_LEN; i++){
		checkSum += COMEX_Area[i];
	}
	printf("	Buffer checkSum = %lu\n", checkSum);
}

void checkSumAll(){
	unsigned long i, checkSum=0;
	
	for(i=0; i<BUF_LEN; i++){
		checkSum += COMEX_Area[i];
	}
	printf("		All checkSum = %lu\n", checkSum);
}

#ifdef IS_SERVER
void recv_request(int Requester, int Order);
void fill_COMEX_freelist(int remoteID, unsigned long offset, int order);

void COMEX_server_msg(char *message){
	printf("%s: %s\n", __FUNCTION__, message);
}
#endif