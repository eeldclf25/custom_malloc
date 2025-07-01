#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "mm.h"
#include "memlib.h"

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

// 기본 상수 및 매크로
#define ALIGNMENT 8
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define WSIZE 4             // Word size
#define DSIZE 8             // Double word size
#define CHUNKSIZE (1 << 12) // 초기 힙 확장량

#define MAX(x, y) ((x) > (y) ? (x) : (y))

#define PACK(size, alloc) ((size) | (alloc))             // header/footer 포맷
#define GET(p) (*(unsigned int *)(p))                    // 메모리 읽기
#define PUT(p, val) (*(unsigned int *)(p) = (val))       // 메모리 쓰기

#define GET_SIZE(p) (GET(p) & ~0x7)                      // 블록 크기
#define GET_ALLOC(p) (GET(p) & 0x1)                      // 할당 여부

#define HDRP(bp) ((char *)(bp) - WSIZE)                                // 헤더 주소
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)          // 푸터 주소

#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)))             // 다음 블록
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE((char *)(bp) - DSIZE)) // 이전 블록

// 수정된 최소 블록 크기: header(4) + pred(8) + succ(8) + footer(4) = 24 bytes
// 하지만 alignment를 위해 32 bytes로 설정
#define MIN_BLOCK_SIZE (4 * DSIZE)  // 32 bytes

// 수정된 명시적 가용 리스트에서 pred/succ 포인터 (8바이트 포인터)
#define PRED(bp) (*(void **)(bp))
#define SUCC(bp) (*(void **)((char *)(bp) + DSIZE))  // 8바이트 뒤에 succ

// SIZE_MAX 정의 (size_t의 최대값)
#ifndef SIZE_MAX
#define SIZE_MAX (~(size_t)0)
#endif

static char *heap_listp = NULL;
static void *free_listp = NULL; // free block list의 시작

// 함수 선언
static void *extend_heap(size_t words);
static void *coalesce(void *bp);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void insert_free_block(void *bp);
static void remove_free_block(void *bp);
static int is_valid_heap_addr(void *bp);

// 힙 주소 유효성 검사 함수
static int is_valid_heap_addr(void *bp) {
    if (bp == NULL) return 0;
    return ((char *)bp >= mem_heap_lo() && (char *)bp <= mem_heap_hi());
}

// 초기화
int mm_init(void) {
    // 초기 빈 힙 생성
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1)
        return -1;

    PUT(heap_listp, 0);                             // padding
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));  // prologue header
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));  // prologue footer
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));      // epilogue header
    heap_listp += (2 * WSIZE);

    free_listp = NULL;

    // 초기 힙 확장
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL)
        return -1;

    return 0;
}

// 힙 확장
static void *extend_heap(size_t words) {
    char *bp;
    size_t size = (words % 2) ? (words + 1) * WSIZE : words * WSIZE;
    size = MAX(size, MIN_BLOCK_SIZE);  // 최소 블록 사이즈 보장

    if ((bp = mem_sbrk(size)) == (void *)-1)
        return NULL;

    // header / footer 설정
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1)); // epilogue

    // 새 가용 블록의 pred/succ를 NULL로 초기화
    PRED(bp) = NULL;
    SUCC(bp) = NULL;

    return coalesce(bp);
}

