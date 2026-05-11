#pragma once

#include "ZunEndian.hpp"
#include "inttypes.hpp"

struct ReplayDataInput
{
    i32 frameNum;
    u16 inputKey;
    u16 padding;
};

struct ReplayDataInputRaw
{
    LE<i32> frameNum;
    LE<u16> inputKey;
    LE<u16> padding;
};

struct StageReplayData
{
    i32 score;
    i16 randomSeed;
    i16 pointItemsCollected;
    u8 power;
    i8 livesRemaining;
    i8 bombsRemaining;
    u8 rank;
    i8 powerItemCountForScore;
    i8 padding[3];
    ReplayDataInput replayInputs[53998];
};

struct StageReplayDataRaw
{
    LE<i32> score;
    LE<i16> randomSeed;
    LE<i16> pointItemsCollected;
    u8 power;
    i8 livesRemaining;
    i8 bombsRemaining;
    u8 rank;
    i8 powerItemCountForScore;
    i8 padding[3];
    ReplayDataInputRaw replayInputs[53998];
};

struct ReplayHeader
{
    char magic[4];
    u16 version;
    u8 shottypeChara;
    u8 difficulty;
    i32 checksum;
    u8 rngValue1;
    u8 rngValue2;
    i8 key;
    i8 rngValue3;
    char date[9];
    char name[8];
    i32 score;
    f32 slowdownRate2;
    f32 slowdownRate;
    f32 slowdownRate3;
    u32 stageReplayDataOffsets[7];
};

struct ReplayHeaderRaw
{
    char magic[4];
    LE<u16> version;
    u8 shottypeChara;
    u8 difficulty;
    LE<i32> checksum;
    u8 rngValue1;
    u8 rngValue2;
    i8 key;
    i8 rngValue3;
    char date[9];
    char name[8];
    LE<i32> score;
    LE<f32> slowdownRate2;
    LE<f32> slowdownRate;
    LE<f32> slowdownRate3;
    LE<u32> stageReplayDataOffsets[7];
};

struct ReplayData
{
    ReplayHeader *header;
    StageReplayData *stageReplayData[7];
};
