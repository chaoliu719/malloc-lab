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

#include "./mm.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <unistd.h>
#include <string.h>

#include "./memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team =
        {
                /* Team name */
                "",
                /* First member's full name */
                "",
                /* First member's email address */
                "",
                /* Second member's full name (leave blank if none) */
                "",
                /* Second member's email address (leave blank if none) */
                ""
        };

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8


/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(free_size) (((free_size) + (ALIGNMENT-1)) & ~0x7)


#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* base block size: store size and two pointers */
#define BASE_BLOCK_SIZE (ALIGN(sizeof(size_t) + 2 * sizef(size_t *)))

#define AVAILABLE(now_block) ((void *)((char *)now_block + SIZE_T_SIZE))

#define GET_USED(now_block) (*(size_t *)now_block & 0x1)
#define SET_USED(now_block) (*(size_t *)now_block |= 0x1)
#define CLEAR_USED(now_block) (*(size_t *)now_block &= ~0x1)


#define GET_LAST_USED(now_block) (*(size_t *)now_block & 0x2)
#define SET_LAST_USED(now_block) (*(size_t *)now_block |= 0x2)
#define CLEAR_LAST_USED(now_block) (*(size_t *)now_block &= ~0x2)


#define BLOCK_SIZE(now_block) (*(size_t *)now_block & ~0x7)
#define SET_SIZE(now_block, new_size) (*(size_t *)now_block = new_size | GET_USED(now_block) | GET_LAST_USED(now_block))

#define BLOCK_TAIL(now_block) ((void *) ((char *)now_block + BLOCK_SIZE(now_block) - SIZE_T_SIZE))
#define NEXT_BLOCK(now_block) ((void *) ((char *)now_block + BLOCK_SIZE(now_block)))
#define NEXT_BLOCK_TAIL(now_block) ((void *) (BLOCK_TAIL(NEXT_BLOCK(now_block))))
#define PREV_BLOCK_TAIL(now_block) ((void *) ((char *)now_block - SIZE_T_SIZE))
#define PREV_BLOCK(now_block) ((void *) ((char *)now_block - BLOCK_SIZE(PREV_BLOCK_TAIL(now_block))))

#define GET_NEXT_FREE(now_block) ((void *) ((char *)now_block + sizeof(size_t) + sizeof(size_t *)))
#define GET_PREV_FREE(now_block) ((void *) ((char *)now_block + sizeof(size_t)))
#define SET_NEXT_FREE(now_block, next_block) {size_t ** tmp = (size_t **) GET_NEXT_FREE(now_block); *tmp = (size_t *)next_block;}
#define SET_PREV_FREE(now_block, prev_block) {size_t ** tmp = (size_t **) GET_PREV_FREE(now_block); *tmp = (size_t *)prev_block;}


#define DISP_PROGRESS() {static int c = 0; if (!c) {printf("123456");}c++;\
    if (c % 50 == 0) {printf("\b\b\b\b\b\b%5.2f%%", (double)c*100/731821);}\
    if (c > 730000) {printf("\b\b\b\b\b\b      ");}fflush(stdout);\
}

typedef struct _list
{
    char *head;
    size_t max_capacity;
    size_t min_capacity;
    size_t cnt;
} List;

void list_init(List *l, size_t min, size_t max)
{
    l->head = NULL;
    l->max_capacity = max;
    l->min_capacity = min;
    l->cnt = 0;
}

List g_free_list;

void *find_first_fit(List *free_list, size_t new_size);

void *find_best_fit(List *free_list, size_t new_size);

void push_lifo(List *free_list, void *block);

void push_ao(List *free_list, void *block);

void merge_lifo(void *block);

void merge_ao(void *block);

void *_mm_malloc(List *free_list, size_t new_size, void *(*get_block)(List *, size_t));

void _mm_free(void *block, void (*insert)(List *, void *), void (*merge)(void *));

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    list_init(&g_free_list, 0, (size_t) - 1);
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose free_size is a multiple of the alignment.
 */
void *mm_malloc(size_t free_size)
{
    DISP_PROGRESS();

    size_t new_size = ALIGN(free_size + SIZE_T_SIZE);

    return _mm_malloc(&g_free_list, new_size, find_first_fit);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    void *now_block = (void *) ((char *) ptr - SIZE_T_SIZE);

    _mm_free(now_block, push_lifo, merge_lifo);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t free_size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    if (oldptr == NULL)
    {
        newptr = mm_malloc(free_size);
        return newptr;
    }

    if (free_size == 0)
    {
        mm_free(oldptr);
        return NULL;
    }

    newptr = mm_malloc(free_size);
    if (newptr == NULL)
    {
        return NULL;
    }

    copySize = *(size_t * )((char *) oldptr - SIZE_T_SIZE);
    if (free_size < copySize)
    {
        copySize = free_size;
    }
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}


