/*
 * Copyright (c) 2012, 2013 ARM Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the company may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ARM LTD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ARM LTD BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>

#include "nano_malloc.h"
#include "common.h"

#ifndef MAX
#define MAX(a,b) ((a) >= (b) ? (a) : (b))
#endif


/* Define free_list as internal name to avoid conflict with user names */
#define free_list __malloc_free_list

#define ALIGN_TO(size, align) \
    (((size) + (align) -1) & ~((align) -1))

/* Alignment of allocated block */

#ifdef KIT68K
// Alignment to even address is sufficient for 68008
#define MALLOC_ALIGN (2U)
#define CHUNK_ALIGN (2U)
#else
#define MALLOC_ALIGN (8U)
#define CHUNK_ALIGN (sizeof(void*))
#endif

#define MALLOC_PADDING ((MAX(MALLOC_ALIGN, CHUNK_ALIGN)) - CHUNK_ALIGN)

/* as well as the minimal allocation size
 * to hold a free pointer */
#define MALLOC_MINSIZE (sizeof(void *))

typedef struct malloc_chunk
{
    /*          ------------------
     *   chunk->| size (4 bytes) |
     *          ------------------
     *          | Padding for    |
     *          | alignment      |
     *          | holding neg    |
     *          | offset to size |
     *          ------------------
     * mem_ptr->| point to next  |
     *          | free when freed|
     *          | or data load   |
     *          | when allocated |
     *          ------------------
     */
    /* size of the allocated payload area, including size before
       CHUNK_OFFSET */
    int size;

    /* since here, the memory is either the next free block, or data load */
    struct malloc_chunk * next;
} chunk;

#define CHUNK_OFFSET ((size_t)(&(((struct malloc_chunk *)0)->next)))

/* size of smallest possible chunk. A memory piece smaller than this size
 * won't be able to create a chunk */
#define MALLOC_MINCHUNK (CHUNK_OFFSET + MALLOC_PADDING + MALLOC_MINSIZE)

static chunk * get_chunk_from_ptr(void * ptr)
{
    chunk * c = (chunk *)((char *)ptr - CHUNK_OFFSET);
    /* Skip the padding area */
    if (c->size < 0) c = (chunk *)((char *)c + c->size);
    return c;
}

static chunk* free_list;
static char myHeap[HEAP_SIZE];

void init_freelist(void) {
    free_list = (chunk *)(myHeap);
    free_list->size = HEAP_SIZE;
    free_list->next = NULL;
}

/** Algorithm:
  *   Walk through the free list to find the first match. If fails to find
  *   one, call sbrk to allocate a new chunk.
  */
void* nano_malloc(size_t s)
{
    chunk *p, *r;
    char * ptr, * align_ptr;
    int offset;

    size_t alloc_size;

    alloc_size = ALIGN_TO(s, CHUNK_ALIGN); /* size of aligned data load */
    alloc_size += MALLOC_PADDING; /* padding */
    alloc_size += CHUNK_OFFSET; /* size of chunk head */
    alloc_size = MAX(alloc_size, MALLOC_MINCHUNK);

    if (alloc_size < s)
    {
        return NULL;
    }

    p = free_list;
    r = p;

    while (r)
    {
        int rem = r->size - alloc_size;
        if (rem >= 0)
        {
            if (rem >= MALLOC_MINCHUNK)
            {
                /* Find a chunk that much larger than required size, break
                * it into two chunks and return the second one */
                r->size = rem;
                r = (chunk *)((char *)r + rem);
                r->size = alloc_size;
            }
            /* Find a chunk that is exactly the size or slightly bigger
             * than requested size, just return this chunk */
            else if (p == r)
            {
                /* Now it implies p==r==free_list. Move the free_list
                 * to next chunk */
                free_list = r->next;
            }
            else
            {
                /* Normal case. Remove it from free_list */
                p->next = r->next;
            }
            break;
        }
        p=r;
        r=r->next;
    }

    /* Failed to find a appropriate chunk. Give up */
    if (r == NULL)
    {
        return NULL;
    }

    ptr = (char *)r + CHUNK_OFFSET;

    align_ptr = (char *)ALIGN_TO((unsigned long)ptr, MALLOC_ALIGN);
    offset = align_ptr - ptr;

    if (offset)
    {
        *(int *)((char *)r + offset) = -offset;
    }

    return align_ptr;
}

#define MALLOC_CHECK_DOUBLE_FREE

/** Algorithm:
  *  Maintain a global free chunk single link list, headed by global
  *  variable free_list.
  *  When free, insert the to-be-freed chunk into free list. The place to
  *  insert should make sure all chunks are sorted by address from low to
  *  high.  Then merge with neighbor chunks if adjacent.
  */
void nano_free (void* free_p)
{
    chunk * p_to_free;
    chunk * p, * q;

    if (free_p == NULL) return;

    p_to_free = get_chunk_from_ptr(free_p);

    if (free_list == NULL)
    {
        /* Set first free list element */
        p_to_free->next = free_list;
        free_list = p_to_free;
        return;
    }

    if (p_to_free < free_list)
    {
        if ((char *)p_to_free + p_to_free->size == (char *)free_list)
        {
            /* Chunk to free is just before the first element of
             * free list  */
            p_to_free->size += free_list->size;
            p_to_free->next = free_list->next;
        }
        else
        {
            /* Insert before current free_list */
            p_to_free->next = free_list;
        }
        free_list = p_to_free;
        return;
    }

    q = free_list;
    /* Walk through the free list to find the place for insert. */
    do
    {
        p = q;
        q = q->next;
    } while (q && q <= p_to_free);

    /* Now p <= p_to_free and either q == NULL or q > p_to_free
     * Try to merge with chunks immediately before/after it. */

    if ((char *)p + p->size == (char *)p_to_free)
    {
        /* Chunk to be freed is adjacent
         * to a free chunk before it */
        p->size += p_to_free->size;
        /* If the merged chunk is also adjacent
         * to the chunk after it, merge again */
        if ((char *)p + p->size == (char *) q)
        {
            p->size += q->size;
            p->next = q->next;
        }
    }
#ifdef MALLOC_CHECK_DOUBLE_FREE
    else if ((char *)p + p->size > (char *)p_to_free)
    {
        /* Report double free fault */
        printf("Double free\n");
        return;
    }
#endif
    else if ((char *)p_to_free + p_to_free->size == (char *) q)
    {
        /* Chunk to be freed is adjacent
         * to a free chunk after it */
        p_to_free->size += q->size;
        p_to_free->next = q->next;
        p->next = p_to_free;
    }
    else
    {
        /* Not adjacent to any chunk. Just insert it. Resulting
         * a fragment. */
        p_to_free->next = q;
        p->next = p_to_free;
    }
}