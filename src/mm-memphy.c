
//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory physical module mm/mm-memphy.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/*
 *  MEMPHY_mv_csr - move MEMPHY cursor
 *  @mp: memphy struct
 *  @offset: offset
 */
int MEMPHY_mv_csr(struct memphy_struct *mp, int offset)
{
   int numstep = 0;

#ifdef SYNC
   pthread_mutex_lock(&mp->mutex);
#endif
   mp->cursor = 0;
   while(numstep < offset && numstep < mp->maxsz)
   {
     /* Traverse sequentially */
     mp->cursor = (mp->cursor + 1) % mp->maxsz;
     numstep++;
   }
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   return 0;
}

/*
 *  MEMPHY_seq_read - read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_seq_read(struct memphy_struct *mp, int addr, BYTE *value)
{
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);

#endif
   if (mp == NULL)
   {
#ifdef SYNC
       pthread_mutex_unlock(&mp->mutex);
#endif
       return -1;
   }
   if (!mp->rdmflg)
   {
#ifdef SYNC
       pthread_mutex_unlock(&mp->mutex);
#endif
       return -1; /* Not compatible mode for sequential read */
   }
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   MEMPHY_mv_csr(mp, addr);
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
   *value = (BYTE) mp->storage[addr];
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   return 0;
}

/*
 *  MEMPHY_read read MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @value: obtained value
 */
int MEMPHY_read(struct memphy_struct * mp, int addr, BYTE *value)
{

#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);

#endif
   if (mp == NULL)
   {
#ifdef SYNC
       pthread_mutex_unlock(&mp->mutex);
#endif
       return -1;
   }
   if (mp->rdmflg)
   {
      *value = mp->storage[addr];
   }
   else /* Sequential access device */
   {
#ifdef SYNC
       pthread_mutex_unlock(&mp->mutex);
#endif
       return MEMPHY_seq_read(mp, addr, value);
   }
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   return 0;
}

/*
 *  MEMPHY_seq_write - write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_seq_write(struct memphy_struct * mp, int addr, BYTE value)
{
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
   if (mp == NULL)
   {
#ifdef SYNC
       pthread_mutex_unlock(&mp->mutex);
#endif
       return -1;
   }
   if (!mp->rdmflg)
   {
#ifdef SYNC
       pthread_mutex_unlock(&mp->mutex);
#endif
       return -1; /* Not compatible mode for sequential read */
   }
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   MEMPHY_mv_csr(mp, addr);
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
   mp->storage[addr] = value;
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   return 0;
}

/*
 *  MEMPHY_write-write MEMPHY device
 *  @mp: memphy struct
 *  @addr: address
 *  @data: written data
 */
int MEMPHY_write(struct memphy_struct * mp, int addr, BYTE data)
{

#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
   if (mp == NULL)
   {
#ifdef SYNC
       pthread_mutex_unlock(&mp->mutex);
#endif
       return -1;
   }
   if (mp->rdmflg)
   {
       mp->storage[addr] = data;
   }
   else /* Sequential access device */
   {
#ifdef SYNC
       pthread_mutex_unlock(&mp->mutex);
#endif
       return MEMPHY_seq_write(mp, addr, data);
   }
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   return 0;
}

/*
 *  MEMPHY_format-format MEMPHY device
 *  @mp: memphy struct
 */
int MEMPHY_format(struct memphy_struct *mp, int pagesz)
{
    /* This setting come with fixed constant PAGESZ */
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
    int numfp = mp->maxsz / pagesz;
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
    struct framephy_struct *newfst, *fst;
    int iter = 0;
    if (numfp <= 0)
      return -1;

    /* Init head of free framephy list */ 
    fst = malloc(sizeof(struct framephy_struct));
    fst->fpn = iter;
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
    mp->free_fp_list = fst;
    mp->used_fp_list=malloc(sizeof(struct framephy_struct));
    mp->used_fp_list->fpn=-1;
    mp->used_fp_list->fp_next=NULL;
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif

    /* We have list with first element, fill in the rest num-1 element member*/
    for (iter = 1; iter < numfp ; iter++)
    {
#ifdef SYNC
        pthread_mutex_lock(&mp->mutex);
#endif
       newfst =  malloc(sizeof(struct framephy_struct));
       newfst->fpn = iter;
       newfst->fp_next = NULL;
       fst->fp_next = newfst;
       fst = newfst;
#ifdef SYNC
        pthread_mutex_unlock(&mp->mutex);
#endif
    }
    return 0;
}

