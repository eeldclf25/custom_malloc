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
    ""};





typedef size_t default_t;

#define WSIZE (sizeof(default_t)) // default_t 사이즈로 워드 크기 설정
#define DSIZE (2 * WSIZE)   // 더블 워드 크기 설정

#define CHUNKSIZE (1 << 12)   // 힙 공간을 확장할 때 확장하고싶은 크기

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

#define ALIGN(size) (((size) + DSIZE - 1) & ~(DSIZE - 1)) // 들어온 size를 DSIZE의 가장 가까운 배수로 올림

#define PACK(size, alloc) ((size) | (alloc))    // 해당 size 비트에 alloc 비트를 or 연산

#define GET(p) (*(default_t *)(p))  // p 포인터 위치의 데이터 읽기
#define PUT(p, val) (*(default_t *)(p) = (val))  // p의 포인터 위치에 val 데이터 삽입

#define GET_SIZE(p) (GET(p) & ~(WSIZE - 1))    // 해당 주소 위치의 데이터를 WSZIE 배수로 GET
#define GET_ALLOC(p) (GET(p) & (0x1))    // 해당 주소 위치의 데이터의 ALLOC 비트로 GET

#define HDRP(bp) ((char *)(bp) - WSIZE) // bp 위치에서 헤더 위치를 출력하는 매크로
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)    // bp 위치에서 푸터 위치를 출력하는 매크로

#define NEXT_BP(bp) ((char *)(bp) + GET_SIZE((char *)bp - WSIZE))   // bp 위치에서 다음 블록 bp 위치를 출력하는 매크로
#define PREV_BP(bp) ((char *)(bp) - GET_SIZE((char *)bp - DSIZE))   // bp 위치에서 이전 블록 bp 위치를 출력하는 매크로

static char *heap_listp = NULL;





void *coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    if (!prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(PREV_BP(bp))) + GET_SIZE(HDRP(NEXT_BP(bp)));
        PUT(HDRP(PREV_BP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BP(bp)), PACK(size, 0));
        bp = PREV_BP(bp);
    }
    else if (!prev_alloc && next_alloc) {
        size += GET_SIZE(HDRP(PREV_BP(bp)));
        PUT(HDRP(PREV_BP(bp)), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        bp = PREV_BP(bp);
    }
    else if (prev_alloc && !next_alloc) {
        size += GET_SIZE(HDRP(NEXT_BP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    
    return bp;
}

/*
 * extend_heap - 비어있는 힙 공간을 bytes 만큼의 크기를 가진 새로운 가용 블록(free block)으로 확장
 */
void *extend_heap(size_t bytes)
{
    void *bp = mem_sbrk(bytes);

    if (bp == (void *)-1)
        return NULL;

    PUT(HDRP(bp), PACK(bytes, 0));   // 새로운 가용 블록 헤더 설정
    PUT(FTRP(bp), PACK(bytes, 0));   // 새로운 가용 블록 푸터 설정
    PUT(HDRP(NEXT_BP(bp)), PACK(0, 1));  // 새로운 가용 블록 에필로그 헤더 설정
    
    return coalesce(bp);
}

/*
 * mm_init - malloc 할당기 초기화
 */
int mm_init(void)
{
    heap_listp = mem_sbrk(4 * WSIZE); // 찍어보니 무조건 16배수로 반환

    if (heap_listp == (void *)-1)
        return -1;
    
    PUT(heap_listp + (0 * WSIZE), PACK(0, 0));
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));
    
    heap_listp += (2 * WSIZE);
    
    if (extend_heap(CHUNKSIZE) == NULL)
        return -1;

    return 0;
}

/*
 * find_fit - 해당 bytes의 가용 블록이 있는지 찾는 함수
 */
void *find_fit(size_t bytes)
{
    for (void *bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BP(bp))
        if (!GET_ALLOC(HDRP(bp)) && (GET_SIZE(HDRP(bp)) >= bytes))
            return bp;
    
    return NULL;
}

/*
 * place - 해당 bp(free block)에 alloc 하는 함수 
 */
void place(void *bp, size_t alloc_size)
{
    size_t free_size = GET_SIZE(HDRP(bp));

    if ((free_size - alloc_size) >= (DSIZE + DSIZE)) {
        PUT(HDRP(bp), PACK(alloc_size, 1));
        PUT(FTRP(bp), PACK(alloc_size, 1));

        bp = NEXT_BP(bp);

        PUT(HDRP(bp), PACK(free_size - alloc_size, 0));
        PUT(FTRP(bp), PACK(free_size - alloc_size, 0));
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
    void *check_bp = find_fit(new_size);

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










void *mm_realloc(void *oldP, size_t size)
{
    if (oldP == NULL)
        return mm_malloc(size);
    if (size == 0) {
        mm_free(oldP);
        return NULL;
    }

    // 1) 새로운 블록 할당
    void *newP = mm_malloc(size);
    if (newP == NULL)
        return NULL;  // 할당 실패 시 원본 블록은 그대로 유지

    // 2) 복사할 페이로드 크기 계산 (전체 블록 크기에서 헤더+푸터 오버헤드 제거)
    size_t old_block_size = GET_SIZE(HDRP(oldP));      // 전체 블록 크기
    size_t old_payload  = old_block_size - DSIZE;      // 실제 페이로드 크기
    size_t copySize     = (size < old_payload) ? size  // 요청 크기 vs. 페이로드 크기
                                             : old_payload;

    // 3) 안전하게 복사
    memmove(newP, oldP, copySize);

    // 4) 기존 블록 해제
    mm_free(oldP);

    return newP;
}








// void *mm_malloc2(size_t size)
// {
//     int newsize = ALIGN(size + WSIZE);
//     void *p = mem_sbrk(newsize);
//     if (p == (void *)-1)
//         return NULL;
//     else
//     {
//         *(size_t *)p = size;
//         return (void *)((char *)p + WSIZE);
//     }
// }

// void mm_free2(void *ptr)
// {
// }

// void *mm_realloc(void *ptr, size_t size)
// {
//     void *oldptr = ptr;
//     void *newptr;
//     size_t copySize;

//     newptr = mm_malloc2(size);
//     if (newptr == NULL)
//         return NULL;
//     copySize = *(size_t *)((char *)oldptr - WSIZE);
//     if (size < copySize)
//         copySize = size;
//     memcpy(newptr, oldptr, copySize);
//     mm_free2(oldptr);
//     return newptr;
// }