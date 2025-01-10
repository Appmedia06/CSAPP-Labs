/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 * 
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4 /* Word and header/footer size (bytes) */
#define DSIZE 8 /* Double words size (bytes) */
#define CHUNKSIZE (1 << 12) /* Extend heap size (4K bytes)*/

#define MAX(x, y) ((x) > (y)? (x) : (y))

/* Pack size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and Write a word at address p */
#define GET(p) (*(unsigned int*)(p))
#define PUT(p, val) (*(unsigned int*)(p) = (val))

/* Get the size and allocated field from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Compute address of next and previous block */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// by Chang
#define NEXT_PTR(bp) ((char *)(bp) + WSIZE)
#define PREV_PTR(bp) ((char *)(bp))
#define GET_NEXT(bp) (*(char **)(NEXT_PTR(bp)))
#define GET_PREV(bp) (*(char **)(bp))
#define PUT_PTR(p, ptr) (*(unsigned int *)(p) = (unsigned int)(ptr))

typedef enum FIT_ALGOR {
    FIRST_FIT, 
    NEXT_FIT, 
    BEST_FIT
} FIT_ALGOR;

void *heap_listp;
void *next_listp;

#define SEG_SIZE 16
static void *seg_listp_arr[SEG_SIZE];

static int get_listId(size_t size)
{   
    int i;
    /* 2^4 ~ 2^20 */
    for (i = 4; i < 19; i++) {
        if (size <= (1 << i))
            return i - 4;
    }
    return i - 4;
}

static void insert_blk(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    int list_id = get_listId(size);

    void *root_blk = seg_listp_arr[list_id];
    void *next_blk = root_blk;
    void *prev_blk = NULL;

    //printf("id=%d\n", list_id);

    /* Find smallest block */
    while (next_blk) {
         if (GET_SIZE(HDRP(bp)) < GET_SIZE(HDRP(next_blk))) 
            break;
         prev_blk = next_blk;
         next_blk = GET_NEXT(next_blk);
         //printf("A");
    }
    /* First node */
    if (next_blk == root_blk) {
        PUT_PTR(PREV_PTR(bp), NULL);
        PUT_PTR(NEXT_PTR(bp), next_blk);
        if (next_blk) {
            PUT_PTR(PREV_PTR(next_blk), bp);
        }
        seg_listp_arr[list_id] = bp;
    }
    else {
        PUT_PTR(PREV_PTR(bp), prev_blk);
        PUT_PTR(NEXT_PTR(bp), next_blk);
        PUT_PTR(NEXT_PTR(prev_blk), bp);
        if (next_blk) {
            PUT_PTR(PREV_PTR(next_blk), bp);
        }
    }
}

static void delete_blk(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    int list_id = get_listId(size);

    void *next_blk = GET_NEXT(bp);
    void *prev_blk = GET_PREV(bp);

    /* Empty free list */
    if (!prev_blk && !next_blk) {
        seg_listp_arr[list_id] = NULL;  /* do nothing */
    }
    /* First node */
    else if (!prev_blk && next_blk) {
        PUT_PTR(PREV_PTR(next_blk), NULL); /* delete link */
        seg_listp_arr[list_id] = next_blk;
    }
    /* Last node */
    else if (prev_blk && !next_blk) {
        PUT_PTR(NEXT_PTR(prev_blk), NULL); /* delete link */
    }
    else {
        PUT_PTR(PREV_PTR(next_blk), prev_blk);
        PUT_PTR(NEXT_PTR(prev_blk), next_blk);
    }
}

/*
 * Coalesce free list
*/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     /* previous block alloc bit */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     /* next block alloc bit */
    size_t size = GET_SIZE(HDRP(bp));                       /* the size of current block */

    void *prev_blk = PREV_BLKP(bp);
    void *next_blk = NEXT_BLKP(bp);

    /* Case 1 */
    if(prev_alloc && next_alloc){
        insert_blk(bp);
        return bp;                               
    }
    /* Case 2 */
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));   /* add the size of next block */
        delete_blk(next_blk);                    /* delete next block*/
    }
    /* Case 3 */
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));   /* add the size of previous block */
        delete_blk(prev_blk);                    /* delete prev block */
        bp = prev_blk;                           /* change block pointer */ 
    }
    /* Case 4 */
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));  /* add the size of next and previous block */
        delete_blk(next_blk);                     /* delete next block */
        delete_blk(prev_blk);                     /* delete prev block */
        bp = prev_blk;                            /* change block pointer */ 
    }

    /* Case 2 ~ 4 free block update header and footer */
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    insert_blk(bp);                                /* After delteting free block, insert merged free block */
    return bp;
}

