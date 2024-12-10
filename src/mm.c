//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/* 
 * init_pte - Initialize PTE entry
 */
int init_pte(uint32_t *pte,
             int pre,    // present
             int fpn,    // FPN
             int drt,    // dirty
             int swp,    // swap
             int swptyp, // swap type
             int swpoff) //swap offset
{
  if (pre != 0) {
      // Non swap ~ page online
      if (fpn < 0) 
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 
  } 
  else 
  { // page swapped
      CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT); 
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
   }
  return 0;   
}

/* 
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 */
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
  CLRBIT(*pte, PAGING_PTE_PRESENT_MASK);
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/* 
 * pte_set_swap - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 */
int pte_set_fpn(uint32_t *pte, int fpn)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 

  return 0;
}


/* 
 * vmap_page_range - map a range of page at aligned address, update pte
 */
int vmap_page_range(struct pcb_t *caller, // process call
                                int addr, // start address which is aligned to pagesz
                               int pgnum, // num of mapping page
           struct framephy_struct *frames,// list of the mapped frames
              struct vm_rg_struct *ret_rg)// return mapped region, the real mapped fp
{                                         // no guarantee all given pages are mapped

  struct framephy_struct *fpit = frames;
  int pgn = PAGING_PGN(addr);

 
  ret_rg->rg_end = ret_rg->rg_start = addr; 

  /* TODO: update the rg_end and rg_start of ret_rg*/
  if (ret_rg->vmaid==0)
  {
       int pgit;
      
       if (caller->mm->pgd[0]==0)
       {
           pgit = 0;
       }
       else
       {
           pgit=1;
       }
       for (pgit;pgit<=pgnum;pgit++)
       {
           if (fpit==NULL)
           {
               return -1;
           }
           init_pte(&(caller->mm->pgd[pgn + pgit]), 1,fpit->fpn,0,0,0,0);
           MEMPHY_put_usedfp(caller->mram,fpit->fpn);
           fpit->fpn=-1;
           enlist_pgn_node(&caller->mm->fifo_pgn, pgn+pgit);
           fpit=fpit->fp_next;
       }
       ret_rg->rg_end = ret_rg->rg_start+pgnum*PAGING_PAGESZ;
  }
  else
  {
#ifdef MM_PAGING_HEAP_GODOWN
      int pgit;
      int maxPage=(caller->vmemsz - 1)/PAGING_PAGESZ;
#endif
      if (caller->mm->pgd[maxPage]==0)
      {
          pgit = 0;
      }
      else
      {
          pgit=1;
      }


      for (pgit;pgit<=pgnum;pgit++)
      {
          if (fpit==NULL)
          {
              return -1;
              //not enough frame is provided
          }
          init_pte(&(caller->mm->pgd[pgn - pgit]), 1,fpit->fpn,0,0,0,0);
	  MEMPHY_put_usedfp(caller->mram,fpit->fpn);
          fpit->fpn=-1;
          enlist_pgn_node(&caller->mm->fifo_pgn, pgn-pgit);
          fpit=fpit->fp_next;
      }
      ret_rg->rg_end =  ret_rg->rg_start-pgnum*PAGING_PAGESZ;
  }


  /* TODO map range of frame to address space 
   *      in page table pgd in caller->mm
   */
  return 0;
}

/* 
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */

