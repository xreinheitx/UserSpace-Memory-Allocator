#include "runtime/Memalloc.h"


extern FirstFitHeap heap;
extern VirtualMem e_vMem;

/*
void *malloc(size_t size){
    return heap.malloc(size);
}
*/

void* operator new(size_t size) {
    //cout << "own new, size: " << size <<endl;
    return heap.malloc(size);
}

void* operator new[](size_t size) {
    //cout << "own new[], size: " << size <<endl;
    return heap.malloc(size);
}

void operator delete(void* ptr) {
    heap.free(ptr);
}

void operator delete[](void* ptr) {
    heap.free(ptr);
}


void* malloc(size_t size)
{
    return heap.malloc(size);
}

void *realloc(void* ptr, size_t size){
    return heap.realloc(ptr, size);
}

void *calloc(size_t nmemb, size_t size){
    return heap.calloc(nmemb, size);
}




