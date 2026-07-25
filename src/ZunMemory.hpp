#pragma once

#include <cstdlib>

namespace ZunMemory
{
#ifdef __PS3__
void Init();
void *Alloc(size_t size);
void Free(void *ptr);
void *Realloc(void *ptr, size_t size);
void *AllocAligned(size_t size, size_t alignment);
void FreeAligned(void *ptr);
void GetPoolStats(size_t *used, size_t *total);
size_t GetAllocatedBlockCount();
#else
inline void Init() {}
inline void *Alloc(size_t size)
{
    return std::malloc(size);
}

inline void Free(void *ptr)
{
    std::free(ptr);
}

inline void *Realloc(void *ptr, size_t size)
{
    return std::realloc(ptr, size);
}
inline void *AllocAligned(size_t size, size_t alignment)
{
#if defined(_MSC_VER)
    return _aligned_malloc(size, alignment);
#else
    void *ptr = nullptr;
    size_t alignedAlignment = alignment < sizeof(void *) ? sizeof(void *) : alignment;
    if (posix_memalign(&ptr, alignedAlignment, size) != 0)
    {
        return nullptr;
    }
    return ptr;
#endif
}
inline void FreeAligned(void *ptr)
{
#if defined(_MSC_VER)
    _aligned_free(ptr);
#else
    std::free(ptr);
#endif
}
inline void GetPoolStats(size_t *used, size_t *total)
{
    *used = 0;
    *total = 0;
}

inline size_t GetAllocatedBlockCount()
{
    return 0;
}
#endif
}; // namespace ZunMemory
