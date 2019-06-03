#include <inc/lib.h>
#include <inc/elf.h>

#define UTEMP2USTACK(addr)	((void*) (addr) + (USTACKTOP - PGSIZE) - UTEMP)
#define UTEMP2			(UTEMP + PGSIZE)
#define UTEMP3			(UTEMP2 + PGSIZE)

// Helper functions for spawn.
static int init_stack(envid_t child, const char **argv, uintptr_t *init_esp);
static int map_segment(envid_t child, uintptr_t va, size_t memsz,
		       int fd, size_t filesz, off_t fileoffset, int perm);
static int copy_shared_pages(envid_t child);

int
exec(const char *prog, const char **argv)
{
	unsigned char elf_buf[512];
	struct Trapframe child_tf;
	envid_t child;

	int fd, i, r;
	struct Elf *elf;
	struct Proghdr *ph;
	int perm;

	if ((r = open(prog, O_RDONLY)) < 0)
		return r;
	fd = r;

	// Read elf header
	elf = (struct Elf*) elf_buf;
	if (readn(fd, elf_buf, sizeof(elf_buf)) != sizeof(elf_buf)
	    || elf->e_magic != ELF_MAGIC) {
		close(fd);
		cprintf("elf magic %08x want %08x\n", elf->e_magic, ELF_MAGIC);
		return -E_NOT_EXEC;
	}

	// Create new child environment
	if ((r = sys_exofork()) < 0)
		return r;
	child = r;

	// Set up trap frame, including initial stack.
	child_tf = envs[ENVX(child)].env_tf;
	child_tf.tf_eip = elf->e_entry;

	if ((r = init_stack(child, argv, &child_tf.tf_esp)) < 0)
		return r;

	// Set up program segments as defined in ELF header.
	ph = (struct Proghdr*) (elf_buf + elf->e_phoff);
	for (i = 0; i < elf->e_phnum; i++, ph++) {
		if (ph->p_type != ELF_PROG_LOAD)
			continue;
		perm = PTE_P | PTE_U;
		if (ph->p_flags & ELF_PROG_FLAG_WRITE)
			perm |= PTE_W;
		if ((r = map_segment(child, ph->p_va, ph->p_memsz,
				     fd, ph->p_filesz, ph->p_offset, perm)) < 0)
			goto error;
	}
	close(fd);
	fd = -1;

	// Copy shared library state.
	if ((r = copy_shared_pages(child)) < 0)
		panic("copy_shared_pages: %e", r);

	child_tf.tf_eflags |= FL_IOPL_3;   // devious: see user/faultio.c
	if ((r = sys_env_set_trapframe(child, &child_tf)) < 0)
		panic("sys_env_set_trapframe: %e", r);

	if ((r = sys_env_set_status(child, ENV_RUNNABLE)) < 0)
		panic("sys_env_set_status: %e", r);

  //differ from spawn, swap child and father, then destroy the child
  if((r = sys_env_exchange(child)) < 0)
    panic("sys_env_exchange: %e", r);

  //sys_env_destroy(child);
	return child;

error:
	sys_env_destroy(child);
	close(fd);
	return r;
}

//same with spawnl
int
execl(const char *prog, const char *arg0, ...)
{
	// We calculate argc by advancing the args until we hit NULL.
	// The contract of the function guarantees that the last
	// argument will always be NULL, and that none of the other
	// arguments will be NULL.
	int argc=0;
	va_list vl;
	va_start(vl, arg0);
	while(va_arg(vl, void *) != NULL)
		argc++;
	va_end(vl);

	// Now that we have the size of the args, do a second pass
	// and store the values in a VLA, which has the format of argv
	const char *argv[argc+2];
	argv[0] = arg0;
	argv[argc+1] = NULL;

	va_start(vl, arg0);
	unsigned i;
	for(i=0;i<argc;i++)
		argv[i+1] = va_arg(vl, const char *);
	va_end(vl);
	return exec(prog, argv);
}

