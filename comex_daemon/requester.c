#include<stdio.h>
#include<stdlib.h>
#include<inttypes.h>
#include<string.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/msg.h>
#include<sys/types.h>

#define SHMKEY 5683
#define BUF_LEN  2147483648

#define _derefInt(s) *( (int*)(s) )
#define _derefll(s) *( (long long*)(s) )

#define QTAIL bufferptr+4104

// bufferptr+4096=head of queue, we won't modify (8 bytes)
// bufferptr+4104=tail of queue, we'll increase it.(8 bytes)
// reql queue start at 4112 we'll increase until it's loop at 8192
int worksqueue[200];
char *bufferptr;

// for message queue
struct my_msgbuf {
    long mtype;
    int L;
};
struct my_msgbuf buf;
int msqid;
key_t key;

int redirect64(uint32_t highbitimm,uint32_t immediate){
    memcpy(bufferptr+_derefInt(QTAIL),&immediate,4);
    memcpy(bufferptr+_derefInt(QTAIL)+4,&highbitimm,4);
    printf("redirect print %llx to mem offset %d \n",_derefll(bufferptr+_derefInt(QTAIL)),_derefInt(QTAIL));
    *(QTAIL)+=8; // 8 char=64 bit
// mq send
    buf.L=*(QTAIL); // not used by now
    if (msgsnd(msqid, &buf,sizeof(long), 0) == -1){
        perror("msgsnd");
    }
//
    if(_derefInt(QTAIL)>=8192){
        printf("Buffer Loop\n");
        *(QTAIL)=4112;
    }
    return 0;
}

int init_mq_sender(){
    if ((key = ftok("rdma_both.c", 'B')) == -1) {
        perror("ftok");
        exit(1);
    }
    if ((msqid = msgget(key, 0644| IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
        }
return 0;
}


int main(){
    int shmid;
    char cmd='9',expectedcmd='0';
	if ((shmid = shmget(SHMKEY, BUF_LEN, 0666)) < 0) {
       		perror("shmget");
        	exit(1);
    	}
    	if ((bufferptr = shmat(shmid, NULL, 0)) == (char *) -1) {
        	perror("shmat");
        	exit(1);
    	}
    //
    memset(bufferptr+4096,0,4096); // clear all
    *((int*)(bufferptr+4096))=4112; //head position
    *((int*)(bufferptr+4104))=4112; //tail position
    //

    int place=0;
    int sent=0;
    int tosend=30;
	int keepplace[100];
	int keepInst[100];
	int randsize=rand()%9+1;
    int i=0;
    int location=0;
    int hPID=128; //?
    init_mq_sender();
    while(sent+randsize<tosend){
        location=randsize; //don't know yet, where to put
        //should also fill the location, the ID,TTL will be filled by client
        redirect64(1,(randsize<<13)|((hPID%256)<<24));
        while(cmd!=expectedcmd){
            printf("\nsent %d pages continue press:%c",randsize,expectedcmd);
            scanf("%c",&cmd);
        }
        i++;
        randsize=rand()%9+1;
        expectedcmd=(expectedcmd-'0'+1)%10+'0';
        hPID++;
        hPID%=256;
    }
    if(sent<tosend){
        //pushpage(cb[i%cbcount],tosend-sent,randsize,&(keepInst[i]),&(keepplace[i]));
        redirect64(1,(tosend-sent)<<13|((hPID%256)<<24));
        i++;
    }
    // terminate socket (disabled)
    /*
    if (msgctl(msqid, IPC_RMID, NULL) == -1) {
        perror("msgctl");
        exit(1);
    }
    */
    return 0;
}
