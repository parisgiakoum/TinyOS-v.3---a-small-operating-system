
#include "tinyos.h"
#include "kernel_sched.h"
#include "kernel_proc.h"
#include "assert.h"
#include "kernel_cc.h"

void start_new_thread()
{
  int exitval;

  Task call =  CURTHREAD->ptcb->task;
  int argl = CURTHREAD->ptcb->argl;
  void* args = CURTHREAD->ptcb->args;

  exitval = call(argl,args);
  ThreadExit(exitval);
}

PTCB* lookup_ptcb(Tid_t tid)
{
	rlnode* node=&CURPROC->thread_list;

	for(int i=0; i<rlist_len(node);i++){
		node=node->next;
		if(tid==(Tid_t) node->ptcb->thread){
			break;
		}
	}
	PTCB *ptcb=node->ptcb;
	return ptcb;
}

/** 
  @brief Create a new thread in the current process.
  */
Tid_t CreateThread(Task task, int argl, void* args)
{
	TCB *curthread=CURTHREAD;
	PTCB *newptcb;

	  Mutex_Lock(&kernel_mutex);
	  curthread->owner_pcb->thr_counter++;

	  /* The new process PTCB */
	  newptcb =(PTCB*) malloc(sizeof(PTCB));
	  if(newptcb == NULL){
		  fprintf(stderr, "Couldn't allocate memory for %p thread in CreateThread\n", CURTHREAD);
		  assert(0);
	  }

	  newptcb->detached=false;
	  newptcb->exited=false;


	  /* Set the current thread's function and arguments */
	  newptcb->task = task;
	  newptcb->argl = argl;
	  newptcb->args = args;

	  rlnode *node=rlnode_init(&newptcb->node, newptcb);
	  rlist_push_front(&CURPROC->thread_list,node);

	    /*
		Create and wake up the thread for the main function. This must be the last thing
		we do, because once we wakeup the new thread it may run! so we need to have finished
		the initialization of the PCB.
	   */
	  if(task != NULL) {
		  newptcb->thread = spawn_thread(CURPROC, start_new_thread);
		  newptcb->thread->ptcb=newptcb;
		  wakeup(newptcb->thread);
	  }
	  Mutex_Unlock(&kernel_mutex);
	  return (Tid_t) newptcb->thread;
}

/**
  @brief Return the Tid of the current thread.
 */
Tid_t ThreadSelf()
{
	return (Tid_t) CURTHREAD;
}

/**
  @brief Join the given thread.
  */
int ThreadJoin(Tid_t tid, int* exitval)
{
	Mutex_Lock(&kernel_mutex);
	PTCB *ptcb=lookup_ptcb(tid);
	//if(ptcb==NULL ||ptcb->exited==true || ptcb->detached==true || ptcb->thread==CURTHREAD){
		//Mutex_Unlock(&kernel_mutex);
		//return -1;
	//}
	while(ptcb->exited==false){
		Cond_Wait(&kernel_mutex,&CURPROC->cv);
		printf("%d	---		%d",((Tid_t)ptcb->thread),tid);
	}
	//if(ptcb->exitval!=NULL)
		//*exitval=ptcb->exitval;
	Mutex_Unlock(&kernel_mutex);
	return 0;
}

/**
  @brief Detach the given thread.
  */
int ThreadDetach(Tid_t tid)
{
	Mutex_Lock(&kernel_mutex);
	PTCB *ptcb=lookup_ptcb(tid);
	if(ptcb==NULL || ptcb->exited==true){
		Mutex_Unlock(&kernel_mutex);
		return -1;
	}

	ptcb->detached=true;
	Cond_Broadcast(&CURPROC->cv);
	Mutex_Unlock(&kernel_mutex);
	return 0;
}

/**
  @brief Terminate the current thread.
  */
void ThreadExit(int exitval)
{
	Mutex_Lock(&kernel_mutex);
	CURPROC->thr_counter--;
	CURTHREAD->ptcb->exited=true;
	CURTHREAD->ptcb->exitval=exitval;
	Cond_Broadcast(&CURPROC->cv);
	sleep_releasing(EXITED, &kernel_mutex);		/**< kernel_mutex is released in sleep_releasing().*/
}

/**
  @brief Awaken the thread, if it is sleeping.

  This call will set the interrupt flag of the
  thread.

  */
int ThreadInterrupt(Tid_t tid)
{
	return -1;
}


/**
  @brief Return the interrupt flag of the 
  current thread.
  */
int ThreadIsInterrupted()
{
	return 0;
}

/**
  @brief Clear the interrupt flag of the
  current thread.
  */
void ThreadClearInterrupt()
{

}
