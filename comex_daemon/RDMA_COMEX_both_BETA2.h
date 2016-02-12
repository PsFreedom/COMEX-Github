//#include <stdio.h>
//#include <stdlib.h>
//#include <string.h>
#include <errno.h>
//#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <infiniband/verbs.h>
#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h> // message queue
#include <unistd.h>

#include "IP_Port.h"
#include "COMEX_lib.h"

/*
 * Defines
 */

#define CQ_DEPTH 256
#define MAX_SEND_WR 16
//default 4
#define MAX_SEND_SGE 16

#define MAX_RECV_WR	16
//default 4
#define MAX_RECV_SGE 16

#define SHMKEY 5683
#define SHAREDMEM 1

#define PORTOFFSET 62000

//#define VERBOSE 1
int work=1; // if set=0, init then  end, don't do the workload.
int myinstanceNo;
struct buffer {
	uint64_t addr;
	uint32_t length;
	uint32_t rkey;
	char piggy[64];
};


// for message queue
struct my_msgbuf {
    long mtype;
    int L;
};
struct my_msgbuf buf;
int msqid;
key_t key;

/*
 * Control Block used to store all data structures and context information.
 */
struct rdma_cb {

/*
 * Command line args
 */
	int is_server;
	struct sockaddr_in sin;
    int instanceNo;
/*
 * RDMA CM data structures
 */
	struct rdma_event_channel *event_channel;
	struct rdma_cm_id *cm_id;
	struct rdma_cm_id *listen_cm_id;		/* Server only */

/*
 * IB data structures
 */
	struct ibv_comp_channel *comp_channel;
	struct ibv_cq *cq;
	struct ibv_pd *pd;
	struct ibv_qp *qp;

/*
 * Memory Region and data buffers
 */
	struct ibv_mr *recv_mr;
	struct buffer recv_buffer;

	struct ibv_mr *send_mr;
	struct buffer send_buffer;

	struct ibv_mr *rdma_mr;
	char *rdma_buffer;
	long long rdma_buf_len;
	int rdma_packet_len;

	struct buffer remote_buffer;
};

uint32_t qps[16];
struct rdma_cb* cbs[16];
int qpcount=0;
char SEND_STR[65]="Hello World!";
    void* bug;
struct rdma_cb* qp2cb(uint64_t qp){
	int i;
	for(i=0;i<qpcount;i++){
		if(qps[i]==qp){
			return cbs[i];
		}
	}
    printf("Alert: unknown queue pair detected\n");
    return -1;
}
struct rdma_cb* id2cb(int id){
	int i;
	for(i=0;i<qpcount;i++){
		if(cbs[i]->instanceNo==id){
			return cbs[i];
		}
	}
    printf("Alert: unknown id detected\n");
//	assert(1);
    return -1;
}

int id2cbNum(int id){
	int i;
	for(i=0;i<qpcount;i++){
//		printf("ID: %d\n", cbs[i]->instanceNo);
		if(cbs[i]->instanceNo==id){
			return i;
		}
	}
    printf("Alert: unknown id detected\n");
//	assert(1);
    return -1;
}
/*
 * Function Prototypes
 */
int setup_common(struct rdma_cb *);
void cleanup_common(struct rdma_cb *);
int do_server(struct rdma_cb *[],int);
int do_client(struct rdma_cb *[],int);
int do_recv(struct rdma_cb *);
int do_poll(struct ibv_cq *,int *,int *);
int process_rdma_cm_event(struct rdma_cb *, enum rdma_cm_event_type);

