#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct obj_metadata {
    size_t size;
    struct obj_metadata *next;
};

static const size_t metadata_size = sizeof(struct obj_metadata);
static const size_t align_to = 8;
static const size_t used = 1;

struct obj_metadata *first = NULL;
struct obj_metadata *last = NULL;
int list_size = 0;

size_t align(size_t size) {
    while (size % align_to != 0)
        size += 1;
    return size;
}

void insert(void *block, size_t size) {
    struct obj_metadata *node = block;
    if (first == NULL)
        first = node;
    else
        last->next = node;
    last = node;
    last->size = size + used;
    last->next = NULL;
    list_size += 1;
}

void *reuse(size_t size) {
    struct obj_metadata *k = first;
    while (k != NULL) {
        if (k->size % align_to == 0 && size <= k->size) {
            void *block = k + metadata_size;
            k->size += used;
            return block;
        }
        k = k->next;
    }
    return NULL;
}

void *mymalloc(size_t size) {
    if (size > 0) {
        size_t block_size = align(size);
        void *mem_block = reuse(block_size);
        if (mem_block != NULL)
            return mem_block;
        sbrk(0);
        mem_block = sbrk(block_size + metadata_size);
        if (mem_block != NULL) {
            insert(mem_block, block_size);
            return mem_block + metadata_size;
        }
    }
    return NULL;
}

void *mycalloc(size_t nmemb, size_t size) {
    if (size > 0 && nmemb > 0) {
        void *block;
        sbrk(0);
        void *block_start = sbrk(metadata_size);
        for (size_t i = 0; i < nmemb; i++) {
            block = sbrk(size);
            memset(block, 0, size);
        }
        size_t block_size = align(nmemb * size);
        size_t remain = block_size - nmemb * size;
        block = sbrk(remain);
        if (block_start) {
            insert(block_start, block_size);
            return block_start + metadata_size;
        }
    }
    return NULL;
}

void unmap() {
    struct obj_metadata *k = first;
    struct obj_metadata *m = NULL;
    int last_node = 0;
    while (k != NULL) {
        if (list_size > 2 && k->next != NULL) {
            if (k->next->next == NULL) {
            	last_node = 1;
            	m = k;
            }
        }
        else if (last_node == 1)
            last_node = 2;
        if (k->size % align_to == 0 && k->next == NULL) {
            if (list_size == 1)
                first = last = NULL;
            else if (list_size == 2) {
            	first->next = NULL;
            	last = first;
            }
            else if (last_node == 2) {
            	m->next = NULL;
                last = m;
            }
            list_size -= 1;
            last_node = brk(k);
            k = first;
        }
        else
            k = k->next;
    }
}

void myfree(void *ptr) {
    if (ptr != NULL) {
        struct obj_metadata *k = ptr - metadata_size;
        k->size -= used;
        unmap();
    }
}

void *myrealloc(void *ptr, size_t size) {
    if (ptr != NULL) {
        if (size == 0)
            free(ptr);
        else {
            struct obj_metadata *k = ptr - metadata_size;
            size_t old_size = k->size - used;
            size = align(size);
            if (old_size < size) {
                void *new_ptr = mymalloc(size);
                memcpy(new_ptr, ptr, old_size + 1);
                myfree(ptr);
                return new_ptr;
            }
            else
                return ptr;
        }
    }
    else if (ptr == NULL)
        return mymalloc(size);
    return NULL;
}

/*
 * Enable the code below to enable system allocator support for your allocator.
 */
#if 0
void *malloc(size_t size) { return mymalloc(size); }
void *calloc(size_t nmemb, size_t size) { return mycalloc(nmemb, size); }
void *realloc(void *ptr, size_t size) { return myrealloc(ptr, size); }
void free(void *ptr) { myfree(ptr); }
#endif
