/*
 * mm-naive.c - The fastest, least memory-efficient malloc p.
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
 * provide your information in the following struct.
 ********************************************************/
team_t team = {
    /* Your student ID */
    "20181256",
    /* Your full name*/
    "DoHyun Kim",
    /* Your email address */
    "kdh2646@naver.com",
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/*Basic constants and macros*/
#define WSIZE 4 /* Word and header/footer size (bytes)*/
#define DSIZE 8 /* Double word size (bytes)*/
#define CHUNKSIZE (1<<12) /*Extend heap by this amount (bytes)*/

#define MAX(x,y) ((x)>(y)? (x):(y))

/*Pack a size and allocated bit into a word*/
#define PACK(size, alloc) ((size)|(alloc))

/*Read and write a word at address p*/
#define GET(p) (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/*Read the size and allocated fields from address p*/
#define GET_SIZE(p) (GET(p) & ~0x7) 
#define GET_ALLOC(p) (GET(p) & 0x1) 

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE))) 


/*my macro*/
#define PREV_BLK_SP_R(bp) (GET((char*)bp))
#define NEXT_BLK_SP_R(bp) (GET((char*)(bp)+WSIZE))
#define PREV_BLK_SP_W(bp, ptr) (PUT(bp, ptr))
#define NEXT_BLK_SP_W(bp, ptr) (PUT((char*)(bp)+WSIZE, ptr))

static char* heap_listp; 
size_t segregate_num=8;
static char** segregate_list_pointer;


int seg_index(size_t A){
    int index=0;
     if(A<=524288)
    index=7;
     if(A<=65536)
    index=6;
     if(A<=8192)
    index=5;
    if(A<=2048)
    index=4;
     if(A<=512)
    index=3;
     if(A<=128)
    index=2;
     if(A<=16)
    index=1;
    return index;
}

void seg_delete(void* bp){

    if(NEXT_BLK_SP_R(bp)){
        if(!PREV_BLK_SP_R(bp)){
           PREV_BLK_SP_W(NEXT_BLK_SP_R(bp),NULL); //이전 포인터는 없다(첫번째)
        }
        else{
             PREV_BLK_SP_W(NEXT_BLK_SP_R(bp),PREV_BLK_SP_R(bp));
        }
    }

   if(PREV_BLK_SP_R(bp)){
        if(!NEXT_BLK_SP_R(bp)){
           NEXT_BLK_SP_W(PREV_BLK_SP_R(bp),NULL); //이후 포인터는 없다(마지막)
        }
        else{
             NEXT_BLK_SP_W(PREV_BLK_SP_R(bp),NEXT_BLK_SP_R(bp));
        }
    }
    else{
        segregate_list_pointer[seg_index(GET_SIZE(HDRP(bp)))]=NEXT_BLK_SP_R(bp); //다음 free block을 가르켜야 한다
    }
    

}

void seg_insert(void* bp){

    NEXT_BLK_SP_W(bp,segregate_list_pointer[seg_index(GET_SIZE(HDRP(bp)))]);
   
    if(segregate_list_pointer[seg_index(GET_SIZE(HDRP(bp)))]!=NULL){
        PREV_BLK_SP_W(segregate_list_pointer[seg_index(GET_SIZE(HDRP(bp)))],bp);
    }
      PREV_BLK_SP_W(bp,NULL);
    segregate_list_pointer[seg_index(GET_SIZE(HDRP(bp)))]=bp;
}

static void* coalesce(void* bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {        //앞 뒤로 있을때     /* Case 1 */
    }

    else if (prev_alloc && !next_alloc) {   //뒤에가 free   /* Case 2 */
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        seg_delete(NEXT_BLKP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
     
    }
    else if (!prev_alloc && next_alloc) {    //앞에가 free   /* Case 3 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp)));
       seg_delete(PREV_BLKP(bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }
    else {                                     /* Case 4 */
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
       seg_delete(PREV_BLKP(bp));
       seg_delete(NEXT_BLKP(bp));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);
    }

    seg_insert(bp);
    return bp;
}

static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));         /* Free block header */
    PUT(FTRP(bp), PACK(size, 0));         /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); /* New epilogue header */
    return bp;
}

