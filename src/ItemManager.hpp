#pragma once

#include "AnmVm.hpp"
#include "ZunTimer.hpp"
#include "inttypes.hpp"

// #include <d3dx8math.h>

#ifdef __PS3__
typedef i32 ItemType;
#define ITEM_POWER_SMALL ((ItemType)0)
#define ITEM_POINT ((ItemType)1)
#define ITEM_POWER_BIG ((ItemType)2)
#define ITEM_BOMB ((ItemType)3)
#define ITEM_FULL_POWER ((ItemType)4)
#define ITEM_LIFE ((ItemType)5)
#define ITEM_POINT_BULLET ((ItemType)6)
#define ITEM_NO_ITEM ((ItemType)0xffffffff)
#else
enum ItemType // This enum is 1 byte in size on Enemy
{
    ITEM_POWER_SMALL,
    ITEM_POINT,
    ITEM_POWER_BIG,
    ITEM_BOMB,
    ITEM_FULL_POWER,
    ITEM_LIFE,
    ITEM_POINT_BULLET,
    ITEM_NO_ITEM = 0xffffffff,
};
#endif

struct Item
{
    AnmVm sprite;
    ZunVec3 currentPosition;
    ZunVec3 startPosition;
    ZunVec3 targetPosition;
    ZunTimer timer;
    i8 itemType;
    i8 isInUse;
    i8 unk_142;
    i8 state;
};

struct ItemManager
{
    ItemManager();
    void SpawnItem(const ZunVec3 *position, ItemType type, i32 state);
    void OnUpdate();
    void OnDraw();
    void RemoveAllItems();

    Item items[512];
    Item dummy_item;
    i32 nextIndex;
    u32 itemCount;
};

extern ItemManager g_ItemManager;
