#define PAGE_SIZE 4096
#define MAX_BUFFER 256
#define COMM_BUFFER 16384

typedef struct{
	int	NodeID;
	int	totalCB;
}CommStruct;
CommStruct *myCommStruct = NULL;

unsigned long BUF_LEN;
char *COMEX_Area = NULL;

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

typedef struct replyPagesQueue{		//	Size28
	int target, order, oriOrder;	//	4+4+4
	unsigned long offsetAddr;		//	8
} replyPagesDesc;

typedef struct Remote_buffer_descriptor{	//For user
	int nodeID, buffIDX;
	unsigned long l_Offset;
	unsigned long r_Offset;
} BufferDescUser;

/////////////////////////////////////////

void checkSumPage(unsigned long startOffset){
	unsigned long i, checkSum=0;
	
	for(i=startOffset; i<startOffset+4096; i++){
		checkSum += COMEX_Area[i];
	}
	printf("		Page checkSum = %lu\n", checkSum);
}

void checkSumArea(){
	unsigned long i, checkSum=0;
	
	for(i=0; i<BUF_LEN - (4096*MAX_BUFFER); i++){
		checkSum += COMEX_Area[i];
	}
	printf("		Area checkSum = %lu\n", checkSum);
}

void checkSumBuffer(){
	unsigned long i, checkSum=0;
	
	for(i = BUF_LEN-(4096*MAX_BUFFER); i<BUF_LEN; i++){
		checkSum += COMEX_Area[i];
	}
	printf("		Buffer checkSum = %lu\n", checkSum);
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