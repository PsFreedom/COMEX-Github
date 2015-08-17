#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <infiniband/verbs.h>
#include <infiniband/arch.h>

int
main()
{
	struct ibv_device **dl;
	int num_devices, i;

	/*
 * 	 * Get the list of RDMA devices in the system
 * 	 	 */
	dl = ibv_get_device_list(&num_devices);
	if (dl == NULL) {
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
 * 	 * Walk the device list and print out information about
 * 	 	 * each one in the system.
 * 	 	 	 */
	for (i=0; i < num_devices; i++) {
			
		struct ibv_device *tmp_dl;
		const char *name, *node_type;
		uint64_t guid;

		tmp_dl = dl[i];
		
		name = ibv_get_device_name(tmp_dl);
		if (name == NULL) {
			perror("ibv_get_device_name");
			goto out;
		}

		node_type = ibv_node_type_str(tmp_dl->node_type);
		guid = ibv_get_device_guid(tmp_dl);
			
		printf("Found device: %s\n", name);
		printf("\t Node Type: %s\n", node_type);
		printf("\t GUID %016llx\n", (unsigned long long)ntohll(guid));
	}

	/*
 * 	 * Be sure and clean up 
 * 	 	 */
out:
	ibv_free_device_list(dl);

	return 0;
}