// Set up the initial stack page for the new child process with envid 'child'
// using the arguments array pointed to by 'argv',
// which is a null-terminated array of pointers to null-terminated strings.
//
// On success, returns 0 and sets *init_esp
// to the initial stack pointer with which the child should start.
// Returns < 0 on failure.
static int
init_stack(envid_t child, const char **argv, uintptr_t *init_esp)
{
	size_t string_size;
	int argc, i, r;
	char *string_store;
	uintptr_t *argv_store;

	// Count the number of arguments (argc)
	// and the total amount of space needed for strings (string_size).
	string_size = 0;
	for (argc = 0; argv[argc] != 0; argc++)
		string_size += strlen(argv[argc]) + 1;

	// Determine where to place the strings and the argv array.
	// Set up pointers into the temporary page 'UTEMP'; we'll map a page
	// there later, then remap that page into the child environment
	// at (USTACKTOP - PGSIZE).
	// strings is the topmost thing on the stack.
	string_store = (char*) UTEMP + PGSIZE - string_size;
	// argv is below that.  There's one argument pointer per argument, plus
	// a null pointer.
	argv_store = (uintptr_t*) (ROUNDDOWN(string_store, 4) - 4 * (argc + 1));

	// Make sure that argv, strings, and the 2 words that hold 'argc'
	// and 'argv' themselves will all fit in a single stack page.
	if ((void*) (argv_store - 2) < (void*) UTEMP)
		return -E_NO_MEM;

	// Allocate the single stack page at UTEMP.
	if ((r = sys_page_alloc(0, (void*) UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
		return r;


	//	* Initialize 'argv_store[i]' to point to argument string i,
	//	  for all 0 <= i < argc.
	//	  Also, copy the argument strings from 'argv' into the
	//	  newly-allocated stack page.
	//
	//	* Set 'argv_store[argc]' to 0 to null-terminate the args array.
	//
	//	* Push two more words onto the child's stack below 'args',
	//	  containing the argc and argv parameters to be passed
	//	  to the child's umain() function.
	//	  argv should be below argc on the stack.
	//	  (Again, argv should use an address valid in the child's
	//	  environment.)
	//
	//	* Set *init_esp to the initial stack pointer for the child,
	//	  (Again, use an address valid in the child's environment.)
	for (i = 0; i < argc; i++) {
		argv_store[i] = UTEMP2USTACK(string_store);
		strcpy(string_store, argv[i]);
		string_store += strlen(argv[i]) + 1;
	}
	argv_store[argc] = 0;
	assert(string_store == (char*)UTEMP + PGSIZE);

	argv_store[-1] = UTEMP2USTACK(argv_store);
	argv_store[-2] = argc;

	*init_esp = UTEMP2USTACK(&argv_store[-2]);

	// After completing the stack, map it into the child's address space
	// and unmap it from ours!
	if ((r = sys_page_map(0, UTEMP, child, (void*) (USTACKTOP - PGSIZE), PTE_P | PTE_U | PTE_W)) < 0)
		goto error;
	if ((r = sys_page_unmap(0, UTEMP)) < 0)
		goto error;

	return 0;

error:
	sys_page_unmap(0, UTEMP);
	return r;
}

static int
map_segment(envid_t child, uintptr_t va, size_t memsz,
	int fd, size_t filesz, off_t fileoffset, int perm)
{
	int i, r;
	void *blk;

	//cprintf("map_segment %x+%x\n", va, memsz);

	if ((i = PGOFF(va))) {
		va -= i;
		memsz += i;
		filesz += i;
		fileoffset -= i;
	}

	for (i = 0; i < memsz; i += PGSIZE) {
		if (i >= filesz) {
			// allocate a blank page
			if ((r = sys_page_alloc(child, (void*) (va + i), perm)) < 0)
				return r;
		} else {
			// from file
			if ((r = sys_page_alloc(0, UTEMP, PTE_P|PTE_U|PTE_W)) < 0)
				return r;
			if ((r = seek(fd, fileoffset + i)) < 0)
				return r;
			if ((r = readn(fd, UTEMP, MIN(PGSIZE, filesz-i))) < 0)
				return r;
			if ((r = sys_page_map(0, UTEMP, child, (void*) (va + i), perm)) < 0)
				panic("spawn: sys_page_map data: %e", r);
			sys_page_unmap(0, UTEMP);
		}
	}
	return 0;
}

// Copy the mappings for shared pages into the child address space.
static int
copy_shared_pages(envid_t child)
{
	// LAB 5: Your code here.
	int r;

	for(uint32_t tmp_addr = UTEXT; tmp_addr < USTACKTOP; tmp_addr += PGSIZE){
		if((uvpd[PDX(tmp_addr)] & PTE_P) && (uvpt[PGNUM(tmp_addr)] & (PTE_P | PTE_U | PTE_SHARE)) == (PTE_P | PTE_U | PTE_SHARE)){
			if((r = sys_page_map(0, (void*)tmp_addr, child, (void*)tmp_addr, uvpt[PGNUM(tmp_addr)] & PTE_SYSCALL)) < 0)
				panic("sys_page_map: %e\n", r);
		}
	}
	return 0;
}