int
do_poll(struct ibv_cq *cq,int* retval,int *retqpno) {

	struct ibv_wc wc;
	int err;

	while ((err = ibv_poll_cq(cq, 1, &wc)) == 1) { // http://www.rdmamojo.com/2013/02/15/ibv_poll_cq/

		if (wc.status) {
			fprintf(stderr, "Completion failed for event: %d, status: %d\n", wc.opcode, wc.status);
			return -1;
		}

		*retqpno=wc.qp_num;
		struct rdma_cb *retcb=qp2cb(wc.qp_num);
        //fprintf(stdout, "localqp=%d  qpnum=%d wrid=%d.\n",wc.qp_num,cb->qp->qp_num,wc.wr_id);
        //fprintf(stdout, "argcb=%x  stored=%x\n",cb,qp2cb(wc.qp_num));
        switch (wc.opcode) {
		case IBV_WC_SEND:
		    #ifdef VERBOSE
			fprintf(stdout, "Send completed successfully.\n");
			#endif
			break;

		case IBV_WC_RDMA_READ:
			#ifdef VERBOSE
			fprintf(stdout, "RDMA READ completed successfully.\n");
			#endif
        break;
        case IBV_WC_RDMA_WRITE:
			 #ifdef VERBOSE
             fprintf(stdout, "RDMA Write completed successfully.\n");
             #endif
        break;
		case IBV_WC_RECV:
			#ifdef VERBOSE
			fprintf(stdout, "Recv completed successfully.\n");
            #endif
         if ((wc.wc_flags & IBV_WC_WITH_IMM) == 0){
        /*
         * Copy out the buffer information., if the request doesn't have imm
         */
            #ifdef VERBOSE
            fprintf(stdout, "get IBV_WC_RECV with no imm\n");
            #endif
            retcb->remote_buffer.addr = ntohll(retcb->recv_buffer.addr);
            retcb->remote_buffer.length  = ntohl(retcb->recv_buffer.length);
            retcb->remote_buffer.rkey = ntohl(retcb->recv_buffer.rkey);
            retcb->instanceNo=atoi(retcb->recv_buffer.piggy);
            fprintf(stdout, "Received RDMA Buffer information: addr: 0x%llx, length: %" PRIu32 ", rkey: 0x%x\n",
                (unsigned long long)retcb->remote_buffer.addr, retcb->remote_buffer.length, retcb->remote_buffer.rkey);
            printf("InstanceNo=%d\n",retcb->instanceNo);
        }else{
            *retval=wc.imm_data;
            #ifdef VERBOSE
            fprintf(stdout, "Recv but has imm\n");
            fprintf(stdout, "%s",(retcb->recv_buffer.piggy));
            #endif
        }
        /*
         * Repost our RECV Buffer.
         */
        err = do_recv(retcb);
        if (err) {
            return -1;
        }
        break;
        case IBV_WC_RECV_RDMA_WITH_IMM:
        /*
         * check for immediate data.
         */
            if ((wc.wc_flags & IBV_WC_WITH_IMM) == IBV_WC_WITH_IMM) {
            #ifdef VERBOSE
                fprintf(stdout, "Received immediate data: 0x%x\n", wc.imm_data);
            #endif
			*retval=wc.imm_data;
			}
            /*
             * Repost our RECV Buffer.
             */
                        err = do_recv(retcb);
                        if (err) {
                                return -1;
                        }
        break;
		default:
			fprintf(stderr, "Received unhandled completion type: %d\n", wc.opcode);
			return -1;
		}
	}

	return 0;
}