int alloc_pages_range(struct pcb_t *caller, int req_pgnum, struct framephy_struct** frm_lst)
{
    int pgit, fpn;
    struct framephy_struct *newfp_str;


  /* TODO: allocate the page */
  for(pgit = 0; pgit < req_pgnum; pgit++)
  {
      if(MEMPHY_get_freefp(caller->mram, &fpn) == 0)
      {
          newfp_str = (struct framephy_struct*)malloc(sizeof(struct framephy_struct));
          newfp_str->fpn = fpn;
          newfp_str->owner = caller->mm;
          if (*frm_lst == NULL)
          {
              *frm_lst = newfp_str;
              newfp_str->fp_next = NULL;
          }
          else
          {
              newfp_str->fp_next = *frm_lst;
              *frm_lst = newfp_str;
          }
      }
      else
      {
          int vicpgn, swpfpn;
          int vicfpn;
          uint32_t vicpte;

          if (find_victim_page(caller->mm, &vicpgn)<0)
          {
              printf("find_victim\n");
              return -1;
          }
          vicpte = caller->mm->pgd[vicpgn];
          vicfpn = PAGING_PTE_FPN(vicpte);
        /* Get free frame in MEMSWP */
#ifdef SYNC
          pthread_mutex_lock(&MEM_in_use);
#endif
          int index_of_active_mswp;
          for (index_of_active_mswp = 0; index_of_active_mswp < PAGING_MAX_MMSWP;
          index_of_active_mswp++)
          {
              if ((struct memphy_struct *)&caller->mswp[index_of_active_mswp] == caller->active_mswp)
                  break;
          }
        /* Get free frame in MEMSWP */
        if (MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0)
        {
            int i;
            for (i = 0; i < PAGING_MAX_MMSWP; i++)
            {
                if (MEMPHY_get_freefp(caller->mswp[i], &swpfpn) == 0)
                {
                    caller->active_mswp = (struct memphy_struct *)&caller->mswp[i];
                    index_of_active_mswp = i;
                    break;
                }
            }
#ifdef SYNC
            pthread_mutex_unlock(&MEM_in_use);
#endif
            if (i == PAGING_MAX_MMSWP)
                return -3000; // MEMSWAP doesn't have enough space
       }

#ifdef SYNC
            pthread_mutex_unlock(&MEM_in_use);
#endif

        /* Copy victim frame to swap */
        __swap_cp_page(caller->mram,vicfpn,
        caller->active_mswp, swpfpn);
        MEMPHY_del_usedfp(caller->mram,vicfpn);

        pte_set_swap(&caller->mm->pgd[vicpgn], index_of_active_mswp, swpfpn);

        newfp_str = (struct framephy_struct*)malloc(sizeof(struct framephy_struct));
        newfp_str->fpn = vicfpn;
        newfp_str->owner = caller->mm;
        if (*frm_lst == NULL)
        {
            *frm_lst = newfp_str;
            newfp_str->fp_next = NULL;
        }
        else
        {
            newfp_str->fp_next = *frm_lst;
            *frm_lst = newfp_str;
        }
      }
  }
  return 0;
}


/* 
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
int vm_map_ram(struct pcb_t *caller, int astart, int aend, int mapstart, int incpgnum, struct vm_rg_struct *ret_rg)
{

  struct framephy_struct *frm_lst = NULL;
  int ret_alloc;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide 
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst);

  if (ret_alloc < 0 && ret_alloc != -3000)
  {
      struct framephy_struct *frmit ;
      while (frm_lst!=NULL)
      {
          MEMPHY_put_freefp(caller->mram,frm_lst->fpn);
          frmit=frm_lst;
          frm_lst=frm_lst->fp_next;
          free(frmit);
      }
      return -1;
  }
  /* Out of memory */
  if (ret_alloc == -3000) 
  {
#ifdef MMDBG
     printf("OOM: vm_map_ram out of memory \n");
#endif
      struct framephy_struct *frmit ;
      while (frm_lst!=NULL)
      {
          MEMPHY_put_freefp(caller->mram,frm_lst->fpn);
          frmit=frm_lst;
          frm_lst=frm_lst->fp_next;
          free(frmit);
      }
      return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);

  struct framephy_struct *frm_it = frm_lst;
  while (frm_lst!=NULL)
  {   
      if (frm_lst->fpn!=-1)
      {
         MEMPHY_put_freefp(caller->mram,frm_lst->fpn);
      } 
      frm_it=frm_lst;
      frm_lst=frm_lst->fp_next;
      free(frm_it);
  }
  return 0;
}

/* Swap copy content page from source frame to destination frame 
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                struct memphy_struct *mpdst, int dstfpn) 
{
  int cellidx;
  int addrsrc,addrdst;

  for(cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct * vma0 = malloc(sizeof(struct vm_area_struct));
  struct vm_area_struct * vma1 = malloc(sizeof(struct vm_area_struct));

  mm->pgd = malloc(PAGING_MAX_PGN*sizeof(uint32_t));
  for (int i=0;i<PAGING_MAX_PGN;i++){
     mm->pgd[i]=0;
  }
  vma0->vm_id = 0;
  vma0->vm_start = 0;
  vma0->vm_end = vma0->vm_start;
  vma0->sbrk = vma0->vm_start;
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end, 0);
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /* TODO update VMA0 next */
  vma0->vm_next= vma1;
  vma1->vm_id = 1;