int MEMPHY_get_freefp(struct memphy_struct *mp, int *retfpn)
{
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
   struct framephy_struct *fp = mp->free_fp_list;

   if (fp == NULL)
   {
#ifdef SYNC
       pthread_mutex_unlock(&mp->mutex);
#endif
       return -1;
   }
   *retfpn = fp->fpn;
   mp->free_fp_list = fp->fp_next;

   /* MEMPHY is iteratively used up until its exhausted
    * No garbage collector acting then it not been released
    * return retfpn
    */
   free(fp);
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   return 0;
}

int MEMPHY_dump(struct memphy_struct * mp)
{
    /*TODO dump memphy contnt mp->storage 
     *     for tracing the memory content
     */
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
    struct framephy_struct *fp = mp->used_fp_list;
    int fpn;
    printf("********************MEMPHY DUMP******************\n");
    while (fp!=NULL)
    {
        fpn=fp->fpn;
        if (fpn==-1)
        { 
           fp=fp->fp_next; 
           continue;
        }
        printf("frame: %d----------------------\n",fpn);
        int iter=0;
        for (iter;iter<PAGING_PAGESZ;iter++)
        {
           // BYTE *data;
            int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) +iter;
            if (mp->storage[phyaddr]==0)
                continue;
            printf("address 0x%08x: %d\n", phyaddr, mp->storage[phyaddr]);
        }
        fp=fp->fp_next;
    }
    printf("***************************************************\n\n");
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
    return 0;
}

int MEMPHY_put_freefp(struct memphy_struct *mp, int fpn)
{
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
   struct framephy_struct *fp = mp->free_fp_list;
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));
   /* Create new node with value fpn */
   newnode->fpn = fpn;
   newnode->fp_next = fp;
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
   mp->free_fp_list = newnode;
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   return 0;
}
int MEMPHY_put_usedfp(struct memphy_struct *mp, int fpn)
{
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
    struct framephy_struct *fp = mp->used_fp_list;
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
    struct framephy_struct *newnode = malloc(sizeof(struct framephy_struct));

    /* Create new node with value fpn */
    newnode->fpn = fpn;
    newnode->fp_next = fp;
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
    mp->used_fp_list = newnode;
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
    return 0;
}

int MEMPHY_del_usedfp(struct memphy_struct *mp, int fpn)
{
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
    struct framephy_struct *fp = mp->used_fp_list;
    while (fp!=NULL)
    {
        if (fp->fpn==fpn)
        {
            struct framephy_struct *next_fp=fp->fp_next;
            if (next_fp!=NULL)
            {
                fp->fpn=next_fp->fpn;
                fp->fp_next=next_fp->fp_next;
                free(next_fp);
            }
            else
            {
                fp->fpn=-1;
                fp->fp_next=NULL;
            }
            break;
        }
        fp=fp->fp_next;
    }
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
    return 0;
}


/*
 *  Init MEMPHY struct
 */
int init_memphy(struct memphy_struct *mp, int max_size, int randomflg)
{
#ifdef SYNC
    pthread_mutex_init(&mp->mutex, NULL);
#endif
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
    mp->storage = (BYTE *)malloc(max_size*sizeof(BYTE));
    mp->maxsz = max_size;
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   MEMPHY_format(mp,PAGING_PAGESZ);
#ifdef SYNC
    pthread_mutex_lock(&mp->mutex);
#endif
   mp->rdmflg = (randomflg != 0)?1:0;
   if (!mp->rdmflg )   /* Not Ramdom acess device, then it serial device*/
      mp->cursor = 0;
#ifdef SYNC
    pthread_mutex_unlock(&mp->mutex);
#endif
   return 0;
}

