#ifndef STACK_ALLOCATOR
#define STACK_ALLOCATOR

#ifndef ALLOCATOR_SET
#define ALLOCATOR_SET 0
#endif

layout(set = ALLOCATOR_SET, binding = 0) buffer uuStack { uint uStack[]; };
layout(std140, set = ALLOCATOR_SET, binding = 1) volatile buffer uuStackPtr { uint uStackPtr; };
layout(std140, set = ALLOCATOR_SET, binding = 2) buffer uuAllocCounter { uint uAllocCounter; };

// Allocates 8 uints at once
uint MyAlloc() {
    if (uStackPtr > 0) {
        uint ptr = atomicAdd(uStackPtr, -1);
        return uStack[ptr];
    } else {
        uint addr = atomicAdd(uAllocCounter, 8);
        return addr;
    }
}

void Dealloc(uint addr) {
    uint ptr = atomicAdd(uStackPtr, 1);
    uStack[ptr - 1] = addr;
}

#endif // STACK_ALLOCATOR
