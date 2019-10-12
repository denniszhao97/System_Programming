/**
 * Malloc Lab
 * CS 241 - Spring 2019
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct myblock{
  size_t size;
  struct myblock* prev;
  struct myblock* next;
  struct myblock* freelist_next;
  struct myblock* freelist_prev;
} myblock;

static myblock *freelist_head = NULL;
static myblock *freelist_tail = NULL;
static myblock *tail = NULL;

int isInFreelist(myblock *cur){
  if(cur == freelist_head || (cur->freelist_next || cur->freelist_prev) || cur == freelist_tail){
    return 1;
  }
  return 0;
}

void removeFromFreelist(myblock *cur){
  if(cur == freelist_head){
      if(cur == freelist_tail){
        freelist_tail = NULL;
        freelist_head = NULL;
      } else {
        freelist_head = cur->freelist_next;
        if(freelist_head){
          freelist_head->freelist_prev = NULL;
        }
      }
      cur->freelist_prev = NULL;
      cur->freelist_next = NULL;
      return;
  }

  if(cur == freelist_tail){
    freelist_tail = cur->freelist_prev;
    if(freelist_tail){
      freelist_tail->freelist_next = NULL;
    }
    cur->freelist_prev = NULL;
    cur->freelist_next = NULL;
    return;
  }

  if(cur->freelist_next){
      cur->freelist_next->freelist_prev = cur->freelist_prev;
  }
  if(cur->freelist_prev){
      cur->freelist_prev->freelist_next = cur->freelist_next;
  }
  cur->freelist_prev = NULL;
  cur->freelist_next = NULL;
  return;
}

void merge(myblock* cur){
  myblock *next = cur->next;
  cur->size += next->size + sizeof(myblock);
  if(next == tail){
    tail = cur;
    cur->next = NULL;
  } else{
    next->next->prev = cur;
    cur->next = next->next;
  }
  removeFromFreelist(next);
  return;
}

void split(myblock* cur, size_t data_length){ // can do sort_add inside
  if(cur->size - data_length <= (sizeof(myblock)+8)){
      return;
  }
  myblock *newblock = (myblock*)((void*)(cur+1) + data_length);
  newblock->size = cur->size - data_length - sizeof(myblock);
  newblock->next = cur->next;
  if(cur->next){
      cur->next->prev = newblock;
  }
  newblock->prev = cur;
  cur->size = data_length;
  cur->next = newblock;
  if(cur == tail){
    tail = newblock;
  }
  if(!freelist_head && !freelist_tail){
    freelist_head = newblock;
    freelist_tail = newblock;
    newblock->freelist_prev = NULL;
    newblock->freelist_next = NULL;
  } else {
    newblock->freelist_prev = freelist_tail;
    freelist_tail->freelist_next = newblock;
    newblock->freelist_next = NULL;
    freelist_tail = newblock;
  }
   
  if(newblock->next && isInFreelist(newblock->next)){
      merge(newblock);
  }
  return;
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
   size_t request_size = num * size;
   void *result = malloc(request_size);
   if(result == NULL){
     return NULL;
   }
   memset(result, 0, request_size);
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
    size += (8 - size % 8);
    if(freelist_head){
      myblock *temp = freelist_head;
      while(temp){
        if(temp->size == size){
          removeFromFreelist(temp);
          return (void*)(temp+1);
        }
        if(temp->size > size){
          if(temp->size - size >= 2*sizeof(myblock)){
            split(temp,size);
          }
          removeFromFreelist(temp);
          return (void*)(temp+1);
        }
        temp = temp->freelist_next;
      }
    }
    size_t allocsize = 0;
    int split_flag = 1;
    if(!freelist_head){
        if(8*1024 < size){
        allocsize = size + sizeof(myblock);
        split_flag = 0;
      } else {
        allocsize = 8*1024 +sizeof(myblock);
     }
    } else{
     if(160*1024 < size){
        allocsize = size + sizeof(myblock);
        split_flag = 0;
      } else {
        allocsize = 160*1024 +sizeof(myblock);
     }
    }
    void *temp = sbrk(allocsize);
    if(temp == (void*)-1){
      return NULL;
   }
    myblock *result = (myblock*)temp;
    result->size = allocsize - sizeof(myblock);
    result->next = NULL;
    result->prev = NULL;
    if(tail == NULL){
      tail = result;
    } else {
      tail->next = result;
      result->prev = tail;
      tail = result;
    }
    result->freelist_prev = NULL;
    result->freelist_next = NULL;
    if(split_flag == 1){
      split(result,size);
    }
    return (void*)(result+1);
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
   //    printf("sizeof block is %lu\n",sizeof(myblock));
    if(ptr == NULL){
      return;
    }

    myblock *cur = ((myblock*)ptr) - 1;
   // memset(ptr,0,cur->size);
    if(!freelist_head && !freelist_tail){
      freelist_head = cur;
      freelist_tail = cur;
      cur->freelist_prev = NULL;
      cur->freelist_next = NULL;
    } else {
      cur->freelist_prev = freelist_tail;
      freelist_tail->freelist_next = cur;
      cur->freelist_next = NULL;
      freelist_tail = cur;
    }

    if(cur->next && isInFreelist(cur->next)){
      merge(cur);
    }

    if(cur->prev && isInFreelist(cur->prev)){
      merge(cur->prev);
    }
    return;
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
    if(ptr == NULL && size == 0){
      return NULL;
    }
    if(ptr == NULL){
      return malloc(size);
    }
    if(size == 0){
      free(ptr);
      return NULL;
    }

    myblock *target = (myblock*)ptr - 1;
    if(target->size == size){
      return ptr;
    } else if(target->size > size){
      split(target, size);
      return ptr;
    } else {
      void *result = malloc(size);
      if(result == NULL){
         return NULL;
      }
      memcpy(result, ptr, target->size);
      free(ptr);
      return result;
    }

    return NULL;
}
