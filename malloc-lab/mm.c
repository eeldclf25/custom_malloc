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
#include <stdint.h>
#include <limits.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "eeldclf25",
    /* First member's full name */
    "eeldclf25",
    /* First member's email address */
    "d@d.d",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};





typedef size_t default_t;   // mm.c 에서 사용할 워드 크기(해당 타입 크기로 진행)

#define WSIZE (sizeof(default_t)) // default_t 사이즈로 워드 크기 설정
#define DSIZE (2 * WSIZE)   // 더블 워드 크기 설정

#define CHUNKSIZE (1 << 6)   // 힙 공간을 확장할 때 확장하고싶은 크기

#define MAX(x, y) (((x) > (y)) ? (x) : (y)) // x, y 중에 더 큰값 반환
#define ALIGN(size) (((size) + DSIZE - 1) & ~(DSIZE - 1)) // 들어온 size를 DSIZE의 가장 가까운 배수로 올림

#define PACK(size, alloc) ((size) | (alloc))    // 해당 size 비트에 alloc 비트를 수정

#define GET(p) (*(default_t *)(p))  // p 포인터 위치의 데이터 읽기
#define PUT(p, val) (*(default_t *)(p) = (default_t)(val))  // p의 포인터 위치에 val 데이터 삽입

#define GET_SIZE(p) (GET(p) & ~(WSIZE - 1))    // 해당 주소 위치의 데이터를 WSZIE 배수로 GET
#define GET_ALLOC(p) (GET(p) & (0x1))    // 해당 주소 위치의 데이터의 ALLOC 비트를 GET

#define HDRP(bp) ((char *)(bp) - WSIZE) // bp 위치에서 헤더 위치를 출력하는 매크로
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)    // bp 위치에서 푸터 위치를 출력하는 매크로

#define PREV_BP(bp) ((char *)(bp) - GET_SIZE((char *)bp - DSIZE))   // bp 위치에서 이전 블록 bp 위치를 출력하는 매크로
#define NEXT_BP(bp) ((char *)(bp) + GET_SIZE((char *)bp - WSIZE))   // bp 위치에서 다음 블록 bp 위치를 출력하는 매크로

#define PRED(bp) ((char *)(bp))
#define SUCC(bp) ((char *)(bp) + WSIZE)

#define PRED_BP(bp) (GET((char *)(bp)))
#define SUCC_BP(bp) (GET((char *)(bp) + WSIZE))

#define UNLINK(bp) \
    PUT(PRED(SUCC_BP(bp)), PRED_BP(bp)); \
    PUT(SUCC(PRED_BP(bp)), SUCC_BP(bp))

#define LINK(bp, pred, succ) \
    PUT(PRED(succ), (bp)); \
    PUT(PRED(bp), (pred)); \
    PUT(SUCC(bp), (succ)); \
    PUT(SUCC(pred), (bp))

static char *heap_prologue = NULL;
static char *heap_epilogue = NULL;





#define USE_BEST_FIT





/*
 * coalesce - 해당 free 블록의 앞뒤를 확인하고 병합하는 함수
 */
void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(HDRP(PREV_BP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (!prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(PREV_BP(bp))) + GET_SIZE(HDRP(NEXT_BP(bp)));
        UNLINK(PREV_BP(bp));
        UNLINK(NEXT_BP(bp));
        PUT(HDRP(PREV_BP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BP(bp)), PACK(size, 0));
        bp = PREV_BP(bp);
    }
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BP(bp)));
        UNLINK(PREV_BP(bp));
        PUT(HDRP(PREV_BP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BP(bp);
    }
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BP(bp)));
        UNLINK(NEXT_BP(bp));
        PUT(FTRP(NEXT_BP(bp)), PACK(size, 0));
        PUT(HDRP(bp), PACK(size, 0));
    }

    LINK(bp, heap_prologue, SUCC_BP(heap_prologue));

    return bp;
}

/*
 * extend_heap - 비어있는 힙 공간을 bytes 만큼의 크기를 가진 새로운 free 블록으로 확장
 */
void *extend_heap(size_t bytes)
{
    void *bp = heap_epilogue;
    if (mem_sbrk(bytes) == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(bytes, 0));
    PUT(FTRP(bp), PACK(bytes, 0));
    
    heap_epilogue = NEXT_BP(bp);
    PUT(HDRP(heap_epilogue), PACK(DSIZE + DSIZE, 1));
    PUT(PRED(heap_epilogue), PACK(0, 0));
    PUT(SUCC(heap_epilogue), PACK(0, 0));
    PUT(FTRP(heap_epilogue), PACK(DSIZE + DSIZE, 1));
    
    LINK(bp, PRED_BP(bp), heap_epilogue);
    UNLINK(bp);
    
    return coalesce(bp);
}