void *_mm_malloc(List *free_list, size_t new_size, void *(*get_block)(List *, size_t))
{
    // Get fit block
    void *now_block = get_block(free_list, new_size);

    if (NULL == now_block)
    {
        return NULL;
    }

    // Set next block's indicators
    if (NEXT_BLOCK(now_block) <= mem_heap_hi())
    {
        SET_LAST_USED(NEXT_BLOCK(now_block));
        SET_LAST_USED(NEXT_BLOCK_TAIL(now_block));
    }

    // Block is too large to alloc totally
    if (BLOCK_SIZE(now_block) - new_size >= 4 * sizeof(size_t))
    {
        size_t remain = BLOCK_SIZE(now_block) - new_size;
        void *new_block = (char *) now_block + new_size;

        SET_SIZE(now_block, remain);
        *(size_t * )BLOCK_TAIL(now_block) = *(size_t *) now_block;
        // Remain block need not change its position in global free list

        SET_SIZE(new_block, new_size);
        CLEAR_LAST_USED(new_block);
        SET_USED(new_block);

    }
    else
    {
        size_t *prev_free = GET_PREV_FREE(now_block);
        size_t *next_free = GET_NEXT_FREE(now_block);

        if (NULL != prev_free)
        {
            SET_NEXT_FREE(prev_free, next_free);
        }

        if (NULL != next_free)
        {
            SET_PREV_FREE(next_free, prev_free);
        }

        if (free_list->head == now_block)
        {
            free_list->head == GET_NEXT_FREE(now_block);
        }

        SET_USED(now_block);
    }

    return AVAILABLE(now_block);
}

void _mm_free(void *block, void (*insert)(List *, void *), void (*merge)(void *))
{
    insert(&g_free_list, block);
    merge(block);
}

void *find_first_fit(List *free_list, size_t new_size)
{
    void *now_block = NULL;
    for (now_block = free_list->head; now_block != NULL; now_block = GET_NEXT_FREE(now_block))
    {
        if (BLOCK_SIZE(now_block) >= new_size)
        {
            return now_block;
        }
    }

    now_block = (char *) mem_heap_hi() + 1;

    if (mem_sbrk(new_size) == (void *) -1)
    {
        return NULL;
    }
    else
    {
        SET_SIZE(now_block, new_size);
        CLEAR_USED(now_block);
        CLEAR_LAST_USED(now_block);
        SET_PREV_FREE(now_block, NULL);
        SET_NEXT_FREE(now_block, NULL);
        return now_block;
    }
}

void *find_best_fit(List *free_list, size_t new_size)
{
    size_t min_diff = (size_t) - 1;
    void *min_diff_block = NULL;
    size_t diff;

    for (void *now_block = free_list->head; now_block != NULL; now_block = GET_NEXT_FREE(now_block))
    {
        diff = BLOCK_SIZE(now_block) - new_size;
        if (0 == diff)
        {
            return now_block;
        }
        else if (0 < diff && min_diff > diff)
        {
            min_diff = diff;
            min_diff_block = now_block;
        }
    }


    if (min_diff_block != NULL)
    {
        return min_diff_block;
    }
    else
    {
        min_diff_block = (char *) mem_heap_hi() + 1;

        if (mem_sbrk(new_size) == (void *) -1)
        {
            return NULL;
        }
        else
        {
            SET_SIZE(min_diff_block, new_size);
            CLEAR_USED(min_diff_block);
            CLEAR_LAST_USED(min_diff_block);
            SET_PREV_FREE(min_diff_block, NULL);
            SET_NEXT_FREE(min_diff_block, NULL);
            return min_diff_block;
        }
    }
}

void push_lifo(List *free_list, void *block)
{
    assert(free_list != NULL);

    if (block > mem_heap_lo() && 0 == GET_LAST_USED(block))
    {
        // able to merge with preview block, do nothing

    }
    else
    {
        SET_NEXT_FREE(block, free_list->head);
        SET_PREV_FREE(block, NULL);
        free_list->head = block;

        if (0 != free_list->cnt)
        {
            SET_PREV_FREE(free_list->head, block);
        }


        free_list->cnt += 1;
    }
}