int
do_completion(struct rdma_cb *cb,int *retimm,int *retqpno) {

	struct ibv_cq *event_cq;
	void *event_context;
	int err;

/*
 * Block until we get a completion event.
 */
	err = ibv_get_cq_event(cb->comp_channel, &event_cq, &event_context);
	if (err) {
		fprintf(stderr, "Failed getting CQ event: %d\n", err);
		return -1;
	}

/*
 * Need to rerequest notification so we will get an event the next
 * time a WR completes.
 */
	err = ibv_req_notify_cq(cb->cq, 0);
	if (err) {
                fprintf(stderr, "Failed to request notification: %d\n", err);
		return -1;
	}
/*
 * Process the Completion event.
 */
	err = do_poll(cb->cq,retimm,retqpno);
	if (err) {
		return -1;
	}

/*
 * Ack the CQ Event
 */
	ibv_ack_cq_events(cb->cq, 1);

	return 0;
}
int create_cccq(struct rdma_cb *cb){
/*
 * Create a Completion Channel
 */
    cb->comp_channel=ibv_create_comp_channel(cb->cm_id->verbs); //http://www.rdmamojo.com/2012/10/19/ibv_create_comp_channel/;
    if (!cb->comp_channel) {
		fprintf(stderr, "Error Creating a Completion Channel\n");
		return -1;
	}
/*
 * Create a Completion Queue
 */
    cb->cq=ibv_create_cq(cb->cm_id->verbs, CQ_DEPTH, cb,cb->comp_channel , 0); //http://www.rdmamojo.com/2012/11/03/ibv_create_cq/;
	if (!cb->cq) {
		fprintf(stderr, "Error Allocating Completion Queue\n");
		return -1;
	}
    return 0;
}
int
ib_setup_common(struct rdma_cb *cb,struct ibv_comp_channel *cc,struct ibv_cq *cq) {

	struct ibv_qp_init_attr qp_init_attrs;
	int err;

/*
 * Allocate the Protection Domain
 */
	cb->pd = ibv_alloc_pd(cb->cm_id->verbs);
	if (!cb->pd) {
		fprintf(stderr, "Error Allocating Protection Domain\n");
		return -1;
	}
	printf("ib_setup_common: %d %d \n",cc,cq);
    if(!cc){
        create_cccq(cb);
    }else{
        cb->comp_channel=cc;
        cb->cq=cq;
    }
/*
 * Request Event Notification.
 */
	err = ibv_req_notify_cq(cb->cq, 0);
	if (err) {
                fprintf(stderr, "Failed to request notification: %d\n", err);
		return -1;
	}

/*
 * Initialize the QP attributes
 */
	memset(&qp_init_attrs, 0, sizeof(qp_init_attrs));
	qp_init_attrs.send_cq = cb->cq;
	qp_init_attrs.recv_cq = cb->cq;
	qp_init_attrs.qp_type = IBV_QPT_RC; //http://www.rdmamojo.com/2013/06/01/which-queue-pair-type-to-use/

	qp_init_attrs.cap.max_send_wr = MAX_SEND_WR;
	qp_init_attrs.cap.max_send_sge = MAX_SEND_SGE;

	qp_init_attrs.cap.max_recv_wr = MAX_RECV_WR;
	qp_init_attrs.cap.max_recv_sge = MAX_RECV_SGE;

/*
 * Create the Queue Pair using the RDMA CM
 */
	err = rdma_create_qp(cb->cm_id, cb->pd, &qp_init_attrs);
	if (err) {
		fprintf(stderr, "Error Creating Queue Pair\n");
		return -1;
	}
	cb->qp = cb->cm_id->qp;
	// for traceback qp to cb
    qps[qpcount]=cb->qp->qp_num;
    cbs[qpcount]=cb;

    qpcount++;
    //////
	fprintf(stdout, "Created QP: 0x%x\n", cb->qp->qp_num);

/*
 * Register our Memory Regions
 */
	cb->recv_mr = ibv_reg_mr(cb->pd, &cb->recv_buffer, sizeof(cb->recv_buffer), IBV_ACCESS_LOCAL_WRITE);
	if (!cb->recv_mr) {
		fprintf(stderr, "Error registering recv buffer\n");
		return -1;
	}

	cb->send_mr = ibv_reg_mr(cb->pd, &cb->send_buffer, sizeof(cb->send_buffer), 0);
	if (!cb->send_mr) {
		fprintf(stderr, "Error registering send buffer\n");
		return -1;
	}

	cb->rdma_packet_len = strlen(SEND_STR)+1;	/* +1 for NULL Char */
	//cb->rdma_buffer = malloc(cb->rdma_buf_len);
	cb->rdma_buf_len = BUF_LEN;
	int shmid;
	//if ((shmid = shmget(SHMKEY, BUF_LEN, 0666 | IPC_CREAT)) < 0) {
	if(COMEX_Area == NULL){
		if ((shmid = shmget(SHMKEY, BUF_LEN, 0666 )) < 0) {
				perror("shmget");
				exit(1);
			}
		if ((cb->rdma_buffer = shmat(shmid, NULL, 0)) == (char *) -1) {
			perror("shmat");
			exit(1);
		}
		COMEX_Area = cb->rdma_buffer;
		#ifdef VERBOSE
			printf("COMEX AREA is null, registered at %p\n", cb->rdma_buffer);
		#endif
	}else{
		#ifdef VERBOSE
			printf("COMEX AREA is %p\n",COMEX_Area);
		#endif
		cb->rdma_buffer = COMEX_Area;
	}
	printf("cb->rdma_mr= %p\n",cb->rdma_mr);
	if (!cb->rdma_buffer) {
		fprintf(stderr, "Error allocating rdma buffer\n");
		return -1;
	}
	// original
	 cb->rdma_mr = ibv_reg_mr(cb->pd, cb->rdma_buffer, cb->rdma_buf_len,
					IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ| IBV_ACCESS_REMOTE_WRITE );

	//test
	/*
	cb->rdma_mr = ibv_reg_mr(cb->pd, cb->rdma_buffer, cb->rdma_buf_len,
					IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ| IBV_ACCESS_REMOTE_WRITE| IBV_ACCESS_MW_BIND );
	*/
	printf("cb->rdma_mr= %p\n",cb->rdma_mr);
	if (!cb->rdma_mr) {
		fprintf(stderr, "Error registering rdma buffer\n");
		return -1;
	}

/*
 * Post a receive buffer
 */
	err = do_recv(cb);
	if (err) {
		return -1;
	}

	return 0;
}

void
cleanup_common(struct rdma_cb *cb) {


	if (cb->rdma_mr) {
		ibv_dereg_mr(cb->rdma_mr);
	}
#ifndef SHAREDMEM
	if (cb->rdma_buffer) {
		free(cb->rdma_buffer);
	}
#endif
	if (cb->send_mr) {
		ibv_dereg_mr(cb->send_mr);
	}

	if (cb->recv_mr) {
		ibv_dereg_mr(cb->recv_mr);
	}

	if (cb->qp) {
		rdma_destroy_qp(cb->cm_id);
	}

	if (cb->comp_channel) {
		ibv_destroy_comp_channel(cb->comp_channel);
	}

	if (cb->cq) {
		ibv_destroy_cq(cb->cq);
	}

	if (cb->pd) {
		ibv_dealloc_pd(cb->pd);
	}

	if (cb->cm_id) {
		rdma_destroy_id(cb->cm_id);
	}

	if (cb->listen_cm_id) {
		rdma_destroy_id(cb->listen_cm_id);
	}

	if (cb->event_channel) {
		rdma_destroy_event_channel(cb->event_channel);
	}

	free(cb);

	return;
}

