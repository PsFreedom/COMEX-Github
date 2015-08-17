#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <infiniband/verbs.h>
#include <infiniband/arch.h>
#include <rdma/rdma_cma.h>

#include <unistd.h>

/*
 *  * Defines
 *   */
#define CQ_DEPTH	8
#define SRV_RR_PORT 	10200

#define MAX_SEND_WR	4
#define MAX_SEND_SGE	1

#define MAX_RECV_WR	4
#define MAX_RECV_SGE	1

//#define BUF_LEN 17179869184ll
//#define BUF_LEN 1024
#define BUF_LEN 1073741824
#define SEND_SIZE 1048576
#define LOOP_TIME 1000
//#define VERBOSE 1
//#define SLEEP_TIME 0

//#define RDMA_STRING_PRE 	"Hello World!"
char RDMA_STRING[65]="Hello World!";

struct buffer {
	uint64_t addr;
	uint32_t length;
	uint32_t rkey;
};

/*
 *  * Control Block used to store all data structures and context information.
 *   */
struct rdma_cb {

	/*
 *  	 * Command line args
 *  	  	 */
	int is_server;
	struct sockaddr_in sin;
	
	/*
 * 	 * RDMA CM data structures
 * 	 	 */
	struct rdma_event_channel *event_channel;
	struct rdma_cm_id *cm_id;
	struct rdma_cm_id *listen_cm_id;		/* Server only */

	/*
 * 	 * IB data structures
 * 	 	 */
	struct ibv_comp_channel *comp_channel;
	struct ibv_cq *cq;
	struct ibv_pd *pd;
	struct ibv_qp *qp;

