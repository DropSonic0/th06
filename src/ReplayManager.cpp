#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "Controller.hpp"
#include "FileSystem.hpp"
#include "GameManager.hpp"
#include "Gui.hpp"
#include "ReplayManager.hpp"
#include "Rng.hpp"
#include "Supervisor.hpp"
#include "utils.hpp"

ReplayManager *g_ReplayManager;

ZunResult ReplayManager::ValidateReplayData(const ReplayHeaderRaw *data, i32 fileSize)
{
    u8 *checksumCursor;
    u32 checksum;
    u8 *obfuscateCursor;
    u8 obfOffset;
    i32 idx;

    if (data == NULL)
    {
        return ZUN_ERROR;
    }

    /* "T6RP" magic bytes */
    if (*(i32 *)data->magic != *(i32 *)"T6RP")
    {
        return ZUN_ERROR;
    }

    /* Deobfuscate the replay decryptedData */
    obfuscateCursor = (u8 *)&data->rngValue3;
    obfOffset = data->key;
    for (idx = 0; idx < fileSize - (i32)offsetof(ReplayHeaderRaw, rngValue3); idx += 1, obfuscateCursor += 1)
    {
        *obfuscateCursor -= obfOffset;
        obfOffset += 7;
    }

    /* Calculate the checksum */
    /* (0x3f000318 + key + sum(c for c in decryptedData)) % (2 ** 32) */
    checksumCursor = (u8 *)&data->key;
    checksum = 0x3f000318;
    for (idx = 0; idx < fileSize - (i32)offsetof(ReplayHeaderRaw, key); idx += 1, checksumCursor += 1)
    {
        checksum += *checksumCursor;
    }

    if (checksum != (u32)data->checksum)
    {
        return ZUN_ERROR;
    }

    if (data->version != GAME_VERSION)
    {
        return ZUN_ERROR;
    }

    return ZUN_SUCCESS;
}

ZunResult ReplayManager::RegisterChain(i32 isDemo, const char *replayFile)
{
    ReplayManager *replayMgr;

    if (g_Supervisor.framerateMultiplier < 0.99f && !isDemo)
    {
        return ZUN_SUCCESS;
    }
    g_Supervisor.framerateMultiplier = 1.0f;
    if (g_ReplayManager == NULL)
    {
        replayMgr = new ReplayManager();
        g_ReplayManager = replayMgr;
        replayMgr->replayData = NULL;
        replayMgr->isDemo = isDemo;
        replayMgr->replayFile = replayFile;
        switch (isDemo)
        {
        case false:
            replayMgr->calcChain = g_Chain.CreateElem((ChainCallback)ReplayManager::OnUpdate);
            replayMgr->calcChain->addedCallback = (ChainAddedCallback)AddedCallback;
            replayMgr->calcChain->deletedCallback = (ChainDeletedCallback)DeletedCallback;
            replayMgr->drawChain = g_Chain.CreateElem((ChainCallback)ReplayManager::OnDraw);
            replayMgr->calcChain->arg = replayMgr;
            if (g_Chain.AddToCalcChain(replayMgr->calcChain, TH_CHAIN_PRIO_CALC_REPLAYMANAGER))
            {
                return ZUN_ERROR;
            }
            replayMgr->calcChainDemoHighPrio = NULL;
            break;
        case true:
            replayMgr->calcChain = g_Chain.CreateElem((ChainCallback)ReplayManager::OnUpdateDemoHighPrio);
            replayMgr->calcChain->addedCallback = (ChainAddedCallback)AddedCallbackDemo;
            replayMgr->calcChain->deletedCallback = (ChainDeletedCallback)DeletedCallback;
            replayMgr->drawChain = g_Chain.CreateElem((ChainCallback)ReplayManager::OnDraw);
            replayMgr->calcChain->arg = replayMgr;
            if (g_Chain.AddToCalcChain(replayMgr->calcChain, TH_CHAIN_PRIO_CALC_LOW_PRIO_REPLAYMANAGER_DEMO))
            {
                return ZUN_ERROR;
            }
            replayMgr->calcChainDemoHighPrio = g_Chain.CreateElem((ChainCallback)ReplayManager::OnUpdateDemoLowPrio);
            replayMgr->calcChainDemoHighPrio->arg = replayMgr;
            g_Chain.AddToCalcChain(replayMgr->calcChainDemoHighPrio, TH_CHAIN_PRIO_CALC_HIGH_PRIO_REPLAYMANAGER_DEMO);
            break;
        }
        replayMgr->drawChain->arg = replayMgr;
        g_Chain.AddToDrawChain(replayMgr->drawChain, TH_CHAIN_PRIO_DRAW_REPLAYMANAGER);
    }
    else
    {
        switch (isDemo)
        {
        case false:
            AddedCallback(g_ReplayManager);
            break;
        case true:
            return AddedCallbackDemo(g_ReplayManager);
            break;
        }
    }
    return ZUN_SUCCESS;
}

