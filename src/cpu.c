

#include "cpu.h"
#include "mem.h"
#include "mm.h"

int  statistic(struct pcb_t* proc, int rgid){
  printf("\nSTATISTIC ----------------------------\n");
  if (proc ==NULL)
  {
  
     printf("NULL CALLER\n");
    return -1;
  }
  struct mm_struct* mm=proc->mm;
  if (rgid < 0 || rgid >= PAGING_MAX_SYMTBL_SZ )
  {
        printf("INVALID RGID\n");
        return -1;
  }
  struct vm_rg_struct region= mm->symrgtbl[rgid];
  int count;
  int rgstart=region.rg_start;
 if (region.vmaid==0) rgstart+=1;
 else rgstart-=1;
  printf("RGID %d: rg_start %d rg_end %d vm_id %d\n", rgid,rgstart,
         region.rg_end,region.vmaid); 
  struct vm_area_struct* cur_vma=mm->mmap;	
  while (cur_vma!=NULL)
  {
        printf("VM_AREA %d vm_start %d vm_end %d\n", cur_vma->vm_id, cur_vma->vm_start,
              cur_vma->vm_end);
         struct vm_rg_struct * free_list=cur_vma->vm_freerg_list;
          
            count=0;
           while (free_list!=NULL)
            {
                     if (free_list->rg_start!=free_list->rg_end)
                     {
                        count++;
                      }
                     free_list=free_list->rg_next;
              }
                printf("COUNT FREE LIST : %d\n", count);
              cur_vma=cur_vma->vm_next;
   }
   printf("-------------------------------\n\n");
   return 0;
 }



int calc(struct pcb_t * proc) {
	return ((unsigned long)proc & 0UL);
}

int alloc(struct pcb_t * proc, uint32_t size, uint32_t reg_index) {
	addr_t addr = alloc_mem(size, proc);
	if (addr == 0) {
		return 1;
	}else{
		proc->regs[reg_index] = addr;
		return 0;
	}
}

int free_data(struct pcb_t * proc, uint32_t reg_index) {
	return free_mem(proc->regs[reg_index], proc);
}

int read(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		uint32_t offset, // Source address = [source] + [offset]
		uint32_t destination) { // Index of destination register
	
	BYTE data;
	if (read_mem(proc->regs[source] + offset, proc,	&data)) {
		proc->regs[destination] = data;
		return 0;		
	}else{
		return 1;
	}
}

int write(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		uint32_t offset) { 	// Destination address =
					// [destination] + [offset]
	return write_mem(proc->regs[destination] + offset, proc, data);
} 

int run(struct pcb_t * proc) {
	/* Check if Program Counter point to the proper instruction */
	if (proc->pc >= proc->code->size) {
		return 1;
	}
	
	struct inst_t ins = proc->code->text[proc->pc];
	proc->pc++;
	int stat = 1;
#ifdef SCHED_INFO
        printf("\tCurrent time %d proc %d\n",
               (int)current_time(),proc->pid);
#endif
	switch (ins.opcode) {
	case CALC:
		stat = calc(proc);
		break;
	case ALLOC:
#ifdef MM_PAGING
		stat = pgalloc(proc, ins.arg_0, ins.arg_1);
                //printf("TEST %d\n",ins.arg_1);
                 
               // statistic(proc->mm,ins.arg_1);
                       
#ifdef MEM_INFO
                print_pgtbl(proc,0,-1);
#endif
#else
		stat = alloc(proc, ins.arg_0, ins.arg_1);
#endif
		break;
#ifdef MM_PAGING
	case MALLOC:
		stat = pgmalloc(proc, ins.arg_0, ins.arg_1);
               //printf("TEST %d",ins.arg_1); 
               //statistic(proc->mm,ins.arg_1);
#ifdef MEM_INFO
		print_pgtbl(proc,0,-1);
#endif
                break;
#endif
	case FREE:
#ifdef MM_PAGING
		stat = pgfree_data(proc, ins.arg_0);
                //statistic(proc->mm,ins.arg_0);
#ifdef MEM_INFO
                print_pgtbl(proc,0,-1);
#endif
#else
		stat = free_data(proc, ins.arg_0);
#endif
		break;
	case ADDRESS:
#ifdef MM_PAGING
                stat = statistic(proc, ins.arg_0);
      
#ifdef MEM_INFO
                print_pgtbl(proc,0,-1);
#endif
#else
                stat = free_data(proc, ins.arg_0);
#endif
                break;

         case READ:
#ifdef MM_PAGING
		stat = pgread(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#ifdef MEM_INFO
                print_pgtbl(proc,0,-1);
#endif
#else
		stat = read(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
		break;
	case WRITE:
#ifdef MM_PAGING
		stat = pgwrite(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#ifdef MEM_INFO
                print_pgtbl(proc,0,-1);
#endif
#else 
		stat = write(proc, ins.arg_0, ins.arg_1, ins.arg_2);
#endif
		break;
	default:
		stat = 1;
	}
	return stat;

}