int
init_rdma_cm(struct rdma_cb *cb) {

	int err;

/*
 * Initialize the RDMA CM Event channel
 */
	cb->event_channel = rdma_create_event_channel();

/*
 * Create an RDMA CM id
 */
	if (cb->is_server) {
		err = rdma_create_id(cb->event_channel, &cb->listen_cm_id, cb, RDMA_PS_TCP);
	} else {
		err = rdma_create_id(cb->event_channel, &cb->cm_id, cb, RDMA_PS_TCP);
	}
	if (err) {
		fprintf(stderr, "Error creating RDMA CM ID: %d\n", err);
	}

	return err;

}
int
do_recv(struct rdma_cb *cb) {

	struct ibv_recv_wr recv_wr, *bad_wr;
	struct ibv_sge recv_sge;
	int err;

/*
 * Initialize the recv sge
 */
	recv_sge.addr = (uint64_t)&cb->recv_buffer;
	recv_sge.length = sizeof(cb->recv_buffer);
	recv_sge.lkey = cb->recv_mr->lkey;

/*
 * Initialize the WR
 */
	memset(&recv_wr, 0, sizeof(recv_wr));
	recv_wr.sg_list = &recv_sge;
	recv_wr.num_sge = 1;

/*
 * Issue the post
 */
	err = ibv_post_recv(cb->qp, &recv_wr, &bad_wr);
	if (err) {
		fprintf(stderr, "Error posting recv work request: %d\n", err);
		return err;
	}
#ifdef VERBOSE
	fprintf(stdout, "Posted received buffer 0x%llx with length %d\n", (unsigned long long)recv_sge.addr, recv_sge.length);
#endif
    bug=recv_sge.addr;
	return 0;
}

int do_send(struct rdma_cb *cb, enum ibv_wr_opcode opcode,unsigned long ouroffset,unsigned long remoteoffset,uint32_t immdOrsize) {

	struct ibv_send_wr send_wr, *bad_wr;
	struct ibv_sge send_sge;
	int err;

	memset(&send_wr, 0, sizeof(send_wr));
	send_wr.opcode = opcode;
	send_wr.send_flags = IBV_SEND_SIGNALED;
	send_wr.num_sge = 1;
	send_wr.sg_list = &send_sge;

	if (opcode == IBV_WR_RDMA_READ) {
		send_sge.addr = (uint64_t)cb->rdma_buffer+ouroffset;
		send_sge.length = immdOrsize;
		send_sge.lkey = cb->rdma_mr->lkey;

		send_wr.wr.rdma.remote_addr = cb->remote_buffer.addr+remoteoffset;
		send_wr.wr.rdma.rkey = cb->remote_buffer.rkey;
	}else if (opcode == IBV_WR_RDMA_WRITE) {
        send_sge.addr = (uint64_t)cb->rdma_buffer+ouroffset;
		send_sge.length = immdOrsize; //
		send_sge.lkey = cb->rdma_mr->lkey;
        send_wr.wr.rdma.remote_addr = cb->remote_buffer.addr+remoteoffset;
        send_wr.wr.rdma.rkey = cb->remote_buffer.rkey;
	}else if (opcode == IBV_WR_RDMA_WRITE_WITH_IMM) {
        send_sge.addr = (uint64_t)cb->rdma_buffer+ouroffset;
		send_sge.length = 1; // should fix it
		send_sge.lkey = cb->rdma_mr->lkey;
        //send_wr.imm_data = 0xdeadbeef;
        send_wr.imm_data = immdOrsize; //it's an immeridate
        send_wr.wr.rdma.remote_addr = cb->remote_buffer.addr+remoteoffset;
        send_wr.wr.rdma.rkey = cb->remote_buffer.rkey;
    }else if(opcode == IBV_WR_SEND){
        send_sge.lkey = cb->send_mr->lkey;
        send_sge.addr = (uint64_t)&cb->send_buffer;
        send_sge.length = sizeof(cb->send_buffer); // huh? send the entire sending buffer?
    }else if(opcode == IBV_WR_SEND_WITH_IMM){
        send_sge.lkey = cb->send_mr->lkey;
        send_sge.addr = (uint64_t)&cb->send_buffer;
        send_sge.length = sizeof(cb->send_buffer);
        send_wr.imm_data = immdOrsize; //it's an immediate
	}

	err = ibv_post_send(cb->qp, &send_wr, &bad_wr);
	if (err) {
		printf(stderr, "Error posting send work request: %d\n", err);
		return err;
	}

	return 0;
}