#define TH_BUTTON_REPLAY_CAPTURE                                                                                       \
    (TH_BUTTON_SHOOT | TH_BUTTON_BOMB | TH_BUTTON_FOCUS | TH_BUTTON_SKIP | TH_BUTTON_DIRECTION)

ChainCallbackResult ReplayManager::OnUpdate(ReplayManager *mgr)
{
    u16 inputs;

    if (!g_GameManager.isInMenu)
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
    inputs = IS_PRESSED(TH_BUTTON_REPLAY_CAPTURE);
    if (inputs != mgr->replayInputs->inputKey)
    {
        mgr->replayInputs += 1;
        mgr->replayInputStageBookmarks[g_GameManager.currentStage - 1] = mgr->replayInputs + 1;
        mgr->replayInputs->frameNum = mgr->frameId;
        mgr->replayInputs->inputKey = inputs;
    }
    mgr->frameId += 1;
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult ReplayManager::OnUpdateDemoLowPrio(ReplayManager *mgr)
{
    if (!g_GameManager.isInMenu)
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }
    if (g_Gui.HasCurrentMsgIdx() && g_Gui.IsDialogueSkippable() && mgr->frameId % 3 != 2)
    {
        return CHAIN_CALLBACK_RESULT_RESTART_FROM_FIRST_JOB;
    }
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult ReplayManager::OnUpdateDemoHighPrio(ReplayManager *mgr)
{
    if (!g_GameManager.isInMenu)
    {
        return CHAIN_CALLBACK_RESULT_CONTINUE;
    }

    while (mgr->frameId >= mgr->replayInputs[1].frameNum)
    {
        mgr->replayInputs += 1;
    }
    g_CurFrameInput = IS_PRESSED(0xFFFFFFFF & ~TH_BUTTON_REPLAY_CAPTURE) | mgr->replayInputs->inputKey;
    g_IsEigthFrameOfHeldInput = 0;
    if (g_LastFrameInput == g_CurFrameInput)
    {
        if (30 <= g_NumOfFramesInputsWereHeld)
        {
            if (g_NumOfFramesInputsWereHeld % 8 == 0)
            {
                g_IsEigthFrameOfHeldInput = 1;
            }
            if (38 <= g_NumOfFramesInputsWereHeld)
            {
                g_NumOfFramesInputsWereHeld = 30;
            }
        }
        g_NumOfFramesInputsWereHeld++;
    }
    else
    {
        g_NumOfFramesInputsWereHeld = 0;
    }
    mgr->frameId += 1;
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ChainCallbackResult ReplayManager::OnDraw(ReplayManager *mgr)
{
    return CHAIN_CALLBACK_RESULT_CONTINUE;
}

ZunResult ReplayManager::AddedCallback(ReplayManager *mgr)
{
    StageReplayData *stageReplayData;
    StageReplayData *oldStageReplayData;
    i32 idx;

    mgr->frameId = 0;
    if (mgr->replayData == NULL)
    {
        mgr->replayData = (ReplayData *)std::malloc(sizeof(ReplayData));
        mgr->replayData->header = (ReplayHeader *)std::malloc(sizeof(ReplayHeader));
        std::memcpy(&mgr->replayData->header->magic[0], "T6RP", 4);
        mgr->replayData->header->shottypeChara = g_GameManager.character * 2 + g_GameManager.shotType;
        mgr->replayData->header->version = 0x102;
        mgr->replayData->header->difficulty = g_GameManager.difficulty;
        std::memcpy(&mgr->replayData->header->name, "NO NAME", 4);
        for (idx = 0; idx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); idx += 1)
        {
            mgr->replayData->stageReplayData[idx] = NULL;
        }
    }
    else
    {
        oldStageReplayData = mgr->replayData->stageReplayData[g_GameManager.currentStage - 2];
        if (oldStageReplayData == NULL)
        {
            return ZUN_ERROR;
        }
        oldStageReplayData->score = g_GameManager.score;
    }
    if (mgr->replayData->stageReplayData[g_GameManager.currentStage - 1] != NULL)
    {
        utils::DebugPrint2("error : replay.cpp");
    }
    mgr->replayData->stageReplayData[g_GameManager.currentStage - 1] =
        ReplayManager::AllocateStageReplayData(sizeof(StageReplayData));
    stageReplayData = mgr->replayData->stageReplayData[g_GameManager.currentStage - 1];
    stageReplayData->bombsRemaining = g_GameManager.bombsRemaining;
    stageReplayData->livesRemaining = g_GameManager.livesRemaining;
    stageReplayData->power = g_GameManager.currentPower;
    stageReplayData->rank = g_GameManager.rank;
    stageReplayData->pointItemsCollected = g_GameManager.pointItemsCollected;
    stageReplayData->randomSeed = g_GameManager.randomSeed;
    stageReplayData->powerItemCountForScore = g_GameManager.powerItemCountForScore;
    mgr->replayInputs = stageReplayData->replayInputs;
    mgr->replayInputs->frameNum = 0;
    mgr->replayInputs->inputKey = 0;
    mgr->unk44 = 0;
    return ZUN_SUCCESS;
}

ZunResult ReplayManager::AddedCallbackDemo(ReplayManager *mgr)
{
    i32 idx;
    StageReplayData *replayData;
    ReplayHeaderRaw *rawHeader;

    mgr->frameId = 0;
    if (mgr->replayData == NULL)
    {
        mgr->replayData = (ReplayData *)std::malloc(sizeof(ReplayData));

        rawHeader = (ReplayHeaderRaw *)FileSystem::OpenPath(mgr->replayFile, g_GameManager.demoMode == 0);
        if (ValidateReplayData(rawHeader, g_LastFileSize) != ZUN_SUCCESS)
        {
            return ZUN_ERROR;
        }

        mgr->replayData->header = (ReplayHeader *)std::malloc(sizeof(ReplayHeader));
        std::memcpy(mgr->replayData->header->magic, rawHeader->magic, 4);
        mgr->replayData->header->version = rawHeader->version;
        mgr->replayData->header->shottypeChara = rawHeader->shottypeChara;
        mgr->replayData->header->difficulty = rawHeader->difficulty;
        mgr->replayData->header->checksum = rawHeader->checksum;
        mgr->replayData->header->rngValue1 = rawHeader->rngValue1;
        mgr->replayData->header->rngValue2 = rawHeader->rngValue2;
        mgr->replayData->header->key = rawHeader->key;
        mgr->replayData->header->rngValue3 = rawHeader->rngValue3;
        std::memcpy(mgr->replayData->header->date, rawHeader->date, 9);
        std::memcpy(mgr->replayData->header->name, rawHeader->name, 8);
        mgr->replayData->header->score = rawHeader->score;
        mgr->replayData->header->slowdownRate2 = rawHeader->slowdownRate2;
        mgr->replayData->header->slowdownRate = rawHeader->slowdownRate;
        mgr->replayData->header->slowdownRate3 = rawHeader->slowdownRate3;

        for (idx = 0; idx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); idx += 1)
        {
            if (rawHeader->stageReplayDataOffsets[idx] != 0)
            {
                StageReplayDataRaw *rawStageData =
                    (StageReplayDataRaw *)(((u8 *)rawHeader) + (u32)rawHeader->stageReplayDataOffsets[idx]);

                mgr->replayData->stageReplayData[idx] = ReplayManager::AllocateStageReplayData(sizeof(StageReplayData));
                mgr->replayData->stageReplayData[idx]->score = rawStageData->score;
                mgr->replayData->stageReplayData[idx]->randomSeed = rawStageData->randomSeed;
                mgr->replayData->stageReplayData[idx]->pointItemsCollected = rawStageData->pointItemsCollected;
                mgr->replayData->stageReplayData[idx]->power = rawStageData->power;
                mgr->replayData->stageReplayData[idx]->livesRemaining = rawStageData->livesRemaining;
                mgr->replayData->stageReplayData[idx]->bombsRemaining = rawStageData->bombsRemaining;
                mgr->replayData->stageReplayData[idx]->rank = rawStageData->rank;
                mgr->replayData->stageReplayData[idx]->powerItemCountForScore = rawStageData->powerItemCountForScore;
                std::memcpy(mgr->replayData->stageReplayData[idx]->padding, rawStageData->padding, 3);

                for (int inputIdx = 0; inputIdx < 53998; inputIdx++)
                {
                    mgr->replayData->stageReplayData[idx]->replayInputs[inputIdx].frameNum =
                        rawStageData->replayInputs[inputIdx].frameNum;
                    mgr->replayData->stageReplayData[idx]->replayInputs[inputIdx].inputKey =
                        rawStageData->replayInputs[inputIdx].inputKey;
                    mgr->replayData->stageReplayData[idx]->replayInputs[inputIdx].padding =
                        rawStageData->replayInputs[inputIdx].padding;
                }
            }
            else
            {
                mgr->replayData->stageReplayData[idx] = NULL;
            }
        }
        std::free(rawHeader);
    }
    if (mgr->replayData->stageReplayData[g_GameManager.currentStage - 1] == NULL)
    {
        return ZUN_ERROR;
    }
    replayData = mgr->replayData->stageReplayData[g_GameManager.currentStage - 1];
    g_GameManager.character = mgr->replayData->header->shottypeChara / 2;
    g_GameManager.shotType = mgr->replayData->header->shottypeChara % 2;
    g_GameManager.difficulty = (Difficulty)mgr->replayData->header->difficulty;
    g_GameManager.pointItemsCollected = replayData->pointItemsCollected;
    g_Rng.Initialize(replayData->randomSeed);
    g_GameManager.rank = replayData->rank;
    g_GameManager.livesRemaining = replayData->livesRemaining;
    g_GameManager.bombsRemaining = replayData->bombsRemaining;
    g_GameManager.currentPower = replayData->power;
    mgr->replayInputs = replayData->replayInputs;
    g_GameManager.powerItemCountForScore = replayData->powerItemCountForScore;
    if (2 <= g_GameManager.currentStage && mgr->replayData->stageReplayData[g_GameManager.currentStage - 2] != NULL)
    {
        g_GameManager.guiScore = mgr->replayData->stageReplayData[g_GameManager.currentStage - 2]->score;
        g_GameManager.score = g_GameManager.guiScore;
    }
    return ZUN_SUCCESS;
}

