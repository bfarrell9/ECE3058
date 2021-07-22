/*
 * student.c
 * Multithreaded OS Simulation for CS 2200 and ECE 3058
 *
 * This file contains the CPU scheduler for the simulation.
 */

#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include "os-sim.h"

#define DEBUG 0




/** Function prototypes **/
extern void idle(unsigned int cpu_id);
extern void preempt(unsigned int cpu_id);
extern void yield(unsigned int cpu_id);
extern void terminate(unsigned int cpu_id);
extern void wake_up(pcb_t *process);



/*
 * current[] is an array of pointers to the currently running processes.
 * There is one array element corresponding to each CPU in the simulation.
 *
 * current[] should be updated by schedule() each time a process is scheduled
 * on a CPU.  Since the current[] array is accessed by multiple threads, you
 * will need to use a mutex to protect it.  current_mutex has been provided
 * for your use.
 */
static pcb_t **current;
static pthread_mutex_t current_mutex;

// simple data structure for our queue
typedef struct _Node {
    struct _Node* next;
    pcb_t* data;
} Node;

//useful variables
static Node* process_list = NULL;
static pthread_mutex_t process_list_mutex;
static pthread_cond_t process_list_contains;
static int LRTF = 0;
static int empty = 1;
static int time_slice = -1;
static unsigned int cpu_count;

//allocate space for new node
static Node* createNode() {
    return (Node*) malloc(sizeof(Node));
}

//oush new node onto queue
static void push(pcb_t* data) {
    //lock process list for editing
    pthread_mutex_lock(&process_list_mutex);
    if (DEBUG) printf("PUSHING...");
    Node* node = createNode();
    node->data = data;
    node->next = NULL;
    //pointer to go to one before insertion point
    Node* ptr = process_list;
    if (ptr && LRTF) {
        //loop to node pointer one before time remaining less than data's
        while (ptr->next && ptr->next->data->time_remaining > data->time_remaining) {
            ptr = ptr->next;
        }
    } else if (ptr && !LRTF) {
        //loop to final ptr
        while (ptr->next) { 
            ptr = ptr->next;
        }
    }
    if (ptr) {
        node->next = ptr->next;
        ptr->next = node;
    } else {
        process_list = node;
    }
    empty = 0;
    if (DEBUG) printf("DONE\n");
    //unlock mutexes and send signal that process list now contains info
    pthread_cond_signal(&process_list_contains);
    pthread_mutex_unlock(&process_list_mutex);
}

//removes most desired noe (at front)
static pcb_t* pop() {
    //lock process list for operation
    pthread_mutex_lock(&process_list_mutex);
    pcb_t* result = NULL;
    //pop top value from process_list given there are values in the queue
    if (process_list) {
        result = process_list->data;
        Node* res = process_list;
        process_list = process_list->next;
        if (!process_list) empty = 1;
        //free previously allocated node
        free(res);
    }
    //unlock queue usage
    pthread_mutex_unlock(&process_list_mutex);
    return result;
}
//cd /mnt/c/Users/james/iCloudDrive/Desktop/ECE\ 3058/Labs/5/assignment/

/*
 * schedule() is your CPU scheduler.  It should perform the following tasks:
 *
 *   1. Select and remove a runnable process from your ready queue which 
 *	you will have to implement with a linked list or something of the sort.
 *
 *   2. Set the process state to RUNNING
 *
 *   3. Call context_switch(), to tell the simulator which process to execute
 *      next on the CPU.  If no process is runnable, call context_switch()
 *      with a pointer to NULL to select the idle process.
 *	The current array (see above) is how you access the currently running process indexed by the cpu id. 
 *	See above for full description.
 *	context_switch() is prototyped in os-sim.h. Look there for more information 
 *	about it and its parameters.
 */
static void schedule(unsigned int cpu_id)
{
    if (DEBUG) printf("SCHEDULE \n");
    
    //pop value
    pcb_t* process = pop();
    
    //edit state info
    if (process) process->state = PROCESS_RUNNING;

    //lock, edit, unlock current
    pthread_mutex_lock(&current_mutex);
    current[cpu_id] = process;
    pthread_mutex_unlock(&current_mutex);

    //call for context switch
    context_switch(cpu_id, process, time_slice);
}


/*
 * idle() is your idle process.  It is called by the simulator when the idle
 * process is scheduled.
 *
 * This function should block until a process is added to your ready queue.
 * It should then call schedule() to select the process to run on the CPU.
 */
extern void idle(unsigned int cpu_id)
{   
    //lock process and wait intil no longer empty
    pthread_mutex_lock(&process_list_mutex);
    if (DEBUG) printf("IDLE\n");
    if (empty) pthread_cond_wait(&process_list_contains, &process_list_mutex);
    
    //unlock and schedule new process to execute
    pthread_mutex_unlock(&process_list_mutex);
    schedule(cpu_id);

    /*
     * REMOVE THE LINE BELOW AFTER IMPLEMENTING IDLE()
     *
     * idle() must block when the ready queue is empty, or else the CPU threads
     * will spin in a loop.  Until a ready queue is implemented, we'll put the
     * thread to sleep to keep it from consuming 100% of the CPU time.  Once
     * you implement a proper idle() function using a condition variable,
     * remove the call to mt_safe_usleep() below.
     */
    //mt_safe_usleep(1000000);
    if (DEBUG) printf("END IDLE\n");
}


/*
 * preempt() is the handler called by the simulator when a process is
 * preempted due to its timeslice expiring.
 *
 * This function should place the currently running process back in the
 * ready queue, and call schedule() to select a new runnable process.
 */
