// Simple command-line kernel monitor useful for
// controlling the kernel and exploring the system interactively.

#include <inc/stdio.h>
#include <inc/string.h>
#include <inc/memlayout.h>
#include <inc/assert.h>
#include <inc/x86.h>

#include <kern/console.h>
#include <kern/monitor.h>
#include <kern/kdebug.h>

#define CMDBUF_SIZE	80	// enough for one VGA text line
#define BYTE_MASK	0xFF

struct Command {
	const char *name;
	const char *desc;
	// return -1 to force monitor to exit
	int (*func)(int argc, char** argv, struct Trapframe* tf);
};

static struct Command commands[] = {
	{ "help", "Display this list of commands", mon_help },
	{ "kerninfo", "Display information about the kernel", mon_kerninfo },
};

/***** Implementations of basic kernel monitor commands *****/

int
mon_help(int argc, char **argv, struct Trapframe *tf)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(commands); i++)
		cprintf("%s - %s\n", commands[i].name, commands[i].desc);
	return 0;
}

int
mon_time(int argc, char **argv, struct Trapframe *tf)
{
	/*unsigned long eax, edx;
 	 __asm__ volatile("rdtsc" : "=a" (eax), "=d" (edx));
  	unsigned long long timestart  = ((unsigned long long)eax) | (((unsigned long long)edx) << 32);
	
	for(int i = 0; i < argc; i++){
		runcmd(argv[i], NULL);	
	}
  	

  	__asm__ volatile("rdtsc" : "=a" (eax), "=d" (edx));
  	unsigned long long timeend  = ((unsigned long long)eax) | (((unsigned long long)edx) << 32);

  	cprintf("kerninfo cycles: %08d\n",timeend-timestart);*/
	return 0;
}
int
mon_kerninfo(int argc, char **argv, struct Trapframe *tf)
{
	extern char _start[], entry[], etext[], edata[], end[];

	cprintf("Special kernel symbols:\n");
	cprintf("  _start                  %08x (phys)\n", _start);
	cprintf("  entry  %08x (virt)  %08x (phys)\n", entry, entry - KERNBASE);
	cprintf("  etext  %08x (virt)  %08x (phys)\n", etext, etext - KERNBASE);
	cprintf("  edata  %08x (virt)  %08x (phys)\n", edata, edata - KERNBASE);
	cprintf("  end    %08x (virt)  %08x (phys)\n", end, end - KERNBASE);
	cprintf("Kernel executable memory footprint: %dKB\n",
		ROUNDUP(end - entry, 1024) / 1024);
	return 0;
}

// Lab1 only
// read the pointer to the retaddr on the stack
static uint32_t
read_pretaddr() {
    uint32_t pretaddr;
    __asm __volatile("leal 4(%%ebp), %0" : "=r" (pretaddr)); 
    return pretaddr;
}

void
do_overflow(void)
{
    cprintf("Overflow success\n");
}

void
start_overflow(void)
{
	// You should use a techique similar to buffer overflow
	// to invoke the do_overflow function and
	// the procedure must return normally.

    // And you must use the "cprintf" function with %n specifier
    // you augmented in the "Exercise 9" to do this job.

    // hint: You can use the read_pretaddr function to retrieve 
    //       the pointer to the function call return address;

    char str[256] = {};
    int nstr = 0;
   	// Your code here.
    char* pretaddr = (char*)read_pretaddr();
    int overflow_addr = (int) do_overflow;
    char tmp_ch = ' ';
    int tmp_num = 0;
    for(int i = 0; i < 4; i++){
	tmp_num = (unsigned char)(pretaddr[i]);
	cprintf("%*s%n\n", tmp_num, "", pretaddr + 4 + i);
    }

    for(int i = 0; i < 4; i++){
	tmp_num = (overflow_addr >> (8*i)) & BYTE_MASK;
	cprintf("%*s%n\n", tmp_num, "", pretaddr + i);
    }

}

void
overflow_me(void)
{
        start_overflow();
}

int
mon_backtrace(int argc, char **argv, struct Trapframe *tf)
{
	overflow_me();
	// Your code here.
	uint32_t ebp = read_ebp();
	uint32_t eip = *((uint32_t*)(ebp + 4));
	struct Eipdebuginfo eipinfo;
	uint32_t* args;
	
	char func_name[1024];
	while(ebp){
		args = (uint32_t*)(ebp + 8);
		cprintf(" eip %08x ebp %08x args %08x %08x %08x %08x %08x", eip, ebp, args[0], args[1], args[2], args[3], args[4]);
		if(debuginfo_eip(eip, &eipinfo) == 0){
			memcpy(func_name, eipinfo.eip_fn_name, eipinfo.eip_fn_namelen);
			func_name[eipinfo.eip_fn_namelen] = '\0';
			cprintf("	%s:%d %s+%d\n", eipinfo.eip_file, eipinfo.eip_line, func_name, eip - eipinfo.eip_fn_addr);
		}
		ebp = *((uint32_t*)ebp); //the last level ebp
		eip = *((uint32_t*)(ebp + 4)); //the last level eip
	}		

    	cprintf("Backtrace success\n");
	return 0;
}



/***** Kernel monitor command interpreter *****/

#define WHITESPACE "\t\r\n "
#define MAXARGS 16

static int
runcmd(char *buf, struct Trapframe *tf)
{
	int argc;
	char *argv[MAXARGS];
	int i;

	// Parse the command buffer into whitespace-separated arguments
	argc = 0;
	argv[argc] = 0;
	while (1) {
		// gobble whitespace
		while (*buf && strchr(WHITESPACE, *buf))
			*buf++ = 0;
		if (*buf == 0)
			break;

		// save and scan past next arg
		if (argc == MAXARGS-1) {
			cprintf("Too many arguments (max %d)\n", MAXARGS);
			return 0;
		}
		argv[argc++] = buf;
		while (*buf && !strchr(WHITESPACE, *buf))
			buf++;
	}
	argv[argc] = 0;

	// Lookup and invoke the command
	if (argc == 0)
		return 0;
	for (i = 0; i < ARRAY_SIZE(commands); i++) {
		if (strcmp(argv[0], commands[i].name) == 0)
			return commands[i].func(argc, argv, tf);
	}
	cprintf("Unknown command '%s'\n", argv[0]);
	return 0;
}

void
monitor(struct Trapframe *tf)
{
	char *buf;

	cprintf("Welcome to the JOS kernel monitor!\n");
	cprintf("Type 'help' for a list of commands.\n");


	while (1) {
		buf = readline("K> ");
		if (buf != NULL)
			if (runcmd(buf, tf) < 0)
				break;
	}
}
