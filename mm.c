#include "./mm.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <limits.h>

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
#define ORIGINAL(now_block) (void *) ((char *)now_block - SIZE_T_SIZE)

#define BLOCK_SIZE(now_block) (*(size_t *)now_block & ~0x7)
#define SET_SIZE(now_block, new_size) (*(size_t *)now_block = new_size)

#define GET_NEXT_FREE_ADDR(now_block) ((void *) ((char *)now_block + sizeof(size_t) + sizeof(size_t *)))
#define GET_PREV_FREE_ADDR(now_block) ((void *) ((char *)now_block + sizeof(size_t)))
#define GET_NEXT_FREE(now_block) ((void *) *(size_t *)GET_NEXT_FREE_ADDR(now_block))
#define GET_PREV_FREE(now_block) ((void *) *(size_t *)GET_PREV_FREE_ADDR(now_block))
#define SET_NEXT_FREE(now_block, next_block) {size_t ** tmp = (size_t **) GET_NEXT_FREE_ADDR(now_block); *tmp = (size_t *)next_block;}
#define SET_PREV_FREE(now_block, prev_block) {size_t ** tmp = (size_t **) GET_PREV_FREE_ADDR(now_block); *tmp = (size_t *)prev_block;}


/* ---------------------------------------Debug------------------------------------------- */

int c = 0;

void DISP_PROGRESS()
{
    c++;

    if (DISP)
    {
        if (c % 1000 == 0)
        {
            printf("\r%d", c);
        }

        if (c == 1578816)
        { printf("\r-----completed-----\n"); }
        fflush(stdout);
    }

    if (DEBUG && c == 14401)
    {
        int a = 1;
    }
}


/* -----------------------------------list------------------------------------------------ */

typedef struct _list
{
    char *head;
    char *tail;
    size_t max_capacity;
    size_t min_capacity;
    size_t cnt;
} List;

void list_init(List *l, size_t min, size_t max)
{
    l->head = NULL;
    l->tail = NULL;
    l->max_capacity = max;
    l->min_capacity = min;
    l->cnt = 0;
}

/*
 * remove block from list, return next block in free_list
 * */
void *list_remove(List *free_list, void *block)
{
    size_t *prev_free = GET_PREV_FREE(block);
    size_t *next_free = GET_NEXT_FREE(block);

    if (NULL != prev_free)
    {
        SET_NEXT_FREE(prev_free, next_free);
    }

    if (NULL != next_free)
    {
        SET_PREV_FREE(next_free, prev_free);
    }

    if (free_list->head == block)
    {
        free_list->head = (char *) next_free;
    }

    if (free_list->tail == block)
    {
        free_list->tail = (char *) prev_free;
    }

    free_list->cnt -= 1;

    return next_free;
}

/*
 * insert block after prev_block, if prev_block is NULL, equal to push_head
 * */
void list_insert_after(List *free_list, void *prev_block, void *block)
{
    if (prev_block == NULL)
    {
        // insert head
        SET_NEXT_FREE(block, free_list->head);
        SET_PREV_FREE(block, NULL);

        if (0 != free_list->cnt)
        {
            SET_PREV_FREE(free_list->head, block);
        }
        else
        {
            free_list->tail = block;
        }

        free_list->head = block;
    }
    else
    {
        // insert after prev_block
        void *next_free = GET_NEXT_FREE(prev_block);
        SET_NEXT_FREE(block, next_free);
        SET_PREV_FREE(block, prev_block);
        SET_NEXT_FREE(prev_block, block);
        if (next_free != NULL)
        {
            SET_PREV_FREE(next_free, block);
        }
        else
        {
            free_list->tail = block;
        }

    }

    free_list->cnt += 1;
}

List g_free_list;


/* -----------------------------------------tools---------------------------------------- */

void *get_new_block(List *free_list, size_t new_size);

void *split(List *free_list, void *block, size_t new_size);