static void place_ext(void* bp, size_t asize)
{ //extended 해서 넣을 때
     size_t csize = GET_SIZE(HDRP(bp));
    if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        //split
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        seg_insert(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
  
    }
    
}


static void place_seg(void* bp, size_t asize)
{
     size_t csize = GET_SIZE(HDRP(bp));

    //seglist에서 가져오기
        seg_delete(bp);
         if ((csize - asize) >= (2 * DSIZE)) {
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        //split해서 seglist에 넣기
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize - asize, 0));
        PUT(FTRP(bp), PACK(csize - asize, 0));
        seg_insert(bp);
    }
    else {
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}

static void* find_fit(size_t asize)
{
    void* bp;
    void* best_fit=NULL;
    size_t size;
    size_t least_size= 999999;
    int i=seg_index(asize);
    
    while(i<segregate_num){
        bp=segregate_list_pointer[i];
        i++;
        while (1) {
            if(!bp)
            break;
            size = GET_SIZE(HDRP(bp));
            if ((asize > size)) 
            {
                bp = NEXT_BLK_SP_R(bp);
                continue;}
            else{
                if (size < least_size) {
                    least_size = size;
                    best_fit = bp;
                }
                bp = NEXT_BLK_SP_R(bp);
            }
        }
        if (best_fit!=NULL) return best_fit;
    }
    return NULL;
}

int mm_init(void)
{
    /* Create the initial empty heap */
    if ((segregate_list_pointer = mem_sbrk(segregate_num*WSIZE)) == (void *)-1)
    return -1;

    for(int i=0;i<segregate_num;i++){
    segregate_list_pointer[i]=NULL; //seglist 초기화
}
if ((heap_listp = mem_sbrk(4*WSIZE)) == (void *)-1)
    return -1;
    PUT(heap_listp, 0);                          /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1)); /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1)); /* Prologue footer */
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));     /* Epilogue header */
    heap_listp += (2 * WSIZE);

    return 0;
}

void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char* bp;

    /* Ignore spurious requests */
    if (size == 0) 
    return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) 
    asize = 2 * DSIZE;
    else asize = DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize)) != NULL) {  //free list 찾고
        place_seg(bp, asize); //free list 제거 및 해당 block에 저장
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize, CHUNKSIZE);               
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) //확장
        return NULL;
        place_ext(bp,asize); //extend해서 넣기  
    return bp;
}

void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
 
}



void *mm_realloc(void *ptr, size_t size)
{
    if(size==0){
        mm_free(ptr);
        return NULL;
    }
    if(ptr==NULL)
    return mm_malloc(size);
    size_t copySize=GET_SIZE(HDRP(ptr));
    size_t newSize= DSIZE * ((size + (DSIZE)+(DSIZE - 1)) / DSIZE);
    
    if(newSize>copySize){ //다음 free block 공간에 넣을 수 있다

        
        if (!GET_ALLOC(HDRP(NEXT_BLKP(ptr)))&&copySize +  GET_SIZE(HDRP(NEXT_BLKP(ptr))) >= newSize)
        {
            void*  nextptr = NEXT_BLKP(ptr);
            seg_delete(nextptr);

              PUT(HDRP(ptr), PACK(copySize +  GET_SIZE(HDRP(nextptr)), 1));
              PUT(FTRP(ptr), PACK(copySize +  GET_SIZE(HDRP(nextptr)), 1));
               return ptr;
        }
    }
    else{
        return ptr;
    }
    void*  newptr = mm_malloc(size);
    if (newptr == NULL) //새로운 malloc 실패시 null return
      return NULL;
    if (size < copySize)
      copySize = size;
    memcpy(newptr, ptr, copySize);
    mm_free(ptr);
    return newptr;
}