static void *extend_heap(size_t words) 
{
    char *bp;
    int size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2)? (words + 1) * WSIZE: words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header*/
    PUT(HDRP(bp), PACK(size, 0));         /* Header */
    PUT(FTRP(bp), PACK(size, 0));         /* Footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    PUT_PTR(PREV_PTR(bp), NULL);          /* Set NULL pointer to prev */
    PUT_PTR(NEXT_PTR(bp), NULL);          /* Set NULL pointer to next */


    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/* 
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *) - 1)
        return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + WSIZE, PACK(DSIZE, 1));     /* Prologue header */
    PUT(heap_listp + 2 * WSIZE, PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + 3 * WSIZE, PACK(0, 1));     /* Epilogue header */
    heap_listp += (2 * WSIZE);
    next_listp = heap_listp;

    for (int i = 0; i < 16; i++) {
        seg_listp_arr[i] = NULL;
    }

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    char *bp;
    if ((bp = extend_heap(CHUNKSIZE/WSIZE)) == NULL)
        return -1;

    /* Set NULL into prev and next pointer */
    PUT_PTR(PREV_PTR(bp), NULL);
    PUT_PTR(NEXT_PTR(bp), NULL);
    
    return 0;
}

static void *first_fit(size_t asize)
{
    void *bp;
    int list_id = get_listId(asize);

    for (; list_id < SEG_SIZE; list_id++) {
        bp = seg_listp_arr[list_id];
        for (; bp != NULL; bp = GET_NEXT(bp)) {
            if (GET_SIZE(HDRP(bp)) >= asize) {
                return bp;
            }
        }
    }
    return NULL;
}

static void *best_fit(size_t asize)
{
    void *bp;
    void *best_bp = NULL;
    size_t min_size = 0, size;
    int list_id = get_listId(asize);

    for (; list_id < SEG_SIZE; list_id++) {
        bp = seg_listp_arr[list_id];
        for (; bp != NULL; bp = GET_NEXT(bp)) {
            if ((size = GET_SIZE(HDRP(bp))) >= asize) {
                if (min_size == 0 || size < min_size) {
                    min_size = size;
                    best_bp = bp;
                }
            }
        }
    }
    return best_bp;
}

void *find_fit(FIT_ALGOR algor, size_t asize)
{
    char* bp;

    switch (algor) {
        case FIRST_FIT:
            bp = first_fit(asize);
            break;
        // case NEXT_FIT:
        //     bp = next_fit(asize);
        //     break;
        case BEST_FIT:
            bp = best_fit(asize);
            break;
        default:
            printf("Error: fit algorithm error.\n");
            bp = NULL;
            break;
    }
    return bp;
}

/*
 * place the block and split free block
 */
void place(void *bp, size_t asize)
{
    size_t blk_size = GET_SIZE(HDRP(bp));
    size_t remain_size = blk_size - asize;

    delete_blk(bp);

    /* Split allocated block and free block if there are some remain space */
    if(remain_size >= 2 * DSIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(remain_size, 0));
        PUT(FTRP(bp), PACK(remain_size, 0));
        insert_blk(bp);
    }
    /* Not split */
    else{
        PUT(HDRP(bp), PACK(blk_size, 1));
        PUT(FTRP(bp), PACK(blk_size, 1));
    }
}

/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;
    size_t extendsize;
    void *bp;
    FIT_ALGOR FA = BEST_FIT;

    if(size == 0)
        return NULL;

    /* Adjust block size to include overhead and ailgnment reqs. */
    if(size <= DSIZE)
        asize = 2 * DSIZE;
    else
        asize = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if((bp = find_fit(FA, asize)) != NULL){
        place(bp, asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);
    if((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;
    place(bp, asize);
    return bp;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    size = GET_SIZE(HDRP(oldptr));
    copySize = GET_SIZE(HDRP(newptr));
    if (size < copySize)
      copySize = size;
    memcpy(newptr, oldptr, copySize-WSIZE);
    mm_free(oldptr);
    return newptr;
}