void recycle(List *free_list, void *block, size_t new_size, void (*free)(List *, void *));

void *find_first_fit(List *free_list, size_t new_size);

void *find_best_fit(List *free_list, size_t new_size);

void lifo_free(List *free_list, void *block);

void ao_free(List *free_list, void *block);

void *_mm_malloc(List *free_list, size_t new_size, void *(*list_find)(List *, size_t));

void _mm_free(void *block, void (*free)(List *, void *));

void *_mm_realloc(List *free_list, void *old_block, size_t new_size, void *(*list_find)(List *, size_t),
                  void (*free)(List *, void *));


/* -----------------------------------------malloc---------------------------------------- */

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
    DISP_PROGRESS();

    _mm_free(ORIGINAL(ptr), ao_free);
}

/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t free_size)
{
    DISP_PROGRESS();

    size_t new_size = ALIGN(free_size + SIZE_T_SIZE);

    return _mm_realloc(&g_free_list, ORIGINAL(ptr), new_size, find_first_fit, lifo_free);

}

void *_mm_malloc(List *free_list, size_t new_size, void *(*list_find)(List *, size_t))
{
    // Find fit block
    void *now_block = list_find(free_list, new_size);

    if (now_block == NULL)
    {
        now_block = get_new_block(free_list, new_size);
    }
    else
    {
        now_block = split(free_list, now_block, new_size);
    }

    return AVAILABLE(now_block);
}

void _mm_free(void *block, void (*free)(List *, void *))
{
    free(&g_free_list, block);
}

void *_mm_realloc(List *free_list, void *old_block, size_t new_size, void *(*list_find)(List *, size_t),
                  void (*free)(List *, void *))
{
    if (old_block == NULL)
    {
        return mm_malloc(new_size);
    }

    if (new_size == 0)
    {
        mm_free(old_block);
        return NULL;
    }

    if (new_size == BLOCK_SIZE(old_block))
    {
        // equal -> return
        return AVAILABLE(old_block);
    }
    else if (new_size < BLOCK_SIZE(old_block))
    {
        // small -> resize, add remain to freelist
        recycle(free_list, old_block, new_size, free);

        return AVAILABLE(old_block);
    }
    else
    {
        // big -> check old merge, get new block, copy data, split and free later block
        size_t *stage_pointer[2] = {GET_PREV_FREE(old_block), GET_NEXT_FREE(old_block)};
        size_t old_size = BLOCK_SIZE(old_block);

        _mm_free(old_block, free);  // may merge with other block

        void *new_block = list_find(free_list, new_size);   // instead of malloc(), malloc break data in split()

        if (new_block == NULL)
        {
            new_block = get_new_block(free_list, new_size);

            if (new_block == NULL)
            {
                return NULL;
            }
        }
        else
        {
            list_remove(free_list, new_block);
        }

        if (old_block != new_block)
        {
            memmove(new_block, old_block, old_size);
        }

        SET_SIZE(new_block, new_size);
        SET_PREV_FREE(new_block, stage_pointer[0]);
        SET_NEXT_FREE(new_block, stage_pointer[1]);

        recycle(free_list, new_block, new_size, free);

        return AVAILABLE(new_block);
    }
}

void *find_first_fit(List *free_list, size_t new_size)
{
    for (void *now_block = free_list->head; now_block != NULL; now_block = GET_NEXT_FREE(now_block))
    {
        if (BLOCK_SIZE(now_block) >= new_size)
        {
            return now_block;
        }
    }

    return NULL;
}

void *find_best_fit(List *free_list, size_t new_size)
{
    long long int min_diff = LLONG_MIN;
    void *min_diff_block = NULL;
    long long int diff;

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

    return NULL;
}