ZunResult ReplayManager::DeletedCallback(ReplayManager *mgr)
{
    i32 idx;
    g_Chain.Cut(mgr->drawChain);
    mgr->drawChain = NULL;
    if (mgr->calcChainDemoHighPrio != NULL)
    {
        g_Chain.Cut(mgr->calcChainDemoHighPrio);
        mgr->calcChainDemoHighPrio = NULL;
    }
    for (idx = 0; idx < 7; idx++)
    {
        if (g_ReplayManager->replayData->stageReplayData[idx] != NULL)
        {
            ReplayManager::ReleaseStageReplayData(g_ReplayManager->replayData->stageReplayData[idx]);
            g_ReplayManager->replayData->stageReplayData[idx] = NULL;
        }
    }
    std::free(g_ReplayManager->replayData->header);
    std::free(g_ReplayManager->replayData);
    delete g_ReplayManager;
    g_ReplayManager = NULL;
    return ZUN_SUCCESS;
}

void ReplayManager::StopRecording()
{
    ReplayManager *mgr = g_ReplayManager;
    if (mgr != NULL)
    {
        mgr->replayInputs += 1;
        mgr->replayInputs->frameNum = mgr->frameId;
        mgr->replayInputs->inputKey = 0;
        mgr->replayInputs += 1;
        mgr->replayInputs->frameNum = 9999999;
        mgr->replayInputs->inputKey = 0;
        mgr->replayInputStageBookmarks[g_GameManager.currentStage - 1] = mgr->replayInputs + 1;
    }
}