int
process_rdma_cm_event(struct rdma_cb *cb, enum rdma_cm_event_type expected_event) {

	struct rdma_cm_event *cm_event;
	int err;

/*
 * Wait for the RDMA_CM event to complete
 */
	err = rdma_get_cm_event(cb->event_channel, &cm_event);
	if (err) {
		fprintf(stderr, "RDMA get CM event failed: %d\n", err);
		return err;
	}

/*
 * Process the RDMA CM event based on what we expect to
 */
	if (cm_event->event != expected_event) {
		fprintf(stderr, "RDMA CM event (%s) failed: %s: %d\n", rdma_event_str(expected_event),
			rdma_event_str(cm_event->event), cm_event->status);
		err = -1;
		goto ack;
	}

/*
 * On a connection request we need to save off and use the
 * cm_id in the event.
 */
	if (cb->is_server && (cm_event->event == RDMA_CM_EVENT_CONNECT_REQUEST) ) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *)&cm_event->id->route.addr.src_addr;
		fprintf(stdout, "Received Connection Request from: %s\n", inet_ntoa(sin->sin_addr));
		cb->cm_id = cm_event->id;
	}

/*
 * Ack the event so it gets freed.
 */
ack:
	rdma_ack_cm_event(cm_event);

	return err;
}

int sendRDMAinfo(struct rdma_cb *cb,int instanceNo){
/*
 * Pass RDMA buffer information to the other side
 */
        int retqpno;
		int retimm;
        int err;
        cb->send_buffer.addr  = htonll((uint64_t)cb->rdma_buffer);
        cb->send_buffer.length = htonl(cb->rdma_buf_len);
        cb->send_buffer.rkey = htonl(cb->rdma_mr->rkey);
        sprintf(cb->send_buffer.piggy,"%d",instanceNo);
        fprintf(stdout, "Sending RDMA Buffer information: addr: 0x%llx, length: %lld, rkey: 0x%x instanceNo=%d\n",
                        (unsigned long long)cb->rdma_buffer, cb->rdma_buf_len, cb->rdma_mr->rkey,instanceNo);
        err = do_send(cb, IBV_WR_SEND,0,0,0);
        if (err) {
                return 1;
        }
        err = do_completion(cb,&retimm,&retqpno);
        if (err) {
                return 1;
        }
        return 0;
}
int server_Internalinit(struct rdma_cb *cb ,struct rdma_conn_param *conn_params){

	int err;

/*
 * Init the RDMA CM Event channel and CM_ID
 */
	err = init_rdma_cm(cb);
	if (err) {
		return err;
	}

/*
 * Bind on a port and an optional interface.
 */
	err = rdma_bind_addr(cb->listen_cm_id, (struct sockaddr *)&cb->sin);
	if (err) {
		fprintf(stderr, "Error binding to address/port: %d\n", err);
		return err;
	}

/*
 * Start listening for Connection Requests.
 */
	err = rdma_listen(cb->listen_cm_id, 1);
	if (err) {
		fprintf(stderr, "Error trying to listen: %d\n", err);
		return err;
	}

/*
 * Zero out the rdma buffer.  This will get written by the RDMA READ.
 */
	fprintf(stdout, "Initializing RDMA buffer contents to 0\n");
	memset(cb->rdma_buffer, 0, cb->rdma_buf_len);
    return 0;
}
int server_disconnect(struct rdma_cb *cb){
	rdma_disconnect(cb->cm_id);
	process_rdma_cm_event(cb, RDMA_CM_EVENT_DISCONNECTED);

	return -1;
}
int server_getConnect(struct rdma_cb *cb ,struct rdma_conn_param *conn_params,struct ibv_comp_channel *cc,struct ibv_cq *cq){

/*
 * Wait for a connection request
 */
    int err;
	fprintf(stdout, "Waiting for a Connection Request\n");
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_CONNECT_REQUEST);
	if (err) {
		return err;
	}

/*
 * Allocate the IB resources
 */
    err = ib_setup_common(cb,cc,cq);
    if (err) {
		return err;
	}

/*
 * Initialize the connection parameters.
 */
	memset(conn_params, 0, sizeof(*conn_params));
	conn_params->responder_resources = 1; //diff
	conn_params->initiator_depth = 1; //diff
	conn_params->retry_count = 8;
	conn_params->rnr_retry_count = 8;

/*
 * Accept the Connection
 */
	fprintf(stdout, "Accepting the connection\n");
	err = rdma_accept(cb->cm_id, conn_params);

