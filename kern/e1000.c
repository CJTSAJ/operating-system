#include <kern/e1000.h>
#include <kern/pmap.h>
#include <inc/string.h>
#include <inc/error.h>

#define N_TXDESC (PGSIZE / sizeof(struct tx_desc))

static volatile struct E1000 *base;

struct tx_desc *tx_descs;
char tx_buffer[N_TXDESC][TX_PKT_SIZE];
int
e1000_tx_init()
{
	// Allocate one page for descriptors

	// Initialize all descriptors

	// Set hardward registers
	// Look kern/e1000.h to find useful definations
	struct PageInfo *pp = page_alloc(ALLOC_ZERO);
	tx_descs = (struct tx_desc *)page2kva(pp);
	//memset(tx_descs, 0, sizeof(tx_descs));
	memset(tx_buffer, 0, sizeof(tx_buffer));
	for(int i = 0; i < N_TXDESC; i++){
		tx_descs[i].addr = PADDR(tx_buffer[i]);
		tx_descs[i].status |= E1000_TX_STATUS_DD;
	}

	//init five describ register
	base->TDLEN = PGSIZE;
	base->TDBAL = PADDR(tx_descs);
	base->TDBAH = 0;
	base->TDH = 0;
	base->TDT = 0;

	//Initialize the Transmit Control Register
	base->TCTL |= E1000_TCTL_EN | E1000_TCTL_PSP | E1000_TCTL_CT_ETHER;
	base->TCTL |= E1000_TCTL_COLD_FULL_DUPLEX;
	base->TIPG = E1000_TIPG_DEFAULT;

	return 0;
}

struct rx_desc *rx_descs;
#define N_RXDESC (PGSIZE / sizeof(struct rx_desc))
char rx_buffer[N_RXDESC][2048];


int
e1000_rx_init()
{
	// Allocate one page for descriptors

	// Initialize all descriptors
	// You should allocate some pages as receive buffer

	// Set hardward registers
	// Look kern/e1000.h to find useful definations

	struct PageInfo *pp = page_alloc(ALLOC_ZERO);
	rx_descs = (struct rx_desc *)page2kva(pp);

	//memset(rx_descs, 0, sizeof(rx_descs));
	memset(rx_buffer, 0, sizeof(rx_buffer));

	for(int i = 0; i < N_RXDESC; i++)
		rx_descs[i].addr = PADDR(rx_buffer[i]);

	base->RDLEN = PGSIZE;
	base->RDBAL = PADDR(rx_descs);
	base->RDBAH = 0;
	base->RDH = 1;
	base->RDT = 0;

	base->RCTL = E1000_RCTL_EN | E1000_RCTL_SECRC | E1000_RCTL_BSIZE_2048;
	base->RAH = QEMU_MAC_HIGH;
	base->RAL = QEMU_MAC_LOW;
	//base->IMS  = 0xdf;
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
	if(!buf || len < 0 || len > TX_PKT_SIZE)
		return -E_INVAL;

	uint32_t tdt = base->TDT;

	if(!(tx_descs[tdt].status & E1000_TX_STATUS_DD))
		return -E_AGAIN;

	memset(tx_buffer[tdt], 0, TX_PKT_SIZE);
	memmove(tx_buffer[tdt], buf, len);

	tx_descs[tdt].length = len;
	tx_descs[tdt].cmd |= E1000_TX_CMD_RS;
	tx_descs[tdt].cmd |= E1000_TX_CMD_EOP;
	tx_descs[tdt].status &= ~E1000_TX_STATUS_DD;

	base->TDT = (tdt + 1) % N_TXDESC;
	//cprintf("e1000_tx tdt: %d  tdh: %d------------\n", base->TDT, base->TDH);
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
	if (!buf)
		return -E_INVAL;

	uint32_t rdt = (base->RDT + 1) % N_RXDESC;
	if (!(rx_descs[rdt].status & E1000_RX_STATUS_DD))
		return -E_AGAIN;

	while (rdt == base->RDT);
	len = rx_descs[rdt].length;
	memmove(buf, rx_buffer[rdt], len);
	rx_descs[rdt].status &= ~E1000_RX_STATUS_DD;
	rx_descs[rdt].status &= ~E1000_RX_STATUS_EOP;
	base->RDT = rdt;
	return len;
}