// 연결 (수정된 버전)
static void *coalesce(void *bp) {
    void *prev_bp = PREV_BLKP(bp);
    void *next_bp = NEXT_BLKP(bp);
    
    // 안전한 할당 상태 확인
    int prev_alloc = 1;  // 기본값 할당됨
    int next_alloc = 1;  // 기본값 할당됨
    
    // 이전 블록이 유효한 힙 주소인지 확인
    if (is_valid_heap_addr(prev_bp) && prev_bp != heap_listp - DSIZE) {
        prev_alloc = GET_ALLOC(FTRP(prev_bp));
    }
    
    // 다음 블록이 유효한 힙 주소인지 확인
    if (is_valid_heap_addr(next_bp)) {
        next_alloc = GET_ALLOC(HDRP(next_bp));
    }
    
    size_t size = GET_SIZE(HDRP(bp));

    if (prev_alloc && next_alloc) {
        // case 1: 앞뒤 모두 할당됨
    } else if (prev_alloc && !next_alloc) {
        // case 2: 앞은 할당, 뒤는 가용
        remove_free_block(next_bp);
        size += GET_SIZE(HDRP(next_bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    } else if (!prev_alloc && next_alloc) {
        // case 3: 앞은 가용, 뒤는 할당
        remove_free_block(prev_bp);
        size += GET_SIZE(HDRP(prev_bp));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(prev_bp), PACK(size, 0));
        bp = prev_bp;
    } else {
        // case 4: 앞뒤 모두 가용
        remove_free_block(prev_bp);
        remove_free_block(next_bp);
        size += GET_SIZE(HDRP(prev_bp)) + GET_SIZE(HDRP(next_bp));
        PUT(HDRP(prev_bp), PACK(size, 0));
        PUT(FTRP(next_bp), PACK(size, 0));
        bp = prev_bp;
    }

    // 병합된 블록 내부 초기화
    PRED(bp) = NULL;
    SUCC(bp) = NULL;

    insert_free_block(bp);
    return bp;
}

// malloc
void *mm_malloc(size_t size) {
    size_t asize;
    size_t extendsize;
    char *bp;

    if (size == 0)
        return NULL;

    // 최소 블록 크기를 고려한 크기 계산
    if (size <= DSIZE) {
        asize = MIN_BLOCK_SIZE;
    } else {
        asize = ALIGN(size + DSIZE); // header + footer 포함
        asize = MAX(asize, MIN_BLOCK_SIZE);
    }

    if ((bp = find_fit(asize)) != NULL) {
        place(bp, asize);
        return bp;
    }

    // 메모리 부족 시 힙 확장
    extendsize = MAX(asize, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL)
        return NULL;

    place(bp, asize);
    return bp;
}

// free
void mm_free(void *bp) {
    if (bp == NULL) return;

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    
    // pred/succ 초기화
    PRED(bp) = NULL;
    SUCC(bp) = NULL;
    
    coalesce(bp);
}

// 재할당
void *mm_realloc(void *ptr, size_t size) {
    if (ptr == NULL) return mm_malloc(size);
    if (size == 0) {
        mm_free(ptr);
        return NULL;
    }

    void *newptr = mm_malloc(size);
    if (newptr == NULL) return NULL;

    size_t old_size = GET_SIZE(HDRP(ptr)) - DSIZE;
    if (size < old_size) old_size = size;
    memcpy(newptr, ptr, old_size);
    mm_free(ptr);
    return newptr;
}

// 할당 위치 지정
static void place(void *bp, size_t asize) {
    size_t csize = GET_SIZE(HDRP(bp));
    remove_free_block(bp);

    if ((csize - asize) >= MIN_BLOCK_SIZE) {
        // 분할 가능
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        void *next = NEXT_BLKP(bp);
        PUT(HDRP(next), PACK(csize - asize, 0));
        PUT(FTRP(next), PACK(csize - asize, 0));
        PRED(next) = NULL;
        SUCC(next) = NULL;
        insert_free_block(next);
    } else {
        // 전부 사용
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


static void *find_fit(size_t asize) {
    void *best_bp = NULL;
    size_t best_size = SIZE_MAX;
    
    for (void *bp = free_listp; bp != NULL; bp = SUCC(bp)) {
        size_t block_size = GET_SIZE(HDRP(bp));
        if (block_size >= asize) {
            if (block_size == asize) {
                // 완벽한 매치 - 즉시 반환
                return bp;
            }
            if (block_size < best_size) {
                best_bp = bp;
                best_size = block_size;
            }
        }
    }
    return best_bp;
}

// 가용 리스트 삽입 (맨 앞에 삽입)
static void insert_free_block(void *bp) {
    if (bp == NULL) return;
    
    // 할당 상태 확인
    if (GET_ALLOC(HDRP(bp)) != 0) return;

    PRED(bp) = NULL;
    SUCC(bp) = free_listp;

    if (free_listp != NULL)
        PRED(free_listp) = bp;

    free_listp = bp;
}

// 가용 리스트 제거 (수정된 안전한 버전)
static void remove_free_block(void *bp) {
    if (bp == NULL) return;

    // 유효 범위 체크
    if (!is_valid_heap_addr(bp)) return;

    void *pred = PRED(bp);
    void *succ = SUCC(bp);

    // pred 처리
    if (pred != NULL) {
        if (is_valid_heap_addr(pred)) {
            SUCC(pred) = succ;
        } else {
            // pred가 유효하지 않으면 리스트에서 bp를 head로 만듦
            free_listp = succ;
        }
    } else {
        free_listp = succ;
    }

    // succ 처리
    if (succ != NULL && is_valid_heap_addr(succ)) {
        PRED(succ) = pred;
    }

    // bp 자체 초기화
    PRED(bp) = NULL;
    SUCC(bp) = NULL;
}