/*
 * Wait for the connection to get fully established.
 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_ESTABLISHED);
	if (err) {
		return err;
	}

	fprintf(stdout, "Connection Established!\n");

	return 0;
}
int do_asyncread(struct rdma_cb *cb ,int ouroffset,int remoteoffset,int SEND_SIZE){
int err;

#ifdef VERBOSE
	fprintf(stdout, "Issuing an RDMA READ from client memory to server memory\n");
#endif
	err = do_send(cb, IBV_WR_RDMA_READ,ouroffset,remoteoffset,SEND_SIZE);
	if (err) {
		return 1;
	}
	return 0;
}

int do_write_imm(struct rdma_cb *cb,int immediate){
int err;
int retqpno;
int retimm;
#ifdef VERBOSE
        fprintf(stdout, "Issuing an RDMA Write with immediate to the server\n");
#endif
	err = do_send(cb, IBV_WR_RDMA_WRITE_WITH_IMM,0,0,immediate);
        if (err) {
                return 1;
        }
/*
 * Check our write completion
 */
        err = do_completion(cb,&retimm,&retqpno);
        if (err) {
			printf("error in write withimm completion");
                return 1;
        }
return 0;
}
int get_write_imm(struct rdma_cb *cb,int *immediate){
int retqpno;
    return get_write_imm_whoever(cb,immediate,&retqpno);
}
int get_write_imm_whoever(struct rdma_cb *cb,int *immediate,int* retqpno){
int err;
        err = do_completion(cb,immediate,retqpno);
        if (err) {
                return 1;
        }
        return 0;
}
int do_write(struct rdma_cb *cb,unsigned long ouroffset,unsigned long remoteoffset,unsigned int size){
int err;
int retqpno;
int retimm;
#ifdef VERBOSE
        fprintf(stdout, "Issuing an RDMA Write to the server\n");
#endif
	err = do_send(cb, IBV_WR_RDMA_WRITE,ouroffset,remoteoffset,size);
        if (err) {
                return 1;
        }
    err = do_completion(cb,&retimm,&retqpno);
        if (err) {
                return 1;
        }
return 0;
}
// verb send, accept through do_completion then values are stored in cb->recv_buffer.piggy
int do_sendout(struct rdma_cb *cb,int imm){
    int err;
    int retqpno;
	int retimm;
    err= do_send(cb, IBV_WR_SEND_WITH_IMM,0,0,imm);
        if (err) {
			printf("error!");
                return 1;
        }

    err = do_completion(cb,&retimm,&retqpno);
        if (err) {

			printf("error2!");
                return 1;
    }
    return 0;
}