#ifdef MM_PAGING_HEAP_GODOWN
  vma1->vm_start = caller->vmemsz-1;
  vma1->vm_end = vma1->vm_start;
  vma1->sbrk = vma1->vm_start;
#endif
    struct vm_rg_struct *first_rg1 = init_vm_rg(vma1->vm_start, vma1->vm_end, 1);
    enlist_vm_rg_node(&vma1->vm_freerg_list,first_rg1);
    vma1->vm_next=NULL;
  /* TODO: update one vma for HEAP */


  /* Point vma owner backward */
  vma0->vm_mm = mm; 
  vma1->vm_mm = mm;

  /* TODO: update mmap */
  mm->mmap = vma0;

  return 0;
}

struct vm_rg_struct* init_vm_rg(int rg_start, int rg_end, int vmaid)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->vmaid = vmaid;
  rgnode->rg_next = NULL;

  return rgnode;
}

int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct* rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}

int enlist_pgn_node(struct pgn_t **plist, int pgn)
{
  struct pgn_t* pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}

int print_list_fp(struct framephy_struct *ifp)
{
   struct framephy_struct *fp = ifp;
 
   printf("print_list_fp: ");
   if (fp == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (fp != NULL )
   {
       printf("fp[%d]\n",fp->fpn);
       fp = fp->fp_next;
   }
   printf("\n");
   return 0;
}

int print_list_rg(struct vm_rg_struct *irg)
{
   struct vm_rg_struct *rg = irg;
 
   printf("print_list_rg: ");
   if (rg == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (rg != NULL)
   {
       printf("rg[%ld->%ld<at>vma=%d]\n",rg->rg_start, rg->rg_end, rg->vmaid);
       rg = rg->rg_next;
   }
   printf("\n");
   return 0;
}

int print_list_vma(struct vm_area_struct *ivma)
{
   struct vm_area_struct *vma = ivma;
 
   printf("print_list_vma: ");
   if (vma == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (vma != NULL )
   {
       printf("va[%ld->%ld]\n",vma->vm_start, vma->vm_end);
       vma = vma->vm_next;
   }
   printf("\n");
   return 0;
}

int print_list_pgn(struct pgn_t *ip)
{
   printf("print_list_pgn: ");
   if (ip == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (ip != NULL )
   {
       printf("va[%d]-\n",ip->pgn);
       ip = ip->pg_next;
   }
   printf("n");
   return 0;
}

int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  
  if (caller!=NULL)
     printf("PAGE TABLE FOR PROCESS %d************************\n",caller->pid);
  int pgn_start,pgn_end;
  int pgit;

  if(end == -1)
  {
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0);
    end = cur_vma->vm_end;
  }
  pgn_start = PAGING_PGN(start);
  pgn_end = PAGING_PGN(end);

  printf("print_pgtbl for vmaid 0: %d - %d", start, end);
  if (caller == NULL)
  {
      printf("NULL caller\n");
      return -1;
  }
  printf("\n");
  if (end!=0)
  {
      for(pgit = pgn_start; pgit <= pgn_end; pgit++)
      {
          printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
      }
  }
#ifdef MM_PAGING_HEAP_GODOWN
  uint32_t heap_end;
  pgn_start = (caller->vmemsz -1) / PAGING_PAGESZ ;
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 1);
  heap_end = cur_vma->vm_end;

  pgn_end = PAGING_PGN(heap_end) ;

  printf( "print_pgtbl for vmaid 1: %d - %d", caller->vmemsz - 1, heap_end);
  if (caller == NULL)
  {
      printf("NULL caller\n");
      return -1;
  }
  printf("\n");
  if (heap_end!=caller->vmemsz-1)
  {
      for(pgit = pgn_start; pgit >=pgn_end; pgit--)
      {
          printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
      }
  }
#endif
  printf("********************************************\n\n");
  return 0;
}

