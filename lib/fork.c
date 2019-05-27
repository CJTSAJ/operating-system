// implement fork from user space

#include <inc/string.h>
#include <inc/lib.h>

// PTE_COW marks copy-on-write page table entries.
// It is one of the bits explicitly allocated to user processes (PTE_AVAIL).
#define PTE_COW		0x800
extern void _pgfault_upcall(void);
//
// Custom page fault handler - if faulting page is copy-on-write,
// map in our own private writable copy.
//
static void
pgfault(struct UTrapframe *utf)
{
	void *addr = (void *) utf->utf_fault_va;
	uint32_t err = utf->utf_err;
	int r;

	// Check that the faulting access was (1) a write, and (2) to a
	// copy-on-write page.  If not, panic.
	// Hint:
	//   Use the read-only page table mappings at uvpt
	//   (see <inc/memlayout.h>).

	// LAB 4: Your code here.
	if(!(err & FEC_WR) || !(uvpd[PDX(addr)] & PTE_P)
		|| !((uvpt[PGNUM(addr)] & (PTE_P | PTE_COW)) == (PTE_P | PTE_COW)))
		panic("not copy-on-write page\n");
	// Allocate a new page, map it at a temporary location (PFTEMP),
	// copy the data from the old page to the new page, then move the new
	// page to the old page's address.
	// Hint:
	//   You should make three system calls.

	// LAB 4: Your code here.
	addr = ROUNDDOWN(addr, PGSIZE);
	r = sys_page_alloc(0, PFTEMP, PTE_P|PTE_U|PTE_W);
	if(r < 0)
		panic("pgfault: sys_page_alloc failed\n");
	//copy page
	memmove(PFTEMP, addr, PGSIZE);
	r = sys_page_map(0, PFTEMP, 0, addr, PTE_P|PTE_U|PTE_W);
	if(r < 0)
		panic("pgfault: sys_page_alloc failed\n");
	r = sys_page_unmap(0, PFTEMP);
	if(r < 0)
		panic("pgfault: sys_page_unmap failed\n");
	//panic("pgfault not implemented");
}

//
// Map our virtual page pn (address pn*PGSIZE) into the target envid
// at the same virtual address.  If the page is writable or copy-on-write,
// the new mapping must be created copy-on-write, and then our mapping must be
// marked copy-on-write as well.  (Exercise: Why do we need to mark ours
// copy-on-write again if it was already copy-on-write at the beginning of
// this function?)
//
// Returns: 0 on success, < 0 on error.
// It is also OK to panic on error.
//
static int
duppage(envid_t envid, unsigned pn)
{
	int r;
	void* va = (void*)(pn * PGSIZE);
	if(uvpt[pn] & (PTE_W | PTE_COW)){
		r = sys_page_map(0, va, envid, va, PTE_U | PTE_P | PTE_COW);
		if(r < 0)
			panic("duppage: sys_page_map failed\n");
		r = sys_page_map(0, va, 0, va, PTE_U | PTE_P | PTE_COW);
		if(r < 0)
			panic("duppage: sys_page_map failed\n");
	}
	else{
		r = sys_page_map(0, va, envid, va, PTE_U | PTE_P);
		if(r < 0)
			panic("duppage: sys_page_map failed\n");
	}
	// LAB 4: Your code here.
	//panic("duppage not implemented");
	return 0;
}

//
// User-level fork with copy-on-write.
// Set up our page fault handler appropriately.
// Create a child.
// Copy our address space and page fault handler setup to the child.
// Then mark the child as runnable and return.
//
// Returns: child's envid to the parent, 0 to the child, < 0 on error.
// It is also OK to panic on error.
//
// Hint:
//   Use uvpd, uvpt, and duppage.
//   Remember to fix "thisenv" in the child process.
//   Neither user exception stack should ever be marked copy-on-write,
//   so you must allocate a new page for the child's user exception stack.
//
envid_t
fork(void)
{
	// LAB 4: Your code here.
	set_pgfault_handler(pgfault);
	envid_t envid = sys_exofork();

	if(envid < 0)
		panic("fork: sys_exofork failed\n");
	else if(envid == 0){ //this is child
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}

	uintptr_t i = UTEXT;
	for(; i < USTACKTOP; i += PGSIZE){
		if(uvpd[PDX(i)] & PTE_P && (uvpt[PGNUM(i)] & (PTE_P | PTE_U)) == (PTE_P | PTE_U))
			duppage(envid, PGNUM(i));
	}

	int ret = sys_page_alloc(envid, (void *)(UXSTACKTOP-PGSIZE), PTE_U|PTE_W|PTE_P);
	if(ret < 0)
		panic("fork: sys_page_alloc failed\n");
	ret = sys_env_set_pgfault_upcall(envid, _pgfault_upcall);
	if(ret < 0)
		panic("fork: sys_env_set_pgfault_upcall failed\n");
	ret = sys_env_set_status(envid, ENV_RUNNABLE);
	if(ret < 0)
		panic("fork: sys_env_set_status failed\n");

	return envid;
	//panic("fork not implemented");
}

// Challenge!
int
sfork(void)
{
	/*panic("sfork not implemented");
	return -E_INVAL;*/

	int r;
	set_pgfault_handler(pgfault);

	envid_t child = sys_exofork();

	if(child < 0)
		panic("sfork: sys_exofork failed\n");
	if(child == 0){
		thisenv = &envs[ENVX(sys_getenvid())];
		return 0;
	}


	//now it is parent
	//share memory except stack
	extern unsigned char end[];
	uintptr_t i = UTEXT;
	for(; i < (uintptr_t)end; i+=PGSIZE){
		if((uvpd[PDX(i)] & PTE_P) && (uvpt[PGNUM(i)] & PTE_P)){
			int perm = uvpt[PGNUM(i)] & (PTE_P | PTE_U | PTE_W | PTE_COW | PTE_SYSCALL);
			r = sys_page_map(0, (void*)i, child, (void*)i, perm);
			if(r < 0)
				panic("sfork: sys_page_map failed\n");
		}
	}

	//stack---COW
	for (i = (uintptr_t)ROUNDDOWN(&i, PGSIZE); i < USTACKTOP; i += PGSIZE)
		duppage(child, PGNUM(i));

	//alloc new page for exception statck
	r = sys_page_alloc(child, (void *)(UXSTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W);
	if(r < 0)
		panic("sfork: sys_page_alloc failed\n");

	//set page fault handler
	extern void _pgfault_upcall(void);
	r = sys_env_set_pgfault_upcall(child, _pgfault_upcall);
	if (r < 0)
		panic("sfork: sys_env_set_pgfault_upcall failed\n");

	r = sys_env_set_status(child, ENV_RUNNABLE);
	if (r < 0)
		panic("sfork: sys_env_set_statusfailed");

	return child;
}
