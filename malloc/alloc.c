/**
 * author: Jiaqi Cao
 * malloc
 * CS 241 - Spring 2021
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>

typedef struct struct_metadata_t {
    void *ptr;
    size_t size;
    int free;
    struct struct_metadata_t *next;
    struct struct_metadata_t *prev;
} metadata_t;

#define min(X, Y) (((X) < (Y)) ? (X) : (Y))
#define BLOCKSIZE sizeof(metadata_t)
static size_t requested = 0;
static size_t allocated = 0;
static metadata_t *head = NULL;


void split(metadata_t *metadata, size_t size) {
    metadata_t *newmetadata = metadata->ptr + size;
    newmetadata->ptr = (newmetadata + 1);
    newmetadata->free = 1;
    newmetadata->size = metadata->size - size - BLOCKSIZE;
    newmetadata->next = metadata;
    if (metadata->prev) {
      metadata->prev->next = newmetadata;
    } else {
      head = newmetadata;
    }
    newmetadata->prev = metadata->prev;
    metadata->size = size;
    metadata->prev = newmetadata;
   
}

void coalescePrev(metadata_t * metadata) {
    if (metadata->prev && metadata->prev->free == 1) {
        metadata->size += metadata->prev->size+BLOCKSIZE;
        metadata->prev = metadata->prev->prev;
        if (metadata->prev) {
        metadata->prev->next = metadata;
        }
        else {
        head = metadata;
        }
        requested -= BLOCKSIZE;
    }   
}
void coalesceNext(metadata_t * input) {
    if (input->next && input->next->free == 1) {
        input->next->size += input->size+BLOCKSIZE;
        input->next->prev = input->prev;
        if (input->prev) {
        input->prev->next = input->next;
        } else {
        head = input->next;
        }
        requested -= BLOCKSIZE;
    }
}

/**
 * Allocate space for array in memory
 *
 * Allocates a block of memory for an array of num elements, each of them size
 * bytes long, and initializes all its bits to zero. The effective result is
 * the allocation of an zero-initialized memory block of (num * size) bytes.
 *
 * @param num
 *    Number of elements to be allocated.
 * @param size
 *    Size of elements.
 *
 * @return
 *    A pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory, a
 *    NULL pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/calloc/
 */

void *calloc(size_t num, size_t size) {
    // implement calloc!
    void *result = malloc(num * size);
    if (!result) return NULL;
    memset(result, 0, num * size);
    return result;
}

/**
 * Allocate memory block
 *
 * Allocates a block of size bytes of memory, returning a pointer to the
 * beginning of the block.  The content of the newly allocated block of
 * memory is not initialized, remaining with indeterminate values.
 *
 * @param size
 *    Size of the memory block, in bytes.
 *
 * @return
 *    On success, a pointer to the memory block allocated by the function.
 *
 *    The type of this pointer is always void*, which can be cast to the
 *    desired type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a null pointer is returned.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/malloc/
 */

void *malloc(size_t size) {
    // implement malloc!
    if (size == 0) return NULL; 
    metadata_t *p = head;
    metadata_t *chosen = NULL;
    if (allocated-requested >= size) {
      while(p!=NULL) {
        if (p->free && p->size >= size) {
          chosen = p; // first fit 
          if (p->size >= 2*size && (p->size-size) >= 1024) {
            split(p, size);
            requested += BLOCKSIZE; 
          }
          break;
        }
        p = p->next;
      }
    }
    if (chosen) {
      chosen->free = 0;
      requested += chosen->size;
      return chosen->ptr;
    } 
    chosen = sbrk(BLOCKSIZE+size);
    if (chosen == (void *)-1)
        return NULL;
    chosen->ptr = chosen + 1;
    chosen->size = size;
    chosen->free = 0;
    chosen->next = head;
    if (head) {
        chosen->prev = head->prev;
        head->prev = chosen;
    } else {
        chosen->prev = NULL;
    }
    head = chosen;
    allocated += BLOCKSIZE+size;
    requested += BLOCKSIZE+size;

    return chosen->ptr;
}


/**
 * Deallocate space in memory
 *
 * A block of memory previously allocated using a call to malloc(),
 * calloc() or realloc() is deallocated, making it available again for
 * further allocations.
 *
 * Notice that this function leaves the value of ptr unchanged, hence
 * it still points to the same (now invalid) location, and not to the
 * null pointer.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(),
 *    calloc() or realloc() to be deallocated.  If a null pointer is
 *    passed as argument, no action occurs.
 */
void free(void *ptr) {
    // implement free!
    if (!ptr) return;
    metadata_t *p = (metadata_t *)ptr - 1;
    if(p->free) return;
    p->free = 1;
    requested -= p->size;
    coalescePrev(p);
    coalesceNext(p);
}

/**
 * Reallocate memory block
 *
 * The size of the memory block pointed to by the ptr parameter is changed
 * to the size bytes, expanding or reducing the amount of memory available
 * in the block.
 *
 * The function may move the memory block to a new location, in which case
 * the new location is returned. The content of the memory block is preserved
 * up to the lesser of the new and old sizes, even if the block is moved. If
 * the new size is larger, the value of the newly allocated portion is
 * indeterminate.
 *
 * In case that ptr is NULL, the function behaves exactly as malloc, assigning
 * a new block of size bytes and returning a pointer to the beginning of it.
 *
 * In case that the size is 0, the memory previously allocated in ptr is
 * deallocated as if a call to free was made, and a NULL pointer is returned.
 *
 * @param ptr
 *    Pointer to a memory block previously allocated with malloc(), calloc()
 *    or realloc() to be reallocated.
 *
 *    If this is NULL, a new block is allocated and a pointer to it is
 *    returned by the function.
 *
 * @param size
 *    New size for the memory block, in bytes.
 *
 *    If it is 0 and ptr points to an existing block of memory, the memory
 *    block pointed by ptr is deallocated and a NULL pointer is returned.
 *
 * @return
 *    A pointer to the reallocated memory block, which may be either the
 *    same as the ptr argument or a new location.
 *
 *    The type of this pointer is void*, which can be cast to the desired
 *    type of data pointer in order to be dereferenceable.
 *
 *    If the function failed to allocate the requested block of memory,
 *    a NULL pointer is returned, and the memory block pointed to by
 *    argument ptr is left unchanged.
 *
 * @see http://www.cplusplus.com/reference/clibrary/cstdlib/realloc/
 */

void *realloc(void *ptr, size_t size) {
    // implement realloc!
    if (!ptr) return malloc(size);
    metadata_t *metadata = ((metadata_t *)ptr) - 1;
    if(metadata->free) return NULL;
    if(metadata->ptr != ptr) return NULL;
    if (!size) free(ptr);
    size_t oldsize = metadata->size;
    if (oldsize >= 2*size && (oldsize-size) >= 1024) {
        split(metadata, size);
        requested -= metadata->prev->size;
    }
    if (oldsize >= size) {
        return ptr;
    } else if (metadata->prev && metadata->prev->free) {
        if( (oldsize + metadata->prev->size + BLOCKSIZE) >= size) {
            requested += metadata->prev->size;
            coalescePrev(metadata);
            return metadata->ptr;
        }
    }
    void *result = malloc(size);
    memcpy(result, ptr, oldsize);
    free(ptr);
    return result;
}
