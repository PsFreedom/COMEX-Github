#define MAX_BUFFER 64
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
	unsigned long COMEX_Address;
	unsigned long COMEX_Address_End;
	unsigned long Write_Buffer_Address;
	unsigned long Read_Buffer_Address;
	unsigned long MaxBuffer;
	unsigned long Comm_Buffer_Address;
}initStruct;

typedef struct{
	int Requester;
	int Order;
}request1Struct;

typedef struct{
	int remoteID;
	unsigned long offset;
	int order;
}fill2Struct;

/////////////////////////////////////////

unsigned long get_Param_from_Packet(char *message, int pos){
	int start=0, end=0;
	char msgBuff[200];

	strcpy(msgBuff, message);
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
	
	return strtoul(&msgBuff[start], &msgBuff[end], 10);
}

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
void COMEX_server_cmd(char *message){

	int cmdNo, from, order;
	unsigned long offset;
	
//	printf("   %s: %s\n", __FUNCTION__, message);
	cmdNo = (int)get_Param_from_Packet(message, 0);
	switch(cmdNo){
		case 1100:	// Get request
			from = (int)get_Param_from_Packet(message, 1);
			order = (int)get_Param_from_Packet(message, 2);			
			
			recv_request(from ,order);
			break;
		case 1200:	// Recieve pages
			from = (int)get_Param_from_Packet(message, 1);
			offset = get_Param_from_Packet(message, 2);
			order = (int)get_Param_from_Packet(message, 3);			
			
			from = id2cbNum(from);
			fill_COMEX_freelist(from, offset, order);
			break;
		case 9000:	// Debug checksum all area
			printf("   Debug: This is debug message\n", cmdNo, message);
			checkSumArea();
			break;
		case 9100:	// Debug checksum page
			offset = (int)get_Param_from_Packet(message, 1);
		
			printf("   Debug: This is debug message\n", cmdNo, message);
			checkSumPage(offset);
			break;
		default:
			printf("   default: %d - %s\n", cmdNo, message);
			break;
	}
}
void COMEX_server_msg(char *message){
	printf("%s: %s\n", __FUNCTION__, message);
}
#endif