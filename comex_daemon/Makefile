EXERCISES = ibdevs query_dev qp_att rdma_both requester rdma_quit COMEX_Daemon COMEX_Client

CC=gcc

CFLAGS = -Wall -g 
LIBS= -libverbs 
EXTRA_LIBS = -lrdmacm

all: $(EXERCISES)

ibdevs: ibdevs.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS)

query_dev: query_dev.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS)

qp_att: qp_att.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS)

rdma_both: rdma_both.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS) $(EXTRA_LIBS)
	
rdma_quit: rdma_quit.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS) $(EXTRA_LIBS)

COMEX_Client: COMEX_Client.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS) $(EXTRA_LIBS)
	
COMEX_Daemon: COMEX_Daemon.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS) $(EXTRA_LIBS)

requester: requester.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS) $(EXTRA_LIBS)

multicast: multicast.c
	$(CC) -o $@ $< $(CFLAGS) $(LIBS) $(EXTRA_LIBS)

clean:
	rm -f $(EXERCISES)

