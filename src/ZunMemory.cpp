#define ZUN_MEMORY_INTERNAL

#include "ZunMemory.hpp"
#include "GameErrorContext.hpp"
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
static size_t g_PoolSize = 80 * 1024 * 1024; // Default to 80MB Pool
static MemoryBlock *g_Head = nullptr;
static bool g_Initialized = false;
static int g_InitResult = -1;
static sys_lwmutex_t g_Mutex;
static size_t g_AllocatedBlocks = 0;
static bool g_AllocatedFromSys = false;
static bool g_InZunAllocLog = false;

void Init()
{
    if (g_Initialized)
        return;

    sys_memory_info_t mem_info;
    sys_memory_get_user_memory_size(&mem_info);
    printf("[ZunMemory] Total User Memory: %zu, Available User Memory: %zu\n", mem_info.total_user_memory, mem_info.available_user_memory);
    g_GameErrorContext.Log("[ZunMemory] Total User Memory: %zu, Available User Memory: %zu\n", mem_info.total_user_memory, mem_info.available_user_memory);

    sys_addr_t addr = 0;
    size_t trySizes[] = { 80 * 1024 * 1024, 64 * 1024 * 1024, 48 * 1024 * 1024, 32 * 1024 * 1024, 16 * 1024 * 1024 };
    size_t allocatedSize = 0;
    g_AllocatedFromSys = false;

    // Try sys_memory_allocate first with page sizes and standard attributes
    uint64_t tryPages[] = { SYS_MEMORY_PAGE_SIZE_1M, SYS_MEMORY_PAGE_SIZE_64K };
    for (size_t p = 0; p < sizeof(tryPages) / sizeof(tryPages[0]); p++)
    {
        for (size_t i = 0; i < sizeof(trySizes) / sizeof(trySizes[0]); i++)
        {
            g_InitResult = sys_memory_allocate(trySizes[i], tryPages[p], &addr);
            if (g_InitResult == 0)
            {
                allocatedSize = trySizes[i];
                g_AllocatedFromSys = true;
                break;
            }
        }
        if (g_InitResult == 0)
        {
            break;
        }
    }

    // If sys_memory_allocate failed, try allocating the pool via standard malloc (ppu heap)
    if (g_InitResult != 0)
    {
        printf("[ZunMemory] sys_memory_allocate failed (0x%X). Falling back to ppu heap allocation...\n", g_InitResult);
        g_GameErrorContext.Log("[ZunMemory] sys_memory_allocate failed (0x%X). Falling back to ppu heap allocation...\n", g_InitResult);
        
        for (size_t i = 0; i < sizeof(trySizes) / sizeof(trySizes[0]); i++)
        {
            void *p = std::malloc(trySizes[i]);
            if (p != nullptr)
            {
                addr = (sys_addr_t)p;
                allocatedSize = trySizes[i];
                g_InitResult = 0;
                break;
            }
        }
    }

    if (g_InitResult != 0)
    {
        printf("[ZunMemory] ERROR: All pool allocation methods failed!\n");
        g_GameErrorContext.Log("[ZunMemory] ERROR: All pool allocation methods failed!\n");
        return;
    }

    sys_lwmutex_attribute_t attr;
    sys_lwmutex_attribute_initialize(attr);
    g_InitResult = sys_lwmutex_create(&g_Mutex, &attr);
    if (g_InitResult != 0)
    {
        printf("[ZunMemory] ERROR: sys_lwmutex_create failed with 0x%X\n", g_InitResult);
        g_GameErrorContext.Log("[ZunMemory] ERROR: sys_lwmutex_create failed with 0x%X\n", g_InitResult);
        if (g_AllocatedFromSys)
        {
            sys_memory_free(addr);
        }
        else
        {
            std::free((void*)addr);
        }
        return;
    }

    g_PoolAddr = addr;
    g_PoolSize = allocatedSize;
    g_Head = (MemoryBlock *)g_PoolAddr;
    g_Head->size = g_PoolSize - sizeof(MemoryBlock);
    g_Head->next = nullptr;
    g_Head->prev = nullptr;
    g_Head->isFree = 1;

    g_Initialized = true;
    g_AllocatedBlocks = 0;
    
    printf("[ZunMemory] Initialized custom memory pool at %p of size %zu MB successfully (allocatedFromSys: %d)\n", 
           (void*)g_PoolAddr, g_PoolSize / (1024 * 1024), g_AllocatedFromSys ? 1 : 0);
    g_GameErrorContext.Log("[ZunMemory] Initialized custom memory pool at %p of size %zu MB successfully (allocatedFromSys: %d)\n", 
           (void*)g_PoolAddr, g_PoolSize / (1024 * 1024), g_AllocatedFromSys ? 1 : 0);
}

void *Alloc(size_t size)
{
    if (!g_Initialized)
    {
        void *res = std::malloc(size);
        return res;
    }

    sys_lwmutex_lock(&g_Mutex, SYS_NO_TIMEOUT);

    // 16-byte alignment
    size_t alignedSize = (size + 15) & ~15;

    MemoryBlock *curr = g_Head;
    while (curr)
    {
        if (curr->isFree && curr->size >= alignedSize)
        {
            // Split block if there's enough space for a new block header + at least 16 bytes
            if (curr->size >= alignedSize + sizeof(MemoryBlock) + 16)
            {
                MemoryBlock *newBlock = (MemoryBlock *)((char *)curr + sizeof(MemoryBlock) + alignedSize);
                newBlock->size = curr->size - alignedSize - sizeof(MemoryBlock);
                newBlock->next = curr->next;
                newBlock->prev = curr;
                newBlock->isFree = 1;

                if (curr->next)
                {
                    curr->next->prev = newBlock;
                }

                curr->size = alignedSize;
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
    void *res = std::malloc(size);

    if (!g_InZunAllocLog)
    {
        g_InZunAllocLog = true;
        g_GameErrorContext.Log("[ZunMemory] Alloc: pool full/exhausted, fallback malloc returned %p for size %zu\n", res, size);
        g_InZunAllocLog = false;
    }
    return res;
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
    {
        return ptr;
    }

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

void operator delete(void *ptr) throw()
{
    ZunMemory::Free(ptr);
}

void *operator new[](size_t size)
{
    return ZunMemory::Alloc(size);
}

void operator delete[](void *ptr) throw()
{
    ZunMemory::Free(ptr);
}

void operator delete(void *ptr, size_t size) throw()
{
    (void)size;
    ZunMemory::Free(ptr);
}

void operator delete[](void *ptr, size_t size) throw()
{
    (void)size;
    ZunMemory::Free(ptr);
}

#endif