void lifo_free(List *free_list, void *new_block)
{
    void *ultimate_block = new_block;                                  // merge blocks to there
    int total_size = BLOCK_SIZE(new_block);                             // totally merge size
    void *next_block = (char *) new_block + BLOCK_SIZE(new_block);     // new_block can merge with this block

    for (void *old_block = free_list->head; old_block != NULL;)
    {
        if (old_block == next_block)
        {
            // some block can merge after new_block
            total_size += BLOCK_SIZE(old_block);
            old_block = list_remove(free_list, old_block);
        }
        else
        {
            if ((char *) old_block + BLOCK_SIZE(old_block) == new_block)
            {
                // new_block can merge with before
                ultimate_block = old_block;
                total_size += BLOCK_SIZE(old_block);
            }

            old_block = GET_NEXT_FREE(old_block);
        }
    }

    SET_SIZE(ultimate_block, total_size);

    if (ultimate_block == new_block)
    {
        list_insert_after(free_list, NULL, new_block);
    }
}

void ao_free(List *free_list, void *new_block)
{
    void *prev_block = NULL;
    void *ultimate_block = new_block;
    int total_size = BLOCK_SIZE(new_block);
    void *next_block = (char *) new_block + BLOCK_SIZE(new_block);

    for (void *old_block = free_list->head; old_block != NULL;)
    {
        if (old_block >= next_block)
        {
            // test whether merge or not
            if (old_block == next_block)
            {
                total_size += BLOCK_SIZE(old_block);
                old_block = list_remove(free_list, old_block);
            }
            break;
        }
        else
        {
            // if can merge, do not need insert.
            if ((char *) old_block + BLOCK_SIZE(old_block) == new_block)
            {
                ultimate_block = old_block;
                total_size += BLOCK_SIZE(old_block);
            }

            else if (prev_block == NULL && GET_NEXT_FREE(old_block) > new_block)
            {
                prev_block = old_block;
            }

            old_block = GET_NEXT_FREE(old_block);
        }
    }

    SET_SIZE(ultimate_block, total_size);

    if (ultimate_block == new_block)
    {
        list_insert_after(free_list, prev_block, new_block);
    }
}

void *get_new_block(List *free_list, size_t new_size)
{
    void *tail_free = free_list->tail;

    // try to get block from free list tail
    if (tail_free && (char *) tail_free + BLOCK_SIZE(tail_free) == (char *) mem_heap_hi() + 1)
    {
        if (mem_sbrk(new_size - BLOCK_SIZE(tail_free)) == (void *) -1)
        {
            return NULL;
        }
        else
        {
            SET_SIZE(tail_free, new_size);
            list_remove(free_list, tail_free);
            return tail_free;
        }
    }
    else
    {
        void *block = (char *) mem_heap_hi() + 1;

        if (mem_sbrk(new_size) == (void *) -1)
        {
            return NULL;
        }
        else
        {
            SET_SIZE(block, new_size);
            return block;
        }
    }
}

/*
 * Split block by new size, return suitable block
 * */
void *split(List *free_list, void *block, size_t new_size)
{
    // Block is too large
    if (BLOCK_SIZE(block) - new_size >= 4 * sizeof(size_t))
    {
        size_t remain = BLOCK_SIZE(block) - new_size;
        SET_SIZE(block, remain);
        // Remain block need not be removed

        void *new_block = (void *) ((char *) block + remain);
        SET_SIZE(new_block, new_size);

        return new_block;
    }
    else
    {
        list_remove(free_list, block);
        return block;
    }
}

/*
 * Similar as split, it recycle redundant part at bottom
 * */
void recycle(List *free_list, void *block, size_t new_size, void (*free)(List *, void *))
{
    if (new_size + 4 * sizeof(size_t) <= BLOCK_SIZE(block))
    {
        size_t remain = BLOCK_SIZE(block) - new_size;
        SET_SIZE(block, remain);

        void *new_block = (void *) ((char *) block + remain);
        SET_SIZE(new_block, new_size);

        _mm_free(new_block, free);
    }
}
