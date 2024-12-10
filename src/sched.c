
#include "queue.h"
#include "sched.h"
#include <pthread.h>

#include <stdlib.h>
#include <stdio.h>
static struct queue_t ready_queue;
static struct queue_t run_queue;
static pthread_mutex_t queue_lock;

#ifdef MLQ_SCHED
static struct queue_t mlq_ready_queue[MAX_PRIO];
static int mlq_ready_queue_slot[MAX_PRIO];
static int existQueue[MAX_PRIO];
static int waitingQueue[MAX_PRIO];
int waitingSize=0;

struct Heap {
    int* array;
    int size;
    int capacity;
} queueController;


//static int prevPrio=-1;
#endif

void createHeap(int capacity)
{
    queueController.size = 0;
    queueController.capacity = capacity;
    queueController.array = (int*)malloc(capacity * sizeof(int));
}

void swap(int* a, int* b)
{
    int temp = *a;
    *a = *b;
    *b = temp;
}


void heapify( int i)
{
    int smallest = i;
    int left = 2 * i + 1;
    int right = 2 * i + 2;

    if (left < queueController.size
        && queueController.array[left] < queueController.array[smallest])
        smallest = left;

    if (right < queueController.size
        && queueController.array[right] < queueController.array[smallest])
        smallest = right;

    if (smallest != i) 
    {
        swap(&queueController.array[i], &queueController.array[smallest]);
        heapify( smallest);
    }
}

void insertHeap( int value)
{
    if (queueController.size == queueController.capacity) 
    {
        //printf("Heap overflow\n");
        return;
    }

    queueController.size++;
    int i = queueController.size - 1;
    queueController.array[i] = value;

    // Fix the heap property if it is violated
    while (i != 0
           && queueController.array[(i - 1) / 2] > queueController.array[i]) 
    {
        swap(&queueController.array[i], &queueController.array[(i - 1) / 2]);
        i = (i - 1) / 2;
    }
}


void buildHeap()
{
                  // heap
    //printf("buildHeap\n");
    // Start from the last non-leaf node (parent of the l>
    // leaf) and heapify all levels in reverse order
    for (int i=0;i<waitingSize;i++)
    {
        insertHeap( waitingQueue[i]);
    }
    waitingSize=0;
}



int extractMax()
{   
    if (queueController.size <= 0)
        return -1;
    if (queueController.size == 1)
    {
        queueController.size--;
        return queueController.array[0];
    }

    // Store the maximum value, and remove it
    int root = queueController.array[0];
    queueController.array[0] = queueController.array[queueController.size - 1];
    queueController.size--;
    heapify( 0);

    return root;
}

void init_slot()
{
    for (int i = 0; i < MAX_PRIO; i++)
    {
        mlq_ready_queue_slot[i] = MAX_PRIO - i;
    }
}

int queue_empty(void)
{
#ifdef MLQ_SCHED
	unsigned long prio;
	for (prio = 0; prio < MAX_PRIO; prio++)
		if(!empty(&mlq_ready_queue[prio])) 
			return -1;
#endif
	return (empty(&ready_queue) && empty(&run_queue));
}

void init_scheduler(void)
{
#ifdef MLQ_SCHED
    int i ;
	for (i = 0; i < MAX_PRIO; i ++)
		mlq_ready_queue[i].size = 0;
	init_slot();
        createHeap(MAX_PRIO);
        for (i=0;i<MAX_PRIO;i++)
                existQueue[i]=0;
#endif
	ready_queue.size = 0;
	run_queue.size = 0;
	pthread_mutex_init(&queue_lock, NULL);
}

#ifdef MLQ_SCHED
/* 
 *  Stateful design for routine calling
 *  based on the priority and our MLQ policy
 *  We implement stateful here using transition technique
 *  State representation   prio = 0 .. MAX_PRIO, curr_slot = 0..(MAX_PRIO - prio)
 */
struct pcb_t * get_mlq_proc(void){
	struct pcb_t * proc = NULL;
	/*TODO: get a process from PRIORITY [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */

    pthread_mutex_lock(&queue_lock);
    while (queueController.size!=0||waitingSize!=0)
    {   
    
       if (queueController.size!=0)
       {
          int prio=queueController.array[0];
             proc=dequeue(&mlq_ready_queue[prio]);
             mlq_ready_queue_slot[prio]--;
             if (empty(&mlq_ready_queue[prio])|| mlq_ready_queue_slot[prio]<=0)
             {    
                 extractMax();
                 if (!empty(&mlq_ready_queue[prio]))
                  {
                     waitingQueue[waitingSize++]=prio;
                  }
                  else 
                  {
                      existQueue[prio]=0;
                  }
              }           
        }
        if (queueController.size==0&&waitingSize!=0&&proc==NULL)
        {
                init_slot();
                buildHeap();
        }
         if (proc !=NULL)
        {
               break;
        }    
     }
    pthread_mutex_unlock(&queue_lock);
    return proc;
} 

void put_mlq_proc(struct pcb_t * proc) 
{
	pthread_mutex_lock(&queue_lock);
        if (existQueue[proc->prio]==0&&mlq_ready_queue_slot[proc->prio]!=0)
        {
            existQueue[proc->prio]=1;
            insertHeap( proc->prio);
        }
       else if (existQueue[proc->prio]==0&&mlq_ready_queue_slot[proc->prio]==0)
        {
            waitingQueue[waitingSize++]=proc->prio;
             existQueue[proc->prio]=1;
        }
	enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_mlq_proc(struct pcb_t * proc) 
{
	pthread_mutex_lock(&queue_lock);
       	if (existQueue[proc->prio]==0&&mlq_ready_queue_slot[proc->prio]!=0)
        {
             existQueue[proc->prio]=1;
            insertHeap( proc->prio);
        }
        else if (existQueue[proc->prio]==0&&mlq_ready_queue_slot[proc->prio]==0)
        {
            waitingQueue[waitingSize++]=proc->prio;
             existQueue[proc->prio]=1;
        }
        enqueue(&mlq_ready_queue[proc->prio], proc);
	pthread_mutex_unlock(&queue_lock);	
}

struct pcb_t * get_proc(void) {
	return get_mlq_proc();
}

void put_proc(struct pcb_t * proc) {
	return put_mlq_proc(proc);
}

void add_proc(struct pcb_t * proc) {
	return add_mlq_proc(proc);
}
#else
struct pcb_t * get_proc(void) {
	struct pcb_t * proc = NULL;
	/*TODO: get a process from [ready_queue].
	 * Remember to use lock to protect the queue.
	 * */
    pthread_mutex_lock(&queue_lock);
    proc = dequeue(&ready_queue);;
	pthread_mutex_unlock(&queue_lock);
	return proc;
}

void put_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&run_queue, proc);
	pthread_mutex_unlock(&queue_lock);
}

void add_proc(struct pcb_t * proc) {
	pthread_mutex_lock(&queue_lock);
	enqueue(&ready_queue, proc);
	pthread_mutex_unlock(&queue_lock);	
}
#endif


