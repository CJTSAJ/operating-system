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

	// int32_t ipc_recv(envid_t *from_env_store, void *pg, int *perm_store)
	// int sys_net_send(const void *buf, uint32_t len)
	// (thisenv->env_ipc_from != ns_envid)
	// struct jif_pkt { int jp_len; char jp_data[0]; };

	/*int r;
	uint32_t from_env;
	int perm;

	while (1) {
		r = ipc_recv((envid_t *)&from_env, &nsipcbuf, &perm);
		if (r != NSREQ_OUTPUT) {
			cprintf("not a NSREQ_OUTPUT\n");
			continue;
		}

		struct jif_pkt *pkt = &(nsipcbuf.pkt);
		while ((r = sys_net_send(pkt->jp_data, pkt->jp_len)) < 0) {
			sys_yield();
		}
	}*/

	int r;
  char *data;
  int len;

  while (1) {
    if((r = sys_ipc_recv(&nsipcbuf)) < 0)
      panic("%e",r);

    if ((thisenv->env_ipc_from != ns_envid) || (thisenv->env_ipc_value != NSREQ_OUTPUT))
      continue;

    while((r = sys_net_send(nsipcbuf.pkt.jp_data, nsipcbuf.pkt.jp_len)) < 0)
      if(r != -E_AGAIN)
        panic("%e", r);
  }
}
