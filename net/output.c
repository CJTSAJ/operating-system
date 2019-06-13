#include "ns.h"

extern union Nsipc nsipcbuf;

void
output(envid_t ns_envid)
{
	binaryname = "ns_output";

	// LAB 6: Your code here:
	// 	- read a packet request (using ipc_recv)
	//	- send the packet to the device driver (using sys_net_send)
	//	do the above things in a loop

	int r;
	envid_t from_env;
	int perm;

	while(1){
		if((r = ipc_recv(&from_env, &nsipcbuf, &perm)) < 0)
			panic("ipc_recv: failed %d", r);

		if(r == NSREQ_OUTPUT && from_env == ns_envid){
			struct jif_pkt* pkt = &nsipcbuf.pkt;
			while((r = sys_net_send(pkt->jp_data, pkt->jp_len)) < 0){
				if(r != -E_AGAIN)
					panic("sys_net_send failed: %d\n", r);
			}
		}
	}
}