/*
 * mm_init - malloc 할당기 초기화
 */
int mm_init(void)
{
    char *heap_listp = mem_sbrk(8 * WSIZE); // 찍어보니 무조건 16배수로 반환

    if (heap_listp == (void *)-1)
        return -1;

    PUT(heap_listp + (0 * WSIZE), PACK(DSIZE + DSIZE, 1));
    PUT(heap_listp + (1 * WSIZE), PACK(0, 0));
    PUT(heap_listp + (2 * WSIZE), PACK(0, 0));
    PUT(heap_listp + (3 * WSIZE), PACK(DSIZE + DSIZE, 1));
    PUT(heap_listp + (4 * WSIZE), PACK(DSIZE + DSIZE, 1));
    PUT(heap_listp + (5 * WSIZE), PACK(0, 0));
    PUT(heap_listp + (6 * WSIZE), PACK(0, 0));
    PUT(heap_listp + (7 * WSIZE), PACK(DSIZE + DSIZE, 1));

    heap_prologue = heap_listp + (1 * WSIZE);
    heap_epilogue = heap_listp + (5 * WSIZE);

    PUT(SUCC(heap_prologue), heap_epilogue);
    PUT(PRED(heap_epilogue), heap_prologue);
    
    if (extend_heap(CHUNKSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * first_fit - 프롤로그에서 에필로그까지 체크해서, 해당 bytes의 free 블록이 있는지 찾는 함수
 */
void *first_fit(size_t bytes)
{
#ifdef USE_BEST_FIT
    default_t max = ULONG_MAX;
    void *returnp = NULL;
    for (void *bp = heap_prologue; bp != heap_epilogue; bp = (void *)SUCC_BP(bp)) {
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= bytes)) {
            if (GET_SIZE(HDRP(bp)) == bytes) {
                return bp;
            }
            else if (GET_SIZE(HDRP(bp)) < max) {
                max = GET_SIZE(HDRP(bp));
                returnp = bp;
            }
        }
    }
    return returnp;
#else
    for (void *bp = heap_prologue; bp != heap_epilogue; bp = SUCC_BP(bp))
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= bytes))
            return bp;
    return NULL;
#endif
}

/*
 * place - 해당 bp(free block)에 alloc 하는 함수 
 */
void place(void *bp, size_t alloc_size)
{
    size_t free_size = GET_SIZE(HDRP(bp));
    UNLINK(bp);

    if ((free_size - alloc_size) >= (DSIZE + DSIZE)) {
        PUT(HDRP(bp), PACK(alloc_size, 1));
        PUT(FTRP(bp), PACK(alloc_size, 1));

        bp = NEXT_BP(bp);
        
        PUT(HDRP(bp), PACK(free_size - alloc_size, 0));
        PUT(FTRP(bp), PACK(free_size - alloc_size, 0));
        LINK(bp, heap_prologue, SUCC_BP(heap_prologue));
    }
    else {
        PUT(HDRP(bp), PACK(free_size, 1));
        PUT(FTRP(bp), PACK(free_size, 1));
    }
}

/*
 * mm_malloc - 블록을 할당하는 함수
 */
void *mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;
    
    size_t new_size = (size <= DSIZE) ? (DSIZE + DSIZE) : (ALIGN(size) + DSIZE);
    void *check_bp = first_fit(new_size);

    if (check_bp == NULL)
        check_bp = extend_heap(MAX(new_size, CHUNKSIZE));

    if (check_bp != NULL) {
        place(check_bp, new_size);
        return check_bp;
    }
    else {
        return NULL;
    }
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    size_t size = GET_SIZE(HDRP(ptr));

    PUT(HDRP(ptr), PACK(size, 0));
    PUT(FTRP(ptr), PACK(size, 0));

    coalesce(ptr);
}

/*
 * mm_realloc - 해당 블록을 해당 사이즈만큼 늘리는 함수
 */
void *mm_realloc(void *oldP, size_t size)
{
    if (oldP == NULL) {
        return mm_malloc(size);
    }
    else if (size == 0) {
        mm_free(oldP);
        return NULL;
    }

    void *newP = mm_malloc(size);
    
    if (newP == NULL)
        return NULL;

    size_t old_payload = GET_SIZE(HDRP(oldP)) - DSIZE;
    size_t copy_payload = (size < old_payload) ? size : old_payload;

    memmove(newP, oldP, copy_payload);
    mm_free(oldP);

    return newP;
}