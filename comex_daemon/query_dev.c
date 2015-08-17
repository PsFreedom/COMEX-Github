#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include <infiniband/verbs.h>

int
main(int argc, char **argv)
{
	struct ibv_device **dl, *ibdev;
        struct ibv_context *ibcxt;
	struct ibv_device_attr dev_attrs;
	struct ibv_port_attr port_attrs;
	int num_devices, err, i;
	int rc = 0;

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
			rc = 1;
			goto out;
		}

		if (strncmp(name, argv[1], sizeof(name)) == 0) {
			ibdev = tmp_dl;
			break;
		}
	}

	/*
 * 	 * if ibdev is still null we didnt find the device
 * 	 	 * specified on the command line.
 * 	 	 	 */
	if (!ibdev) {
		printf("Unable to find device: %s\n", argv[1]);
		rc = 1;
		goto out;
	}

	/*
 * 	 * Open the Device
 * 	 	 */
	ibcxt = ibv_open_device(ibdev);
	if (!ibcxt) {
		perror("ibv_open_device");
		rc = 1;
		goto out;
	}

	/*
 * 	 * Query the Device
 * 	 	 */
	err = ibv_query_device(ibcxt, &dev_attrs);
	if (err) {
		perror("ibv_query_device");
		rc = 1;
		goto out2;
	}

	/*
 * 	 * Print out device attributes
 * 	 	 * 
 * 	 	 	 */
	printf("Device Attributes for %s\n", argv[1]);
	printf("\t FW Version: %s\n", dev_attrs.fw_ver);
	printf("\t Vendor ID: 0x%x\n", dev_attrs.vendor_id);
	printf("\t Number of Ports: %d\n", dev_attrs.phys_port_cnt);

	/*
 * 	 * Query port 1
 * 	 	 */
	err = ibv_query_port(ibcxt, 1, &port_attrs);
	if (err) {
		perror("ibv_query_port");
		rc = 1;
		goto out2;
	}

	/*
 * 	 * Print out port attributes
 * 	 	 * 
 * 	 	 	 */
	printf("Port 1 Attributes for %s\n", argv[1]); 
	printf("\t Logical Port State: %s\n", ibv_port_state_str(port_attrs.state));
	printf("\t LID: %d\n", port_attrs.lid);
	
	
	/*
 * 	 * Be sure and clean up 
 * 	 	 */
out2:
	ibv_close_device(ibcxt);
out:
	ibv_free_device_list(dl);

	return rc;
}

