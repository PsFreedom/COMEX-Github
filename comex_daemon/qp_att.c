#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <infiniband/verbs.h>

#define CQ_DEPTH 200

#define MAX_SEND_WR 2000
#define MAX_SEND_SGE 20

#define MAX_RECV_WR 974
#define MAX_RECV_SGE 3

int
main(int argc, char **argv)
{
	struct ibv_device **dl, *ibdev;
        struct ibv_context *ibcxt;
	struct ibv_pd *pd;
	struct ibv_cq *cq;
	struct ibv_qp *qp;
	struct ibv_qp_init_attr qp_init_attrs;
	struct ibv_qp_attr qp_attrs;
	int num_devices, err, i;

        /*
 *          * Validate the Command Line
 *                   */
        if (argc != 2) {
                printf("Usage: %s <device name>\n", argv[0]);
                return -1;
        }

	/*
 * 	 * Get the list of RDMA devices in the system
 * 	 	 */
	dl = ibv_get_device_list(&num_devices);
	if (!dl) {
		perror("ibv_get_device_list");
		return errno; 
	}

	/*
 * 	 * If no devices were found then there is
 * 	 	 * nothing else to do.
 * 	 	 	 */
	if (num_devices == 0) {
		printf("No RDMA capable devices were found!\n");
		goto out;
	} 

	/*
 * 	 * Walk the device list and find the one specified on
 * 	 	 * the command line.
 * 	 	 	 */
	ibdev = NULL;
	for (i=0; i < num_devices; i++) {
		
		struct ibv_device *tmp_dl;
		const char *name;

		tmp_dl = dl[i];
	
		name = ibv_get_device_name(tmp_dl);
		if (!name) {
			perror("ibv_get_device_name");
			goto out;
		}

		if (strncmp(name, argv[1], sizeof(name)) == 0) {
			ibdev = tmp_dl;
			printf("%s",name);
			//break;
		}
	}

	/*
 * 	 * if ibdev is still null we didnt find the device
 * 	 	 * specified on the command line.
 * 	 	 	 */
	if (!ibdev) {
		printf("Unable to find device: %s\n", argv[1]);
		goto out;
	}

	/*
 * 	 * Open the Device
 * 	 	 */
	ibcxt = ibv_open_device(ibdev);
	if (!ibcxt) {
		perror("ibv_open_device");
		goto out;
	}

	/*
 * 	 * Allocate the Protection Domain
 * 	 	 */
	pd = ibv_alloc_pd(ibcxt);
	if (!pd) {
		printf("Unable to allocate a Protection Domain\n");
		goto out2;
	}

	/*
 * 	 * Create a Completion Queue
 * 	 	 */
	cq = ibv_create_cq(ibcxt, CQ_DEPTH, NULL, NULL, 0);
	if (!cq) {
		printf("Unable to allocate Completion Queue\n");
		goto out3;
	}

	printf("Created CQ with size: %d, but actual size is: %d\n", CQ_DEPTH, cq->cqe);

	/*
 * 	 * Initialize the QP attributes
 * 	 	 */
	memset(&qp_init_attrs, 0, sizeof(qp_init_attrs));
	qp_init_attrs.send_cq = cq;
	qp_init_attrs.recv_cq = cq;
	qp_init_attrs.qp_type = IBV_QPT_RC;

	qp_init_attrs.cap.max_send_wr = MAX_SEND_WR;
	qp_init_attrs.cap.max_send_sge = MAX_SEND_SGE;

	qp_init_attrs.cap.max_recv_wr = MAX_RECV_WR;
	qp_init_attrs.cap.max_recv_sge = MAX_RECV_SGE;

	/*
 * 	 * Create a Queue Pair
 * 	 	 */
	qp = ibv_create_qp(pd, &qp_init_attrs);
	if (!qp) {
		printf("Unable to allocate Queue Pair\n");
		goto out4;
	}

	err = ibv_query_qp(qp, &qp_attrs, IBV_QP_CAP, &qp_init_attrs);
	if (err) {
		perror("ibv_query_qp");
		goto out5;
	}

	printf("Created QP number: 0x%x\n", qp->qp_num);
	printf("\t Specified max_send_wr = %d, but actual max_send_wr is = %d\n", MAX_SEND_WR, qp_init_attrs.cap.max_send_wr);
	printf("\t Specified max_send_sge = %d, but actual max_send_sge is = %d\n", MAX_SEND_SGE, qp_init_attrs.cap.max_send_sge);
	printf("\t Specified max_recv_wr = %d, but actual max_recv_wr is = %d\n", MAX_RECV_WR, qp_init_attrs.cap.max_recv_wr);
	printf("\t Specified max_recv_sge = %d, but actual max_recv_sge is = %d\n", MAX_RECV_SGE, qp_init_attrs.cap.max_recv_sge);

	/*
 * 	 * Be sure and clean up 
 * 	 	 */
out5:
	ibv_destroy_qp(qp);
out4:
	ibv_destroy_cq(cq);
out3:
	ibv_dealloc_pd(pd);
out2:
	ibv_close_device(ibcxt);
out:
	ibv_free_device_list(dl);

	return 0;
}