void merge_lifo(void *block)
{
    void *prev_free = NULL;
    void *next_free = NULL;
    void *now_block = block;
    size_t free_size = BLOCK_SIZE(now_block);

    if (now_block > mem_heap_lo() && 0 == GET_LAST_USED(now_block))
    {
        // able to merge with prev block
        free_size += BLOCK_SIZE(PREV_BLOCK(now_block));
        now_block = PREV_BLOCK(now_block);

        SET_SIZE(now_block, free_size);
        CLEAR_USED(now_block);

        *(size_t * )BLOCK_TAIL(now_block) = *(size_t *) now_block;
    }

    else if (NEXT_BLOCK(now_block) <= mem_heap_hi() && 0 == GET_USED(NEXT_BLOCK(now_block)))
    {
        //able to merge with next block

        // delete next block in list
        now_block = NEXT_BLOCK(now_block);
        prev_free = GET_PREV_FREE(now_block);
        next_free = GET_NEXT_FREE(now_block);

        if (prev_free != NULL)
        {
            SET_NEXT_FREE(prev_free, next_free);
        }

        if (next_free != NULL)
        {
            SET_PREV_FREE(next_free, prev_free);
        }

        free_size += BLOCK_SIZE(now_block);
        now_block = block;

        SET_SIZE(now_block, free_size);
        CLEAR_USED(now_block);

        *(size_t * )BLOCK_TAIL(now_block) = *(size_t *) now_block;
    }

    if (NEXT_BLOCK(now_block) <= mem_heap_hi())
    {
        CLEAR_LAST_USED(NEXT_BLOCK(now_block));
        CLEAR_LAST_USED(NEXT_BLOCK_TAIL(now_block));
    }
}

void push_ao(List *free_list, void *block)
{
    assert(free_list != NULL);

    if (free_list->cnt == 0 || free_list->head > (char *) block)
    {
        // insert head
        SET_PREV_FREE(block, NULL);
        SET_NEXT_FREE(block, free_list->head);
        free_list->head = (char *) block;
    }
    else
    {
        size_t *now_block = NULL;
        size_t *prev_block = (size_t *) free_list->head;

        for (now_block = (size_t * )GET_NEXT_FREE(free_list->head);
             now_block != NULL; now_block = GET_NEXT_FREE(now_block))
        {
            if (now_block > (size_t *) block)
            {
                //insert before now_block
                SET_NEXT_FREE(block, now_block);
                SET_PREV_FREE(block, prev_block);
                SET_PREV_FREE(now_block, block);
                SET_NEXT_FREE(prev_block, block);
                prev_block = NULL;
                break;
            }

            prev_block = now_block;
        }

        if (NULL != prev_block)
        {
            //insert tail;
            SET_NEXT_FREE(prev_block, block);
            SET_PREV_FREE(block, prev_block);
        }

    }

    free_list->cnt += 1;
}

void merge_ao(void *block)
{
    void *prev_free = NULL;
    void *next_free = NULL;
    void *now_block = block;
    size_t free_size = BLOCK_SIZE(now_block);

    if (NEXT_BLOCK(now_block) <= mem_heap_hi() && 0 == GET_USED(NEXT_BLOCK(now_block)))
    {
        // able to merge with next block
        next_free = GET_NEXT_FREE(NEXT_BLOCK(now_block));
        free_size += BLOCK_SIZE(NEXT_BLOCK(now_block));
    }
    else
    {
        next_free = GET_NEXT_FREE(now_block);
        if (NEXT_BLOCK(now_block) <= mem_heap_hi())
        {
            CLEAR_LAST_USED(NEXT_BLOCK(now_block));
            CLEAR_LAST_USED(NEXT_BLOCK_TAIL(now_block));
        }
    }

    if (now_block > mem_heap_lo() && 0 == GET_LAST_USED(now_block))
    {
        // able to merge with preview block
        prev_free = GET_PREV_FREE(PREV_BLOCK(now_block));
        free_size += BLOCK_SIZE(PREV_BLOCK(now_block));
        now_block = PREV_BLOCK(now_block);
    }
    else
    {
        prev_free = GET_PREV_FREE(now_block);
    }

    // merge block, modify list
    SET_SIZE(now_block, free_size);
    CLEAR_USED(now_block);

    *(size_t * )BLOCK_TAIL(now_block) = *(size_t *) now_block;

    SET_NEXT_FREE(now_block, next_free);
    SET_PREV_FREE(now_block, prev_free);

    if (prev_free != NULL)
    {
        SET_NEXT_FREE(prev_free, now_block);
    }

    if (next_free != NULL)
    {
        SET_PREV_FREE(next_free, now_block);
    }
}