void ReplayManager::SaveReplay(const char *replayPath, char *replayName)
{
    ReplayManager *mgr;
    FILE *file;
    const u8 *checksumCursor;
    ReplayHeaderRaw replayCopyRaw;
    ReplayHeader replayCopy;
    u8 *obfuscateCursor;
    i32 obfStagePos;
    u8 obfOffset;
    u32 checksum;
    i32 csumStagePos;
    size_t stageReplayPos;
    f32 slowDown;
    i32 stageIdx;
    std::time_t time;
    const std::tm *tm;

    time = std::time(NULL);
    tm = std::localtime(&time);

    if (g_ReplayManager != NULL)
    {
        mgr = g_ReplayManager;
        if (!mgr->IsDemo())
        {
            if (replayPath != NULL)
            {
                replayCopy = *mgr->replayData->header;
                ReplayManager::StopRecording();
                stageReplayPos = sizeof(ReplayHeaderRaw);
                for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(g_ReplayManager->replayData->stageReplayData);
                     stageIdx += 1)
                {
                    if (mgr->replayData->stageReplayData[stageIdx] != NULL)
                    {
                        replayCopy.stageReplayDataOffsets[stageIdx] = (u32)stageReplayPos;
                        stageReplayPos += (size_t)((u8 *)mgr->replayInputStageBookmarks[stageIdx] -
                                                   (u8 *)mgr->replayData->stageReplayData[stageIdx]);
                    }
                }
                utils::DebugPrint2("%s write ...\n", replayPath);
                replayCopy.score = g_GameManager.guiScore;
                slowDown = (g_Supervisor.unk1b4 / g_Supervisor.unk1b8 - 0.5f) * 2.0f;
                if (slowDown < 0.0f)
                {
                    slowDown = 0.0f;
                }
                else if (slowDown >= 1.0f)
                {
                    slowDown = 1.0f;
                }
                replayCopy.slowdownRate = (1.0f - slowDown) * 100.0f;
                replayCopy.slowdownRate2 = replayCopy.slowdownRate + 1.12f;
                replayCopy.slowdownRate3 = replayCopy.slowdownRate + 2.34f;
                mgr->replayData->stageReplayData[g_GameManager.currentStage - 1]->score = g_GameManager.score;

                std::snprintf(replayCopy.name, sizeof(replayCopy.name), "%s", replayName);
                std::sprintf(replayCopy.date, "%02i/%02i/%02i", tm->tm_mon, tm->tm_mday, tm->tm_year % 100);
                replayCopy.key = g_Rng.GetRandomU16InRange(128) + 64;
                replayCopy.rngValue3 = g_Rng.GetRandomU16InRange(256);
                replayCopy.rngValue1 = g_Rng.GetRandomU16InRange(256);
                replayCopy.rngValue2 = g_Rng.GetRandomU16InRange(256);

                // Calculate the checksum.
                checksumCursor = (u8 *)&replayCopy.key;
                checksum = 0x3f000318;
                for (stageIdx = 0; stageIdx < sizeof(ReplayHeader) - offsetof(ReplayHeader, key);
                     stageIdx += 1, checksumCursor += 1)
                {
                    checksum += *checksumCursor;
                }
                for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); stageIdx += 1)
                {
                    if (mgr->replayData->stageReplayData[stageIdx] != NULL)
                    {
                        checksumCursor = (u8 *)mgr->replayData->stageReplayData[stageIdx];
                        for (csumStagePos = 0; csumStagePos < ((iptr)mgr->replayInputStageBookmarks[stageIdx]) -
                                                                  ((iptr)mgr->replayData->stageReplayData[stageIdx]);
                             csumStagePos += 1, checksumCursor += 1)
                        {
                            checksum += *checksumCursor;
                        }
                    }
                }
                replayCopy.checksum = checksum;

                // Prepare raw data for writing.
                std::memcpy(replayCopyRaw.magic, replayCopy.magic, 4);
                replayCopyRaw.version = replayCopy.version;
                replayCopyRaw.shottypeChara = replayCopy.shottypeChara;
                replayCopyRaw.difficulty = replayCopy.difficulty;
                replayCopyRaw.checksum = replayCopy.checksum;
                replayCopyRaw.rngValue1 = replayCopy.rngValue1;
                replayCopyRaw.rngValue2 = replayCopy.rngValue2;
                replayCopyRaw.key = replayCopy.key;
                replayCopyRaw.rngValue3 = replayCopy.rngValue3;
                std::memcpy(replayCopyRaw.date, replayCopy.date, 9);
                std::memcpy(replayCopyRaw.name, replayCopy.name, 8);
                replayCopyRaw.score = replayCopy.score;
                replayCopyRaw.slowdownRate2 = replayCopy.slowdownRate2;
                replayCopyRaw.slowdownRate = replayCopy.slowdownRate;
                replayCopyRaw.slowdownRate3 = replayCopy.slowdownRate3;
                for (int i = 0; i < 7; i++)
                {
                    replayCopyRaw.stageReplayDataOffsets[i] = replayCopy.stageReplayDataOffsets[i];
                }

                // Obfuscate the data.
                obfuscateCursor = (u8 *)&replayCopyRaw.rngValue3;
                obfOffset = replayCopyRaw.key;
                for (stageIdx = 0; stageIdx < sizeof(ReplayHeaderRaw) - offsetof(ReplayHeaderRaw, rngValue3);
                     stageIdx += 1, obfuscateCursor += 1)
                {
                    *obfuscateCursor += obfOffset;
                    obfOffset += 7;
                }

                // Write the data to the replay file.
                file = FileSystem::FopenUTF8(replayPath, "wb");
                std::fwrite(&replayCopyRaw, sizeof(ReplayHeaderRaw), 1, file);
                for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); stageIdx += 1)
                {
                    if (mgr->replayData->stageReplayData[stageIdx] != NULL)
                    {
                        StageReplayData *stageData = mgr->replayData->stageReplayData[stageIdx];
                        StageReplayDataRaw *stageDataRaw =
                            (StageReplayDataRaw *)std::malloc(sizeof(StageReplayDataRaw));

                        stageDataRaw->score = stageData->score;
                        stageDataRaw->randomSeed = stageData->randomSeed;
                        stageDataRaw->pointItemsCollected = stageData->pointItemsCollected;
                        stageDataRaw->power = stageData->power;
                        stageDataRaw->livesRemaining = stageData->livesRemaining;
                        stageDataRaw->bombsRemaining = stageData->bombsRemaining;
                        stageDataRaw->rank = stageData->rank;
                        stageDataRaw->powerItemCountForScore = stageData->powerItemCountForScore;
                        std::memcpy(stageDataRaw->padding, stageData->padding, 3);

                        i32 inputCount = ((iptr)mgr->replayInputStageBookmarks[stageIdx]) -
                                         ((iptr)mgr->replayData->stageReplayData[stageIdx]);

                        // inputCount is in bytes, convert to number of inputs
                        inputCount -= offsetof(StageReplayData, replayInputs);
                        inputCount /= sizeof(ReplayDataInput);

                        for (int inputIdx = 0; inputIdx < inputCount; inputIdx++)
                        {
                            stageDataRaw->replayInputs[inputIdx].frameNum = stageData->replayInputs[inputIdx].frameNum;
                            stageDataRaw->replayInputs[inputIdx].inputKey = stageData->replayInputs[inputIdx].inputKey;
                            stageDataRaw->replayInputs[inputIdx].padding = stageData->replayInputs[inputIdx].padding;
                        }

                        // Obfuscate stage data
                        obfuscateCursor = (u8 *)stageDataRaw;
                        for (obfStagePos = 0;
                             obfStagePos < (iptr)offsetof(StageReplayDataRaw, replayInputs) +
                                               inputCount * (iptr)sizeof(ReplayDataInputRaw);
                             obfStagePos += 1, obfuscateCursor += 1)
                        {
                            *obfuscateCursor += obfOffset;
                            obfOffset += 7;
                        }

                        std::fwrite(stageDataRaw, 1,
                                    offsetof(StageReplayDataRaw, replayInputs) +
                                        inputCount * sizeof(ReplayDataInputRaw),
                                    file);
                        std::free(stageDataRaw);
                    }
                }
                std::fclose(file);
            }
            for (stageIdx = 0; stageIdx < ARRAY_SIZE_SIGNED(mgr->replayData->stageReplayData); stageIdx += 1)
            {
                if (g_ReplayManager->replayData->stageReplayData[stageIdx] != NULL)
                {
                    utils::DebugPrint2("Replay Size %d\n", ((iptr)mgr->replayInputStageBookmarks[stageIdx]) -
                                                               ((iptr)mgr->replayData->stageReplayData[stageIdx]));
                    ReplayManager::ReleaseStageReplayData(g_ReplayManager->replayData->stageReplayData[stageIdx]);
                }
            }
        }
        g_Chain.Cut(g_ReplayManager->calcChain);
    }
    return;
}
