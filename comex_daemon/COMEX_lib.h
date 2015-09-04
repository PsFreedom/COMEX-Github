unsigned long get_Param_from_Packet(char *message, int pos){
	int start=0, end=0;
	char msgBuff[256];

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
	
	return strtol(&msgBuff[start], &msgBuff[end], 10);
}

#ifdef IS_SERVER
void recv_request(int Requester, int Order);
void COMEX_server_cmd(char *message){

	int cmdNo, from, order;
	unsigned long offset;
	
	printf("   %s: %s\n", __FUNCTION__, message);
	cmdNo = (int)get_Param_from_Packet(message, 0);
	switch(cmdNo){
		case 1100:
			from = (int)get_Param_from_Packet(message, 1);
			order = (int)get_Param_from_Packet(message, 2);			
			
			recv_request(from ,order);
		break;
		case 1200:
			from = (int)get_Param_from_Packet(message, 1);
			offset = get_Param_from_Packet(message, 2);
			order = (int)get_Param_from_Packet(message, 3);			
			
			from = id2cbNum(from);
			from = 0;		// Fixxxxx
			fill_COMEX_freelist(from, offset, order);
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
