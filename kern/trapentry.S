/* See COPYRIGHT for copyright information. */

#include <inc/mmu.h>
#include <inc/memlayout.h>
#include <inc/trap.h>

#include <kern/picirq.h>


###################################################################
# exceptions/interrupts
###################################################################

/* TRAPHANDLER defines a globally-visible function for handling a trap.
 * It pushes a trap number onto the stack, then jumps to _alltraps.
 * Use TRAPHANDLER for traps where the CPU automatically pushes an error code.
 *
 * You shouldn't call a TRAPHANDLER function from C, but you may
 * need to _declare_ one in C (for instance, to get a function pointer
 * during IDT setup).  You can declare the function with
 *   void NAME();
 * where NAME is the argument passed to TRAPHANDLER.
 */
#define TRAPHANDLER(name, num)						\
	.globl name;		/* define global symbol for 'name' */	\
	.type name, @function;	/* symbol type is function */		\
	.align 2;		/* align function definition */		\
	name:			/* function starts here */		\
	pushl $(num);							\
	jmp _alltraps

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps

.text

/*
 * Lab 3: Your code here for generating entry points for the different traps.
 */
TRAPHANDLER_NOEC(ENTRY_DIVIDE, T_DIVIDE) //0
TRAPHANDLER_NOEC(ENTRY_DEBUG, T_DEBUG) //1
TRAPHANDLER_NOEC(ENTRY_NMI, T_NMI) //2
TRAPHANDLER_NOEC(ENTRY_BRKPT, T_BRKPT) //3
TRAPHANDLER_NOEC(ENTRY_OFLOW, T_OFLOW) //4
TRAPHANDLER_NOEC(ENTRY_BOUND, T_BOUND) //5
TRAPHANDLER_NOEC(ENTRY_ILLOP, T_ILLOP) //6
TRAPHANDLER_NOEC(ENTRY_DEVICE, T_DEVICE) //7
TRAPHANDLER(ENTRY_DBLFLT, T_DBLFLT) //8
//TRAPHANDLER_NOEC(ENTRY_COPROC, T_COPROC) //9
TRAPHANDLER(ENTRY_TSS, T_TSS) //10
TRAPHANDLER(ENTRY_SEGNP, T_SEGNP) //11
TRAPHANDLER(ENTRY_STACK, T_STACK) //12
TRAPHANDLER(ENTRY_GPFLT, T_GPFLT) //13
TRAPHANDLER(ENTRY_PGFLT, T_PGFLT) //14
TRAPHANDLER_NOEC(ENTRY_FPERR, T_FPERR) //16
TRAPHANDLER_NOEC(ENTRY_ALIGN, T_ALIGN) //17
TRAPHANDLER_NOEC(ENTRY_MCHK, T_MCHK) //18
TRAPHANDLER_NOEC(ENTRY_SIMDERR, T_SIMDERR) //19
TRAPHANDLER_NOEC(ENTRY_SYSCALL, T_SYSCALL) //48

/*.globl sysenter_handler;
.type sysenter_handler, @function;
.align 2;
sysenter_handler:
  pushl %edi
  pushl %ebx
  pushl %ecx
  pushl %edx
  pushl %eax
  call syscall
  movl %ebp, %ecx
  movl %esi, %edx
  sysexit*/

/*
 * Lab 3: Your code here for _alltraps
 */
_alltraps:
  pushw $0
  pushw %ds
  pushw $0
  pushw %es
  pushal
  pushw $GD_KD
  popw %ds
  pushw $GD_KD
  popw %es
  pushl %esp
  call trap
