#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>

void sched_halt(void);

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// LAB 4: Your code here.

	/*envid_t env_id = curenv == NULL ? 0 : ENVX(curenv->env_id);
  for(int i = (env_id + 1) % NENV; i != env_id; i = (i + 1) % NENV){
    if(envs[i].env_status == ENV_RUNNABLE) {
      env_run(&envs[i]);
    }
  }
  if(curenv && curenv->env_status == ENV_RUNNING){
    env_run(curenv);
  }*/

	//cprintf("sched_yield(void)-------------------\n");
	/*envid_t id;
	if (curenv)
		id = ENVX(curenv->env_id);
	else
		id = -1;

	for(int i=0;i<NENV;i++){
		id = ENVX(id+1);
		if (envs[id].env_status == ENV_RUNNABLE){
			//cprintf("sched_yeild: choose env %d to run\n",id);
			env_run(&envs[id]);
		}
	}

	if(curenv && curenv->env_cpunum==cpunum() && curenv->env_status==ENV_RUNNING){
		env_run(curenv);
	}*/
	size_t i;
	if (curenv == NULL) {
		for (i = 0; i < NENV; i++) {
			if (envs[i].env_status == ENV_RUNNABLE)
				env_run(&envs[i]);
		}
	}
	else {
		envid_t env_index = ENVX(curenv->env_id);

		for(i = env_index + 1; i != env_index; i = ENVX(i+1)){
			if(envs[i].env_status == ENV_RUNNABLE){
				env_run(&envs[i]);
			}
		}

		//if no runnable, and current is still running, take it
		if(curenv->env_cpunum == cpunum() && curenv->env_status==ENV_RUNNING)
			env_run(curenv);
	}

	/*for (i = 0; i < NENV; i++) {
		if (envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING)
			break;
	}
	if (i == NENV) {
		cprintf("No more runnable environments!\n");
		while (1)
			monitor(NULL);
	}

	// Run this CPU's idle environment when nothing else is runnable.
	idle = &envs[cpunum()];
	if (!(idle->env_status == ENV_RUNNABLE || idle->env_status == ENV_RUNNING))
		panic("CPU %d: No idle environment!", cpunum());
	env_run(idle);*/
	/*envid_t env_index = ENVX(curenv->env_id);

	for(int i = (env_index + 1) % NENV; (i % NENV) != env_index; i++){
		if(envs[i].env_status == ENV_RUNNABLE){
			env_run(&envs[i]);
		}
	}

	//if no runnable, and current is still running, take it
	if(curenv->env_status == ENV_RUNNING){
		env_run(curenv);
	}*/
	// sched_halt never returns
	sched_halt();
}

// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING))
			break;
	}
	if (i == NENV) {
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile (
		"movl $0, %%ebp\n"
		"movl %0, %%esp\n"
		"pushl $0\n"
		"pushl $0\n"
		// Uncomment the following line after completing exercise 13
		"sti\n"
		"1:\n"
		"hlt\n"
		"jmp 1b\n"
	: : "a" (thiscpu->cpu_ts.ts_esp0));
}
