#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/error.h>

static struct E1000 *base;

struct tx_desc tx_descs[TX_DESC_NUM];
char tx_buffer[TX_DESC_NUM][TX_PKT_SIZE];
//#define N_TXDESC (PGSIZE / sizeof(struct tx_desc))

int
e1000_tx_init()
{
	// Allocate one page for descriptors

	// Initialize all descriptors

	// Set hardward registers
	// Look kern/e1000.h to find useful definations
	memset(tx_descs, 0, sizeof(tx_descs));
	for(int i = 0; i < TX_DESC_NUM; i++){
		tx_descs[i].addr = PADDR(tx_buffer[i]);
		tx_descs[i].status |= E1000_RX_STATUS_DD;
	}

	//init five describ register
	base->TDLEN = sizeof(tx_descs);
	base->TDBAL = PADDR(tx_buffer);
	base->TDBAH = 0;
	base->TDH = 0;
	base->TDT = 0;

	//Initialize the Transmit Control Register
	base->TCTL |= E1000_TCTL_EN;
	base->TCTL |= E1000_TCTL_PSP;
	base->TCTL &= ~0xff0;
	base->TCTL |= E1000_TCTL_CT_ETHER;
	base->TCTL &= ~0x3ff000;
	base->TCTL |= E1000_TCTL_COLD_FULL_DUPLEX;



	return 0;
}

struct rx_desc *rx_descs;
#define N_RXDESC (PGSIZE / sizeof(struct rx_desc))

int
e1000_rx_init()
{
	// Allocate one page for descriptors

	// Initialize all descriptors
	// You should allocate some pages as receive buffer

	// Set hardward registers
	// Look kern/e1000.h to find useful definations

	return 0;
}

int
pci_e1000_attach(struct pci_func *pcif)
{
	// Enable PCI function
	// Map MMIO region and save the address in 'base;
	pci_func_enable(pcif);
	base = (struct E1000*)mmio_map_region(pcif->reg_base[0], pcif->reg_size[0]);
	cprintf("pci_e1000_attach: e1000 status -- %08x\n", base->STATUS);
	e1000_tx_init();
	e1000_rx_init();
	return 0;
}

int
e1000_tx(const void *buf, uint32_t len)
{
	// Send 'len' bytes in 'buf' to ethernet
	// Hint: buf is a kernel virtual address

	return 0;
}

int
e1000_rx(void *buf, uint32_t len)
{
	// Copy one received buffer to buf
	// You could return -E_AGAIN if there is no packet
	// Check whether the buf is large enough to hold
	// the packet
	// Do not forget to reset the decscriptor and
	// give it back to hardware by modifying RDT

	return 0;
}
