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

typedef enum FIT_ALGOR {
    FIRST_FIT, 
    NEXT_FIT, 
    BEST_FIT
} FIT_ALGOR;

void *heap_listp;
void *next_listp;

/*
 * Coalesce free list
*/
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));     /* previous block alloc bit */
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));     /* next block alloc bit */
    size_t size = GET_SIZE(HDRP(bp));                       /* the size of current block */


    /* Case 1 */
    if(prev_alloc && next_alloc){
        return bp;                               /* do nothing */
    }
    /* Case 2 */
    else if(prev_alloc && !next_alloc){
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));   /* add the size of next block */
        PUT(HDRP(bp), PACK(size, 0));            /* modify header */
        PUT(FTRP(bp), PACK(size, 0));            /* modify footer */ 
    }
    /* Case 3 */
    else if(!prev_alloc && next_alloc){
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));   /* add the size of previous block */
        PUT(FTRP(bp), PACK(size, 0));            /* modify footer */ 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); /* modify header */ 
        bp = PREV_BLKP(bp);                      /* change block pointer */ 
    }
    /* Case 4 */
    else{
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));  /* add the size of next and previous block */
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0)); /* modify footer */ 
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0)); /* modify header */ 
        bp = PREV_BLKP(bp);                      /* change block pointer */ 
    }
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

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL)
        return -1;
    
    return 0;
}

static void *first_fit(size_t asize)
{
    void *bp = heap_listp; /* pointer to Prologue footer */
    size_t size;

    for (; (size = GET_SIZE(HDRP(bp))) > 0; bp = NEXT_BLKP(bp)) {
        if ((GET_ALLOC(HDRP(bp)) == 0) && size > asize)
            return bp;
    }
    return NULL; /* Can't find block */
}

/* Get some error */
static void *next_fit(size_t asize)
{
    size_t size;
    void *bp = next_listp;
    void *tmp_listp;

    for (; (size = GET_SIZE(HDRP(bp))) > 0; bp = NEXT_BLKP(bp)) {
        if ((GET_ALLOC(HDRP(bp)) == 0) && size > asize) {
            tmp_listp = NEXT_BLKP(bp);
            if ((GET_SIZE(HDRP(tmp_listp)) == 0) && (GET_ALLOC(HDRP(tmp_listp)) == 1)) {
                next_listp = heap_listp;
                return PREV_BLKP(tmp_listp);
            }
            else {
                next_listp = tmp_listp;
                return PREV_BLKP(tmp_listp);
            }
        }
    }
    next_listp = heap_listp;
    return NULL;  /* Can't find block */
}

static void *best_fit(size_t asize)
{
    void *bp = heap_listp; /* pointer to Prologue footer */
    void *best_bp = NULL;
    size_t min_size = 0, size;

    for (; (size = GET_SIZE(HDRP(bp))) > 0; bp = NEXT_BLKP(bp)) {
        if ((GET_ALLOC(HDRP(bp)) == 0) && size > asize) {
            if (min_size == 0 || (size < min_size)) {
                min_size = size;
                best_bp = bp;
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
        case NEXT_FIT:
            bp = next_fit(asize);
            break;
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
    size_t brk_size = GET_SIZE(HDRP(bp));
    size_t remain_size = brk_size - asize;

    /* Split allocated block and free block if there are some remain space */
    if(remain_size >= 2 * DSIZE) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(remain_size, 0));
        PUT(FTRP(bp), PACK(remain_size, 0));
    }
    /* Not split */
    else{
        PUT(HDRP(bp), PACK(brk_size, 1));
        PUT(FTRP(bp), PACK(brk_size, 1));
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