	/*
 * 	 * Memory Region and data buffers
 * 	 	 */
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

/*
 *  * Function Prototypes
 *   */
int setup_common(struct rdma_cb *);
void cleanup_common(struct rdma_cb *);
int do_server(struct rdma_cb *);
int do_client(struct rdma_cb *);
int do_recv(struct rdma_cb *);
int do_poll(struct rdma_cb *);
int process_rdma_cm_event(struct rdma_cb *, enum rdma_cm_event_type);

int
do_poll(struct rdma_cb *cb) {

	struct ibv_wc wc;
	int err;

	while ((err = ibv_poll_cq(cb->cq, 1, &wc)) == 1) {

		if (wc.status) {
			fprintf(stderr, "Completion failed for event: %d, status: %d\n", wc.opcode, wc.status);
			return -1;
		}

		switch (wc.opcode) {
		case IBV_WC_SEND:
			fprintf(stdout, "Send completed successfully.\n");
			break;

		case IBV_WC_RDMA_READ:
			#ifdef VERBOSE
			fprintf(stdout, "RDMA READ completed successfully.\n");
			#endif
			break;

		case IBV_WC_RECV:
			fprintf(stdout, "Recv completed successfully.\n");

			/*
 * 			 * Copy out the buffer information.
 * 			 			 */
			cb->remote_buffer.addr = ntohll(cb->recv_buffer.addr);
			cb->remote_buffer.length  = ntohl(cb->recv_buffer.length);
			cb->remote_buffer.rkey = ntohl(cb->recv_buffer.rkey);

			/*
 * 			 * Repost our RECV Buffer.
 * 			 			 */
			err = do_recv(cb);
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
do_completion(struct rdma_cb *cb) {

	struct ibv_cq *event_cq;
	void *event_context;
	int err;

	/*
 * 	 * Block until we get a completion event.
 * 	 	 */
	err = ibv_get_cq_event(cb->comp_channel, &event_cq, &event_context);
	if (err) {
		fprintf(stderr, "Failed getting CQ event: %d\n", err);
		return -1;
	}

	/*
 * 	 * Need to rerequest notification so we will get an event the next
 * 	 	 * time a WR completes.
 * 	 	 	 */
	err = ibv_req_notify_cq(cb->cq, 0);
	if (err) {
                fprintf(stderr, "Failed to request notification: %d\n", err);
		return -1;
	}

	/*
 * 	 * Process the Completion event.
 * 	 	 */
	err = do_poll(cb);
	if (err) {
		return -1;
	}

	/*
 * 	 * Ack the CQ Event
 * 	 	 */
	ibv_ack_cq_events(cb->cq, 1);

	return 0;
}

int
ib_setup_common(struct rdma_cb *cb) {

	struct ibv_qp_init_attr qp_init_attrs;
	int err;

	/*
 * 	 * Allocate the Protection Domain
 * 	 	 */
	cb->pd = ibv_alloc_pd(cb->cm_id->verbs);
	if (!cb->pd) {
		fprintf(stderr, "Error Allocating Protection Domain\n");
		return -1;
	}

	/*
 * 	 * Create a Completion Channel
 * 	 	 */
	cb->comp_channel = ibv_create_comp_channel(cb->cm_id->verbs);
	if (!cb->comp_channel) {
		fprintf(stderr, "Error Creating a Completion Channel\n");
		return -1;
	}

	/*
 * 	 * Create a Completion Queue
 * 	 	 */
	cb->cq = ibv_create_cq(cb->cm_id->verbs, CQ_DEPTH, cb, cb->comp_channel, 0);
	if (!cb->cq) {
		fprintf(stderr, "Error Allocating Completion Queue\n");
		return -1;
	}

	/*
 * 	 * Request Event Notification.
 * 	 	 */
	err = ibv_req_notify_cq(cb->cq, 0);
	if (err) {
                fprintf(stderr, "Failed to request notification: %d\n", err);
		return -1;
	}

	/*
 * 	 * Initialize the QP attributes
 * 	 	 */
	memset(&qp_init_attrs, 0, sizeof(qp_init_attrs));
	qp_init_attrs.send_cq = cb->cq;
	qp_init_attrs.recv_cq = cb->cq;
	qp_init_attrs.qp_type = IBV_QPT_RC;

	qp_init_attrs.cap.max_send_wr = MAX_SEND_WR;
	qp_init_attrs.cap.max_send_sge = MAX_SEND_SGE;

	qp_init_attrs.cap.max_recv_wr = MAX_RECV_WR;
	qp_init_attrs.cap.max_recv_sge = MAX_RECV_SGE;

	/*
 * 	 * Create the Queue Pair using the RDMA CM
 * 	 	 */
	err = rdma_create_qp(cb->cm_id, cb->pd, &qp_init_attrs);
	if (err) {
		fprintf(stderr, "Error Creating Queue Pair\n");
		return -1;
	}

	cb->qp = cb->cm_id->qp;

	fprintf(stdout, "Created QP: 0x%x\n", cb->qp->qp_num);

	/*
 * 	 * Register our Memory Regions
 * 	 	 */ 
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

	cb->rdma_packet_len = strlen(RDMA_STRING)+1;	/* +1 for NULL Char */
	//cb->rdma_buffer = malloc(cb->rdma_buf_len);
	cb->rdma_buf_len = BUF_LEN; 
	cb->rdma_buffer = malloc(BUF_LEN);
	if (!cb->rdma_buffer) {
		fprintf(stderr, "Error allocating rdma buffer\n");
		return -1;
	}
	cb->rdma_mr = ibv_reg_mr(cb->pd, cb->rdma_buffer, cb->rdma_buf_len, 
					IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ| IBV_ACCESS_REMOTE_WRITE);
	if (!cb->rdma_mr) {
		fprintf(stderr, "Error registering rdma buffer\n");
		return -1;
	}

	/*
 * 	 * Post a receive buffer
 * 	 	 */
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

	if (cb->rdma_buffer) {
		free(cb->rdma_buffer);
	}

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
 * 	 * Initialize the RDMA CM Event channel
 * 	 	 */
	cb->event_channel = rdma_create_event_channel();

	/*
 * 	 * Create an RDMA CM id
 * 	 	 */
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
 * 	 * Initialize the recv sge
 * 	 	 */
	recv_sge.addr = (uint64_t)&cb->recv_buffer;
	recv_sge.length = sizeof(cb->recv_buffer);
	recv_sge.lkey = cb->recv_mr->lkey;

	/*
 * 	 * Initialize the WR
 * 	 	 */
	memset(&recv_wr, 0, sizeof(recv_wr));
	recv_wr.sg_list = &recv_sge;
	recv_wr.num_sge = 1;

	/*
 * 	 * Issue the post
 * 	 	 */
	err = ibv_post_recv(cb->qp, &recv_wr, &bad_wr);
	if (err) {
		fprintf(stderr, "Error posting recv work request: %d\n", err);
		return err;
	}

	fprintf(stdout, "Posted received buffer 0x%llx with length %d\n", (unsigned long long)recv_sge.addr, recv_sge.length);

	return 0;
}

int
do_send(struct rdma_cb *cb, enum ibv_wr_opcode opcode) {

	struct ibv_send_wr send_wr, *bad_wr;
	struct ibv_sge send_sge;
	int err;

	memset(&send_wr, 0, sizeof(send_wr));
	send_wr.opcode = opcode;
	send_wr.send_flags = IBV_SEND_SIGNALED;
	send_wr.num_sge = 1;
	send_wr.sg_list = &send_sge;

	/*
 * 	 * We only expect RDMA READ or SEND.
 * 	 	 */
	if (opcode == IBV_WR_RDMA_READ) {
		send_sge.addr = (uint64_t)cb->rdma_buffer;
		#ifdef SEND_SIZE
		send_sge.length = SEND_SIZE;
		#else
		send_sge.length = cb->rdma_packet_len;
		#endif
		send_sge.lkey = cb->rdma_mr->lkey;

		send_wr.wr.rdma.remote_addr = cb->remote_buffer.addr;
		send_wr.wr.rdma.rkey = cb->remote_buffer.rkey;
	} else {
		send_sge.addr = (uint64_t)&cb->send_buffer;
		send_sge.length = sizeof(cb->send_buffer);
		send_sge.lkey = cb->send_mr->lkey;
	}

	err = ibv_post_send(cb->qp, &send_wr, &bad_wr);
	if (err) {
		fprintf(stderr, "Error posting send work request: %d\n", err);
		return err;
	}

	return 0;
}

int
process_rdma_cm_event(struct rdma_cb *cb, enum rdma_cm_event_type expected_event) {

	struct rdma_cm_event *cm_event;
	int err;

	/*
 * 	 * Wait for the RDMA_CM event to complete
 * 	 	 */
	err = rdma_get_cm_event(cb->event_channel, &cm_event);
	if (err) {
		fprintf(stderr, "RDMA get CM event failed: %d\n", err);
		return err;
	}

	/*
 * 	 * Process the RDMA CM event based on what we expect to 
 * 	 	 */
	if (cm_event->event != expected_event) {
		fprintf(stderr, "RDMA CM event (%s) failed: %s: %d\n", rdma_event_str(expected_event), 
			rdma_event_str(cm_event->event), cm_event->status);
		err = -1;
		goto ack;
	}

	/*
 * 	 * On a connection request we need to save off and use the
 * 	 	 * cm_id in the event.
 * 	 	 	 */
	if (cb->is_server && (cm_event->event == RDMA_CM_EVENT_CONNECT_REQUEST) ) {
		struct sockaddr_in *sin;
		
		sin = (struct sockaddr_in *)&cm_event->id->route.addr.src_addr;
		fprintf(stdout, "Received Connection Request from: %s\n", inet_ntoa(sin->sin_addr));
		cb->cm_id = cm_event->id;
	}

	/*
 * 	 * Ack the event so it gets freed.
 * 	 	 */
ack:
	rdma_ack_cm_event(cm_event);

	return err;
}

int
do_server(struct rdma_cb *cb) {

	struct rdma_conn_param conn_params;
	int err;

	/*
 * 	 * Init the RDMA CM Event channel and CM_ID
 * 	 	 */
	err = init_rdma_cm(cb);
	if (err) {
		return err;
	}

	/*
 * 	 * Bind on a port and an optional interface.
 * 	 	 */
	err = rdma_bind_addr(cb->listen_cm_id, (struct sockaddr *)&cb->sin);
	if (err) {
		fprintf(stderr, "Error binding to address/port: %d\n", err);
		return err;
	}

	/*
 * 	 * Start listening for Connection Requests.
 * 	 	 */
	err = rdma_listen(cb->listen_cm_id, 1);
	if (err) {
		fprintf(stderr, "Error trying to listen: %d\n", err);
		return err;
	}

	/*
 * 	 * Wait for a connection request
 * 	 	 */
	fprintf(stdout, "Waiting for a Connection Request\n");
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_CONNECT_REQUEST);
	if (err) {
		return err;
	}

	/*
 * 	 * Allocate the IB resources
 * 	 	 */
	err = ib_setup_common(cb);
	if (err) {
		return err;
	}

	/*
 * 	 * Initialize the connection parameters.
 * 	 	 */
	memset(&conn_params, 0, sizeof(conn_params));
	conn_params.responder_resources = 1;
	conn_params.initiator_depth = 1;
	conn_params.retry_count = 8;
	conn_params.rnr_retry_count = 8;

	/*
 * 	 * Accept the Connection
 * 	 	 */
	fprintf(stdout, "Accepting the connection\n");
	err = rdma_accept(cb->cm_id, &conn_params);

	/*
 * 	 * Wait for the connection to get fully established.
 * 	 	 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_ESTABLISHED);
	if (err) {
		return err;
	}

	fprintf(stdout, "Connection Established!\n");

	/*
 * 	 * Zero out the rdma buffer.  This will get written by the RDMA READ.
 * 	 	 */
	fprintf(stdout, "Initializing RDMA buffer contents to 0\n");
	memset(cb->rdma_buffer, 0, cb->rdma_buf_len);

	/*
 * 	 * Wait for the client to send the rdma buffer information
 * 	 	 */
	fprintf(stdout, "Waiting for client rdma buffer info\n");
	err = do_completion(cb);
	if (err) {
		goto disconnect;
	}
	fprintf(stdout, "Received RDMA Buffer information: addr: 0x%llx, length: %" PRIu32 ", rkey: 0x%x\n",
			(unsigned long long)cb->remote_buffer.addr, cb->remote_buffer.length, cb->remote_buffer.rkey);
#ifdef LOOP_TIME
int loop=0;
backserv:
	/*
 * 	 * Perform the RDMA read
 * 	 	 */
#ifdef VERBOSE
	fprintf(stdout, "Issuing an RDMA READ from client memory to server memory\n");
#endif
	err = do_send(cb, IBV_WR_RDMA_READ);
	if (err) {
		goto disconnect;
	}

	/*
 * 	 * Check the completion of the rdma read
 * 	 	 */
	err = do_completion(cb);
	if (err) {
		goto disconnect;
	}

	/*
 * 	 * Print the contents of our RDMA buffer.
 * 	 	 */
#ifdef VERBOSE
	fprintf(stdout, "READ the following data from client memory to local memory: %s\n", cb->rdma_buffer);
#endif
loop++;
if(loop<LOOP_TIME){
#ifdef SLEEP_TIME
sleep(SLEEP_TIME);
#endif
goto backserv;
}
#endif

/*
command='a';
printf("enter e to exit:");
scanf("%c",&command);
printf("\n");
if(command!='e'){
	goto backserv;
}
*/
	/*
 * 	 * Perform another Send to the client, which lets the client know we have
 * 	 	 * processed the data from the RDMA READ.  Send buffer contents are
 * 	 	 	 * irrelevant.
 * 	 	 	 	 */
	fprintf(stdout, "Notifying client I have processed the data\n");
	err = do_send(cb, IBV_WR_SEND);
	if (err) {
		goto disconnect;
	}

	/*
 * 	 * Wait for and process the send completion
 * 	 	 */
	err = do_completion(cb);
	if (err) {
		goto disconnect;
	}

	fprintf(stdout, "Waiting for client to issue the disconnect\n");

	/*
 * 	 * Wait for the disconnect message
 * 	 	 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_DISCONNECTED);
	if (err) {
		return err;
	}

	fprintf(stdout, "Connection has been closed.\n");

	return 0;

disconnect:
	rdma_disconnect(cb->cm_id);
	process_rdma_cm_event(cb, RDMA_CM_EVENT_DISCONNECTED);

	return -1;
}

int
do_client(struct rdma_cb *cb) {

	struct rdma_conn_param conn_params;
	int err;

	/*
 * 	 * Init the RDMA CM Event channel and CM_ID
 * 	 	 */
	err = init_rdma_cm(cb);
	if (err) {
		return err;
	}

	/*
 * 	 * Choose the appropriate RDMA device to use based on the destination
 * 	 	 * address info.
 * 	 	 	 */
	err = rdma_resolve_addr(cb->cm_id, NULL, (struct sockaddr *)&cb->sin, 3000);
	if (err) {
		fprintf(stderr, "Could not resolve RDMA ADDR: %d\n", err);
		return err;
	}

	/*
 * 	 * Process the resolve addr event.
 * 	 	 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_ADDR_RESOLVED);
	if (err) {
		return err;
	}

	/*
 * 	 * Resolve the route
 * 	 	 */
	err = rdma_resolve_route(cb->cm_id, 3000);
	if (err) {
		fprintf(stderr, "Failed to resolve RDMA route: %d\n", err);
		return err;
	}

	/*
 * 	 * Process the resolve addr event.
 * 	 	 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_ROUTE_RESOLVED);
	if (err) {
		return err;
	}

	/*
 * 	 * Now that we have the ADDR and ROUTE resolved we need to do our IB setup.
 * 	 	 */
	err = ib_setup_common(cb);
	if (err) {
		return err;
	}

	/*
 * 	 * Initialize the connection parameters.
 * 	 	 */
	memset(&conn_params, 0, sizeof(conn_params));
	conn_params.responder_resources = 1;
	conn_params.initiator_depth = 1;
	conn_params.retry_count = 8;
	conn_params.rnr_retry_count = 8;
	
	/*
 * 	 * Connect to the server.
 * 	 	 */
	fprintf(stdout, "Initiating the Connection... ");

	err = rdma_connect(cb->cm_id, &conn_params);
	if (err) {
		fprintf(stderr, "Error connecting to the server: %d\n", err);
	}

	/* 
 * 	 * Process the connection request.
 * 	 	 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_ESTABLISHED);
	if (err) {
		return err;
	}

	fprintf(stdout, "Connection Established!\n");

	/*
 * 	 * Initialize the RDMA buffer memory
 * 	 	 */
	strncpy(cb->rdma_buffer, RDMA_STRING, cb->rdma_buf_len);

	/*
 * 	 * Pass RDMA buffer information to the client
 * 	 	 */
	cb->send_buffer.addr  = htonll((uint64_t)cb->rdma_buffer);
	cb->send_buffer.length = htonl(cb->rdma_buf_len);
	cb->send_buffer.rkey = htonl(cb->rdma_mr->rkey);

	fprintf(stdout, "Sending RDMA Buffer information: addr: 0x%llx, length: %lld, rkey: 0x%x\n",
			(unsigned long long)cb->rdma_buffer, cb->rdma_buf_len, cb->rdma_mr->rkey);
	err = do_send(cb, IBV_WR_SEND);
	if (err) {
		goto disconnect;
	}

	/*
 * 	 * Wait for and process the send completion
 * 	 	 */
	err = do_completion(cb);
	if (err) {
		goto disconnect;
	}

	fprintf(stdout, "Waiting for server to notify data has been read\n");

	/*
 * 	 * Wait for confirmation from the server that the data has been read.
 * 	 	 */
	err = do_completion(cb);
	if (err) {
		goto disconnect;
	}

disconnect:
	/*
 * 	 * Disconnect
 * 	 	 */
	fprintf(stdout, "Initiating Disconnect... ");

	err = rdma_disconnect(cb->cm_id);
	if (err) {
		fprintf(stderr, "Error tearing down the connection: %d\n", err);
		return err;
	}

	/*
 * 	 * Process the disconnect request.
 * 	 	 */
	err = process_rdma_cm_event(cb, RDMA_CM_EVENT_DISCONNECTED);
	if (err) {
		return err;
	}

	fprintf(stdout, "Disconnect Complete.\n");

	return 0;
}


int 
main(int argc, char **argv) {

	struct rdma_cb *cb;
	int err = 0;
	if (argc == 2) { //set string for client
		strcat(RDMA_STRING,argv[1]);
	}else if (argc == 3){
		strcat(RDMA_STRING,argv[2]);
	}
	/*
 * 	 * Allocate an initialize the control block structure
 * 	 	 */
	cb = malloc(sizeof(*cb));
	if (!cb) {
		fprintf(stderr, "Could not allocate memory for the control block\n");
		return -1;
	}
	memset(cb, 0, sizeof(*cb));

	/*
 * 	 * Validate the command line
 * 	 	 */
	int port;
	if (argc == 2) {
		fprintf(stdout, "Starting Server\n");
		cb->is_server = 1;
		port=atoi(argv[1]);
	} else if (argc == 3) {
		fprintf(stdout, "Running Client, Server IP %s %d\n", argv[1],atoi(argv[2]));
		port=atoi(argv[2]);
		//strcat(RDMA_STRING,argv[2]);
		err = inet_aton(argv[1], &cb->sin.sin_addr);
		if (!err) {
			fprintf(stderr, "inet_aton failed on address: %s\n", argv[1]);
			goto out;
		}
	} else {
		err = 1;
		fprintf(stderr, "Usage:\n");
		fprintf(stderr, "\t%s \t\t Start the server\n", argv[0]);
		fprintf(stderr, "\t%s <IP addr> \t Connect to the server at <IP addr>\n", argv[0]);
		goto out;
	}

	/*
 * 	 * Server will bind to this port and client will connect to it.
 * 	 	 */
	cb->sin.sin_family = AF_INET;
	//cb->sin.sin_port = SRV_RR_PORT;
	cb->sin.sin_port = port;
	/*
 * 	 * Run client/server specific code
 * 	 	 */
	if (cb->is_server) {
		err = do_server(cb);
	} else {
		err = do_client(cb);
	}

out:
	cleanup_common(cb);
	return err;
}


