#pragma once

#include <cstdlib>

namespace ZunMemory
{
#ifdef __PS3__
void Init();
void *Alloc(size_t size);
void Free(void *ptr);
void *Realloc(void *ptr, size_t size);
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
