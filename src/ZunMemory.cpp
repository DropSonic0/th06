#include "ZunMemory.hpp"
#include <cstring>
#include <cstdlib>

#ifdef __PS3__
#include <sys/memory.h>
#include <sys/process.h>
#include <sys/synchronization.h>
#include <stdio.h>

namespace ZunMemory
{

struct MemoryBlock
{
    size_t size;
    MemoryBlock *next;
    MemoryBlock *prev;
    int isFree;
    int padding[4];
} __attribute__((aligned(16)));

static sys_addr_t g_PoolAddr = 0;
static const size_t g_PoolSize = 64 * 1024 * 1024; // Reverted to 64MB for safety
static MemoryBlock *g_Head = nullptr;
static bool g_Initialized = false;
static int g_InitResult = -1;
static sys_lwmutex_t g_Mutex;
static size_t g_AllocatedBlocks = 0;

void Init()
{
    if (g_Initialized)
        return;

    sys_addr_t addr;
    // Use 64KB pages and correct Read/Write protection flag
    g_InitResult = sys_memory_allocate(g_PoolSize, SYS_MEMORY_PAGE_SIZE_64K | SYS_MEMORY_PROT_READ_WRITE, &addr);
    if (g_InitResult != 0)
    {
        return;
    }

    sys_lwmutex_attribute_t attr;
    sys_lwmutex_attribute_initialize(attr);
    g_InitResult = sys_lwmutex_create(&g_Mutex, &attr);
    if (g_InitResult != 0)
    {
        sys_memory_free(addr);
        return;
    }

    g_PoolAddr = addr;
    g_Head = (MemoryBlock *)g_PoolAddr;
    g_Head->size = g_PoolSize - sizeof(MemoryBlock);
    g_Head->next = nullptr;
    g_Head->prev = nullptr;
    g_Head->isFree = 1;

    g_Initialized = true;
    g_AllocatedBlocks = 0;
}

void *Alloc(size_t size)
{
    if (!g_Initialized)
        return std::malloc(size);

    sys_lwmutex_lock(&g_Mutex, SYS_NO_TIMEOUT);

    // 16-byte alignment
    size = (size + 15) & ~15;

    MemoryBlock *curr = g_Head;
    while (curr)
    {
        if (curr->isFree && curr->size >= size)
        {
            // Split block if there's enough space for a new block header + at least 16 bytes
            if (curr->size >= size + sizeof(MemoryBlock) + 16)
            {
                MemoryBlock *newBlock = (MemoryBlock *)((char *)curr + sizeof(MemoryBlock) + size);
                newBlock->size = curr->size - size - sizeof(MemoryBlock);
                newBlock->next = curr->next;
                newBlock->prev = curr;
                newBlock->isFree = 1;

                if (curr->next)
                {
                    curr->next->prev = newBlock;
                }

                curr->size = size;
                curr->next = newBlock;
            }
            curr->isFree = 0;
            g_AllocatedBlocks++;
            void* res = (void *)((char *)curr + sizeof(MemoryBlock));
            sys_lwmutex_unlock(&g_Mutex);
            return res;
        }
        curr = curr->next;
    }

    sys_lwmutex_unlock(&g_Mutex);
    
    // Fallback to malloc if pool is full
    return std::malloc(size);
}

void Free(void *ptr)
{
    if (!ptr)
        return;

    // Check if pointer belongs to our pool
    if (!g_Initialized || (sys_addr_t)ptr < g_PoolAddr || (sys_addr_t)ptr >= g_PoolAddr + g_PoolSize)
    {
        std::free(ptr);
        return;
    }

    sys_lwmutex_lock(&g_Mutex, SYS_NO_TIMEOUT);

    MemoryBlock *block = (MemoryBlock *)((char *)ptr - sizeof(MemoryBlock));
    if (block->isFree) {
         sys_lwmutex_unlock(&g_Mutex);
         return; // Double free protection
    }
    block->isFree = 1;
    g_AllocatedBlocks--;

    // Coalesce with next
    if (block->next && block->next->isFree)
    {
        block->size += sizeof(MemoryBlock) + block->next->size;
        block->next = block->next->next;
        if (block->next)
        {
            block->next->prev = block;
        }
    }

    // Coalesce with prev
    if (block->prev && block->prev->isFree)
    {
        MemoryBlock *prev = block->prev;
        prev->size += sizeof(MemoryBlock) + block->size;
        prev->next = block->next;
        if (block->next)
        {
            block->next->prev = prev;
        }
    }

    sys_lwmutex_unlock(&g_Mutex);
}

void *Realloc(void *ptr, size_t size)
{
    if (!ptr)
        return Alloc(size);

    if (size == 0)
    {
        Free(ptr);
        return nullptr;
    }

    // Check if pointer belongs to our pool
    if (!g_Initialized || (sys_addr_t)ptr < g_PoolAddr || (sys_addr_t)ptr >= g_PoolAddr + g_PoolSize)
    {
        return std::realloc(ptr, size);
    }

    sys_lwmutex_lock(&g_Mutex, SYS_NO_TIMEOUT);
    MemoryBlock *block = (MemoryBlock *)((char *)ptr - sizeof(MemoryBlock));
    size_t oldSize = block->size;
    sys_lwmutex_unlock(&g_Mutex);

    if (size <= oldSize)
        return ptr;

    void *newPtr = Alloc(size);
    if (newPtr)
    {
        std::memcpy(newPtr, ptr, oldSize);
        Free(ptr);
    }
    return newPtr;
}

void GetPoolStats(size_t *used, size_t *total)
{
    if (!g_Initialized)
    {
        *used = (size_t)g_InitResult;
        *total = 0;
        return;
    }

    sys_lwmutex_lock(&g_Mutex, SYS_NO_TIMEOUT);

    *total = g_PoolSize;
    size_t freeSize = 0;
    MemoryBlock *curr = g_Head;
    while (curr)
    {
        if (curr->isFree)
        {
            freeSize += curr->size;
        }
        curr = curr->next;
    }
    *used = g_PoolSize - freeSize;

    sys_lwmutex_unlock(&g_Mutex);
}

size_t GetAllocatedBlockCount()
{
    return g_AllocatedBlocks;
}

} // namespace ZunMemory

// Global override of new/delete
void *operator new(size_t size)
{
    return ZunMemory::Alloc(size);
}

void operator delete(void *ptr) noexcept
{
    ZunMemory::Free(ptr);
}

void *operator new[](size_t size)
{
    return ZunMemory::Alloc(size);
}

void operator delete[](void *ptr) noexcept
{
    ZunMemory::Free(ptr);
}

void operator delete(void *ptr, size_t size) noexcept
{
    (void)size;
    ZunMemory::Free(ptr);
}

void operator delete[](void *ptr, size_t size) noexcept
{
    (void)size;
    ZunMemory::Free(ptr);
}

#endif