extern void preempt(unsigned int cpu_id)
{
    
    //lock current, gain state, unlock current
    pthread_mutex_lock(&current_mutex);
    pcb_t* proc = current[cpu_id];
    pthread_mutex_unlock(&current_mutex);

    //set ready, push, and schedule
    proc->state = PROCESS_READY;
    push(proc);
    schedule(cpu_id);
}


/*
 * yield() is the handler called by the simulator when a process yields the
 * CPU to perform an I/O request.
 *
 * It should mark the process as WAITING, then call schedule() to select
 * a new process for the CPU.
 */
extern void yield(unsigned int cpu_id)
{
    if (DEBUG) printf("YIELD %d\n", cpu_id);

    //lock current, edit state, unlock current
    pthread_mutex_lock(&current_mutex);
    current[cpu_id]->state = PROCESS_WAITING;
    pthread_mutex_unlock(&current_mutex);

    //schedule the next process at cpu_id
    schedule(cpu_id);
    if (DEBUG)  printf("END YIELD\n");
}


/*
 * terminate() is the handler called by the simulator when a process completes.
 * It should mark the process as terminated, then call schedule() to select
 * a new process for the CPU.
 */
extern void terminate(unsigned int cpu_id)
{
    if (DEBUG) printf("TERMINATE\n");

    //lock current, edit state, unlock current
    pthread_mutex_lock(&current_mutex);
    current[cpu_id]->state = PROCESS_TERMINATED;
    pthread_mutex_unlock(&current_mutex);

    //schedule the next process at cpu_id
    schedule(cpu_id);
}


/*
 * wake_up() is the handler called by the simulator when a process's I/O
 * request completes.  It should perform the following tasks:
 *
 *   1. Mark the process as READY, and insert it into the ready queue.
 *
 *   2. If the scheduling algorithm is LRTF, wake_up() may need
 *      to preempt the CPU with lower remaining time left to allow it to
 *      execute the process which just woke up with higher reimaing time.
 * 	However, if any CPU is currently running idle,
 * 	or all of the CPUs are running processes
 *      with a higher remaining time left than the one which just woke up, wake_up()
 *      should not preempt any CPUs.
 *	To preempt a process, use force_preempt(). Look in os-sim.h for 
 * 	its prototype and the parameters it takes in.
 */
extern void wake_up(pcb_t *process)
{
    if (DEBUG) printf("WAKE UP ");
    //update process state and push onto queue
    process->state = PROCESS_READY;
    push(process);

    //in case of LRTF implementation
    if (LRTF) {
        pthread_mutex_lock(&current_mutex);
        //info variables
        unsigned int id_idx = 0, cause_idle = 0, cause_preempt = 0;
        if (DEBUG) printf("ENTER WHILE...");
        //loop through current until nonexistent element of current found or list exhausted
        for (unsigned int i = 0; i < cpu_count && !cause_idle; i++) {
            if (DEBUG) printf("LOOP ");
            //check for Null element
            if (!current[i]) {
                cause_idle = 1;
            } 
            //if not, calculate if preempt needs to be called and update index of lowest time remaining if appropriate
            else {
                if (current[i]->time_remaining < process->time_remaining)  cause_preempt = 1;
                if (current[i]->time_remaining < current[id_idx]->time_remaining) id_idx = i;
            }
        }
        if (DEBUG) printf("EXIT WHILE\n");
        //if spot open and preempt desired, call force preempt
        if (!cause_idle && cause_preempt) {
            if (DEBUG) printf("FORCE PREEMPT\n");
            pthread_mutex_unlock(&current_mutex);
            force_preempt(id_idx);
            pthread_mutex_lock(&current_mutex);
            if (DEBUG) printf("FORCE PREEMPT FINISH\n");
        }
        //unlock
        pthread_mutex_unlock(&current_mutex);
    }
    if (DEBUG) printf("WAKE UP DONE\n");
}

int strcmp(const char*, const  char*);

/*
 * main() simply parses command line arguments, then calls start_simulator().
 * You will need to modify it to support the -l and -r command-line parameters.
 */
int main(int argc, char *argv[])
{


    /* FIX ME - Add support for -l and -r parameters*/
    /* Parse command-line arguments */
    if (argc < 2) {
        fprintf(stderr, "Multithreaded OS Simulator\n"
            "Usage: ./os-sim <# CPUs> [ -l | -r <time slice> ]\n"
            "    Default : FIFO Scheduler\n"
            "         -l : Longest Remaining Time First Scheduler\n"
            "         -r : Round-Robin Scheduler\n\n");
        return -1;
    }
    //iterate through any added args
    for (int i = 2; i < argc; i++) {
        //Part 2: check for -r arg, alter time slice variable appropriately.
        if (!(strcmp("-r", argv[i])) && i < argc - 1) {
            time_slice = atoi(argv[i + 1]);
        }
        //Part 3: check for -l arg, activate LRTF if so. 
        else if (argc == 3 && !strcmp("-l", argv[i])) {
            LRTF = 1;
        }
    }
    cpu_count = strtoul(argv[1], NULL, 0);

    /* Allocate the current[] array and its mutex */
    current = malloc(sizeof(pcb_t*) * cpu_count);
    for (unsigned int i = 0; i < cpu_count; i++) current[i] = NULL;
    assert(current != NULL);
    pthread_mutex_init(&current_mutex, NULL);
    pthread_mutex_init(&process_list_mutex, NULL);
    pthread_cond_init(&process_list_contains, NULL);

    /* Start the simulator in the library */
    start_simulator(cpu_count);

    //pthread_mutex_lock(&process_list);
    pthread_mutex_destroy(&current_mutex);
    pthread_mutex_destroy(&process_list_mutex);
    pthread_cond_destroy(&process_list_contains);
    

    return 0;
}