int init_mq_sender(){
    if ((key = ftok("query_dev.c", 'B')) == -1) {
        perror("ftok");
        exit(1);
    }
    if ((msqid = msgget(key, 0644| IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
        }
return 0;
}
int init_mq_receiver(){
    if ((key = ftok("query_dev.c", 'B')) == -1) { // same
            perror("ftok");
        exit(1);
    }
    //if ((msqid = msgget(key, 0644)) == -1) { /* connect to the queue */
    if ((msqid = msgget(key, 0644| IPC_CREAT)) == -1) {
        perror("msgget");
        exit(1);
    }
}

int do_server(struct rdma_cb *cb[0],int cbcount) {
	struct rdma_conn_param conn_params[2];
	int err,i;
	int retqpno,retimm;
    //internal init
    for(i=0;i<cbcount;i++){
        err=server_Internalinit(cb[i],&conn_params[i]);
        if(err){
            return err;
        };
	}
	// create message queue
        init_mq_sender();
	//
	//wait for client, establish all connection
	struct ibv_comp_channel *cc=0;
	struct ibv_cq *cq=0;
    // shared cc,cq
    for(i=0;i<cbcount;i++){
        err=server_getConnect(cb[i],&conn_params[i],cc,cq);
        if(err){
            return err;
        };
        //
        err = do_completion(cb[i],&retimm,&retqpno);
        if (err) {
            return err;
        }
        err=sendRDMAinfo(cb[i],myinstanceNo);
        if(err){
            return server_disconnect(cb[i]);
        };
// trade instanceNo integreated in RDMAinfo
        cc=cb[i]->comp_channel;
        cq=cb[i]->cq;
	}
////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
// define what to do here.

	while(work>0){
		int retval,retqpno;
        err=do_completion(cb[0],&retval,&retqpno); // receive whatever
        if (err) {
                return -1;
        }

        switch(retval){  //this is what i expected from otherside's verb send
#ifdef IS_SERVER
            case 2000:
				COMEX_server_msg(qp2cb(retqpno)->recv_buffer.piggy);
				break;
            case 1000:
				COMEX_server_cmd(qp2cb(retqpno)->recv_buffer.piggy);
				break;
#endif
            case 5:
                printf("piggy = %s\n",qp2cb(retqpno)->recv_buffer.piggy);
				break;
            case 2:
                printf("Instance number info? why do i get it now?");
				break;
            case -1:
                printf("get disconnect msg\n"); // should be modify to handle more than one connection
                work=0;
				break;
            default:
            printf("unexpected,unhandled immediate received=%d",retval);
        }
       // printf("%s",SEND_STR);
}
///////////////////////////////////////////////////////////////////////////
/*
 * Wait for the disconnect message
 */

    for(i=0;i<cbcount;i++){
        err = process_rdma_cm_event(cb[i], RDMA_CM_EVENT_DISCONNECTED);
        if (err) {
            return err;
        }
    }
	fprintf(stdout, "Connection has been closed. normally\n");
	return 0;
}

int client_init(struct rdma_cb *cb,struct ibv_comp_channel *cc,struct ibv_cq *cq){

/*
 * Init the RDMA CM Event channel and CM_ID
 */
    struct rdma_conn_param conn_params;
    int err;
	err = init_rdma_cm(cb);
	if (err) {
		return err;
	}

/*
 * Choose the appropriate RDMA device to use based on the destination
 * address info.
 */
	err = rdma_resolve_addr(cb->cm_id, NULL, (struct sockaddr *)&cb->sin, 3000);
	if (err) {
		fprintf(stderr, "Could not resolve RDMA ADDR: %d\n", err);
		return err;
	}

/*
 * Process the resolve addr event.
 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_ADDR_RESOLVED);
	if (err) {
		return err;
	}

/*
 * Resolve the route
 */
	err = rdma_resolve_route(cb->cm_id, 3000);
	if (err) {
		fprintf(stderr, "Failed to resolve RDMA route: %d\n", err);
		return err;
	}

/*
 * Process the resolve addr event.
 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_ROUTE_RESOLVED);
	if (err) {
		return err;
	}

/*
 * Now that we have the ADDR and ROUTE resolved we need to do our IB setup.
 */
	err = ib_setup_common(cb,cc,cq);
	if (err) {
		return err;
	}

/*
 * Initialize the connection parameters.
 */
	memset(&conn_params, 0, sizeof(conn_params));
	conn_params.responder_resources = 1;
	conn_params.initiator_depth = 1;
	conn_params.retry_count = 8;
	conn_params.rnr_retry_count = 8;
/*
 * Connect to the server.
 */
	fprintf(stdout, "Initiating the Connection... ");

	err = rdma_connect(cb->cm_id, &conn_params);
	if (err) {
		fprintf(stderr, "Error connecting to the server: %d\n", err);
	}

/*
 * Process the connection request.
 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_ESTABLISHED);
	if (err) {
		return err;
	}
	fprintf(stdout, "Connection Established!\n");

return 0;
}
int client_disconnect(struct rdma_cb *cb){
    int err;
/*
 * Disconnect
 */
	fprintf(stdout, "Initiating Disconnect... ");

	err = rdma_disconnect(cb->cm_id);
	if (err) {
		fprintf(stderr, "Error tearing down the connection: %d\n", err);
		return err;
	}

/*
 * Process the disconnect request.
 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_DISCONNECTED);
	if (err) {
		return err;
	}

	fprintf(stdout, "Disconnect Complete.\n");
	return 0;
}
int
do_client(struct rdma_cb *cb[],int cbcount) {
	int err,i;
	//all init

 	struct ibv_comp_channel *cc=0;
	struct ibv_cq *cq=0;
	int retqpno,f,retimm;;
    err=client_init(cb[0],cc,cq);
	if (err) {
		return err;
	}
	printf("first connection work!");
	cc=cb[0]->comp_channel;
	cq=cb[0]->cq;
	for(i=1;i<cbcount;i++){
        err=client_init(cb[i],cc,cq);
        if (err) {
            return err;
        }
    }
/*
 * Initialize the RDMA buffer memory
 */
 	for(i=0;i<cbcount;i++){
        //strncpy(cb[i]->rdma_buffer, RDMA_STRING, cb[i]->rdma_buf_len);
        err=sendRDMAinfo(cb[i],myinstanceNo);
        if(err){
			printf("sendRDMAinfo\n");
            return 1;
        }
        err = do_completion(cb[i],&retimm,&retqpno);
        if (err) {
            return err;
        }

 	}
//message queue init
init_mq_receiver();
 	////////////////////////////
// do something here
    if(work==1){
             //example of how to send a message
        //char tmpstr[40]="test bug one two three";
        //strcpy((cb[0]->send_buffer).piggy, tmpstr); //or memcpy or whatever
        //do_sendout(cb[0],24,5); //edit 24


    }
//	printf("here\n");
	/*
 //disconnect msg
	for(i=0;i<cbcount;i++){
        err=do_write_imm(cb[i],-1);
        if(err){
            return client_disconnect(cb[0]);
        }
	}


//////////////////////////////////////////////////////////////////////////////////////
    //
    for(i=cbcount-1;i>0;i--){
        client_disconnect(cb[i]);
    }

    return client_disconnect(cb[0]);
	*/
	return 0;
}

int allinit(struct rdma_cb *cb[],int cbcount,int startport){
    int err,i,plus=0;
/*
 * common setup
 */
    for(i=0;i<cbcount;i++){
		cb[i]->sin.sin_family = AF_INET;
    }
/*
 * Run client/server specific code
 */
	if (cb[0]->is_server) {			// Server
        for(i=0; i<cbcount; i++){
            cb[i]->sin.sin_port = startport+i;
        }
		err = do_server(cb, cbcount);
	}
	else {							// Client
	    for(i=0; i<cbcount; i++){
            cb[i]->sin.sin_port = startport+ atoi(allPort[i]);
            err = inet_aton(allIP[i], &cb[i]->sin.sin_addr);
            if (!err) {
                fprintf(stderr, "inet_aton failed on address: %s\n", allIP[i]);
                return 1;
            }
        }
		err = do_client(cb,cbcount);
		if(err){
			fprintf(stderr, "Error - %d\n", err);
		}
	}

	return err;
}
// argv[1] first arg= number of instance
// argv[2] second= instanceNo
// argv[3],[4] third+= more IP and port offset (s)
/*
int
main(int argc, char **argv) {

	struct rdma_cb *cb[8];
	int err = 0,i,cbcount;
	cbcount=atoi(argv[1]);
	myinstanceNo=atoi(argv[2]);

	for(i=0;i<cbcount;i++){
		cb[i] = malloc(sizeof(*cb[i]));
		if (!cb[i]) {
			fprintf(stderr, "Could not allocate memory for the control block\n");
			return -1;
		}
		memset(cb[i], 0, sizeof(*cb[i]));
	}

	if (argc == 3) {
		fprintf(stdout, "Starting Server\n");
        for(i=0;i<cbcount;i++){
            cb[i]->is_server = 1;
        }
	}else{
	    for(i=0;i<cbcount;i++){
            fprintf(stdout, "Running Client, Server IP %s %d\n", argv[3+i*2],PORTOFFSET+atoi(argv[4+i*2]));
		}
	}
        allinit(cb,cbcount,PORTOFFSET,argv);
out:
    for(i=0;i<cbcount;i++){
        cleanup_common(cb[i]);
	}
	return err;
}
*/

struct rdma_cb **  startRDMA_Server(int totalNodes, int nodeID, unsigned long totalMem, char* COMEXAREA) {
	struct rdma_cb **cb_pointers;
	int i;

	myinstanceNo = nodeID;
	BUF_LEN = totalMem;
	COMEX_Area = COMEXAREA;
	
	cb_pointers = (struct rdma_cb **)malloc(sizeof(struct rdma_cb *)*totalNodes);
	for(i=0; i<totalNodes; i++){
		cb_pointers[i] = (struct rdma_cb *)malloc(sizeof(struct rdma_cb));
		if(!cb_pointers[i]){
			fprintf(stderr, "Could not allocate memory for the control block\n");
			return -1;
		}
		memset(cb_pointers[i], 0, sizeof(struct rdma_cb));
		cb_pointers[i]->is_server = 1;
	}
	allinit(cb_pointers, totalNodes, PORTOFFSET);

out:
    for(i=0; i<totalNodes; i++){
        cleanup_common(cb_pointers[i]);
	}

	fprintf(stdout, "startRDMA_Server OK\n");
	return cb_pointers;
}

struct rdma_cb ** startRDMA_Client(int totalNodes, int nodeID, unsigned long totalMem, char **ShAddr) {
	struct rdma_cb **cb_pointers;
	int i;

	myinstanceNo = nodeID;
	BUF_LEN = totalMem;
	COMEX_Area = NULL;

	cb_pointers = (struct rdma_cb **)malloc(sizeof(struct rdma_cb *)*totalNodes);
	for(i=0; i<totalNodes; i++){
		cb_pointers[i] = (struct rdma_cb *)malloc(sizeof(struct rdma_cb));
		if(!cb_pointers[i]){
			fprintf(stderr, "Could not allocate memory for the control block\n");
			return -1;
		}
		memset(cb_pointers[i], 0, sizeof(struct rdma_cb));
	}

	allinit(cb_pointers, totalNodes, PORTOFFSET);
	*ShAddr = COMEX_Area;
	fprintf(stdout, "startRDMA_Client OK\n");
	return cb_pointers;
}
