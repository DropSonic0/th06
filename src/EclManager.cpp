#include "EclManager.hpp"
#include "AnmManager.hpp"
#include "EffectManager.hpp"
#include "Enemy.hpp"
#include "EnemyEclInstr.hpp"
#include "EnemyManager.hpp"
#include "FileSystem.hpp"
#include "GameErrorContext.hpp"
#include "GameManager.hpp"
#include "Gui.hpp"
#include "Player.hpp"
#include "Rng.hpp"
#include "Stage.hpp"
#include "utils.hpp"

#ifdef __PS3__
#include <cstdio>
#endif

static const i32 g_SpellcardScore[64] = {
    200000, 200000, 200000, 200000, 200000, 200000, 200000, 250000, 250000, 250000, 250000, 250000, 250000,
    250000, 300000, 300000, 300000, 300000, 300000, 300000, 300000, 300000, 300000, 300000, 300000, 300000,
    300000, 300000, 300000, 300000, 300000, 300000, 400000, 400000, 400000, 400000, 400000, 400000, 400000,
    400000, 500000, 500000, 500000, 500000, 500000, 500000, 600000, 600000, 600000, 600000, 600000, 700000,
    700000, 700000, 700000, 700000, 700000, 700000, 700000, 700000, 700000, 700000, 700000, 700000};
EclManager g_EclManager;
typedef void (*ExInsn)(Enemy *, EclRawInstr *);
static const ExInsn g_EclExInsn[17] = {
    EnemyEclInstr::ExInsCirnoRainbowBallJank, EnemyEclInstr::ExInsShootAtRandomArea,
    EnemyEclInstr::ExInsShootStarPattern,     EnemyEclInstr::ExInsPatchouliShottypeSetVars,
    EnemyEclInstr::ExInsStage56Func4,         EnemyEclInstr::ExInsStage5Func5,
    EnemyEclInstr::ExInsStage6XFunc6,         EnemyEclInstr::ExInsStage6Func7,
    EnemyEclInstr::ExInsStage6Func8,          EnemyEclInstr::ExInsStage6Func9,
    EnemyEclInstr::ExInsStage6XFunc10,        EnemyEclInstr::ExInsStage6Func11,
    EnemyEclInstr::ExInsStage4Func12,         EnemyEclInstr::ExInsStageXFunc13,
    EnemyEclInstr::ExInsStageXFunc14,         EnemyEclInstr::ExInsStageXFunc15,
    EnemyEclInstr::ExInsStageXFunc16};

ZunResult EclManager::Load(const char *eclPath)
{
    i32 idx;

    this->eclFile = (EclRawHeader *)FileSystem::OpenPath(eclPath, false);

    if (this->eclFile == NULL)
    {
        GameErrorContext::Log(&g_GameErrorContext, TH_ERR_ECLMANAGER_ENEMY_DATA_CORRUPT);
        return ZUN_ERROR;
    }

    this->timelinePtrs[0] = (EclTimelineInstr *)(((u8 *)this->eclFile) + (u32)this->eclFile->timelineOffsets[0]);

    this->subTable = (EclRawInstr **)malloc(sizeof(EclRawInstr *) * this->eclFile->subCount);

    for (idx = 0; idx < (i32)this->eclFile->subCount; idx++)
    {
        this->subTable[idx] = (EclRawInstr *)(((u8 *)this->eclFile) + (u32)this->eclFile->subOffsets[idx]);
    }

    this->timeline = this->timelinePtrs[0];
    return ZUN_SUCCESS;
}

void EclManager::Unload()
{
    free((void *)this->eclFile);
    this->eclFile = NULL;

    free(this->subTable);
    this->subTable = NULL;

    return;
}

ZunResult EclManager::CallEclSub(EnemyEclContext *ctx, i16 subId) const
{
    ctx->currentInstr = this->subTable[subId];
    ctx->time.InitializeForPopup();
    ctx->subId = subId;
    return ZUN_SUCCESS;
}

ZunResult EclManager::RunEcl(Enemy *enemy)
{
    EclRawInstr *instruction;
    EclRawInstrArgs *args;
    ZunVec3 local_8;
    i32 local_14, local_24, local_28, local_2c, *local_3c, *local_40, local_44, local_48, local_68, local_74, csum,
        scoreIncrease, local_84, local_88, local_8c, local_b8, local_c0;
    f32 local_18, local_30, local_34, local_38, local_4c, local_50, local_bc;
    Catk *local_70, *local_80;
    EclRawInstrBulletArgs *local_54;
    EnemyBulletShooter *local_58;
    const EclRawInstrAnmSetDeathArgs *local_5c;
    EnemyLaserShooter *local_60;
    EclRawInstrLaserArgs *local_64;
    const EclRawInstrSpellcardEffectArgs *local_6c;
    ZunVec3 local_98;
    EclRawInstrEnemyCreateArgs local_b0;
    Enemy *local_b4;
    EclVarId tmpVarId1;
    EclVarId tmpVarId2;
    i32 tmpi32;
    ZunVec3 tmpVec3;

    for (;;)
    {
        instruction = (EclRawInstr *)enemy->currentContext.currentInstr;
        if (0 <= enemy->runInterrupt)
        {
            goto HANDLE_INTERRUPT;
        }

    YOLO:
        if (enemy->currentContext.time.current == (i32)instruction->time)
        {
            if (!(instruction->skipForDifficulty & (1 << g_GameManager.difficulty)))
            {
                goto NEXT_INSN;
            }

            args = &instruction->args;
#ifdef __PS3__
            // printf("Ecl OPCODE : %d\n", (i16)instruction->opCode);
#endif
            switch ((i16)instruction->opCode)
            {
            case ECL_OPCODE_UNIMP:
                return ZUN_ERROR;
            case ECL_OPCODE_JUMPDEC:
                local_14 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->jump.var, NULL);
                local_14--;
                EnemyEclInstr::SetVar(enemy, (EclVarId)args->jump.var, &local_14);
                if (local_14 <= 0)
                    break;
            case ECL_OPCODE_JUMP:
            HANDLE_JUMP:
                enemy->currentContext.time.current = (i32)instruction->args.jump.time;
                instruction = (EclRawInstr *)(((u8 *)instruction) + (i32)args->jump.offset);
                goto YOLO;
            case ECL_OPCODE_SETINT:
            case ECL_OPCODE_SETFLOAT:
                tmpi32 = (i32)args->alu.arg1.i32Param;
                EnemyEclInstr::SetVar(enemy, (EclVarId)instruction->args.alu.res, &tmpi32);
                break;
            case ECL_OPCODE_MATHNORMANGLE:
                tmpVarId1 = (EclVarId)instruction->args.alu.res;
                local_18 = *(f32 *)EnemyEclInstr::GetVar(enemy, &tmpVarId1, NULL);
                local_18 = utils::AddNormalizeAngle(local_18, 0.0f);
                EnemyEclInstr::SetVar(enemy, (EclVarId)instruction->args.alu.res, &local_18);
                break;
            case ECL_OPCODE_SETINTRAND:
                local_24 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->alu.arg1.id, NULL);
                local_14 = g_Rng.GetRandomU32InRange(local_24);
                EnemyEclInstr::SetVar(enemy, (EclVarId)instruction->args.alu.res, &local_14);
                break;
            case ECL_OPCODE_SETINTRANDMIN:
                local_28 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->alu.arg1.id, NULL);
                local_2c = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->alu.arg2.id, NULL);
                local_14 = g_Rng.GetRandomU32InRange(local_28);
                local_14 += local_2c;
                EnemyEclInstr::SetVar(enemy, (EclVarId)instruction->args.alu.res, &local_14);
                break;
            case ECL_OPCODE_SETFLOATRAND:
                local_30 = EnemyEclInstr::GetVarFloatValue(enemy, (f32)args->alu.arg1.f32Param, NULL);
                local_18 = g_Rng.GetRandomF32InRange(local_30);
                EnemyEclInstr::SetVar(enemy, (EclVarId)instruction->args.alu.res, &local_18);
                break;
            case ECL_OPCODE_SETFLOATRANDMIN:
                local_34 = EnemyEclInstr::GetVarFloatValue(enemy, (f32)args->alu.arg1.f32Param, NULL);
                local_38 = EnemyEclInstr::GetVarFloatValue(enemy, (f32)args->alu.arg2.f32Param, NULL);
                local_18 = g_Rng.GetRandomF32InRange(local_34);
                local_18 += local_38;
                EnemyEclInstr::SetVar(enemy, (EclVarId)instruction->args.alu.res, &local_18);
                break;
            case ECL_OPCODE_SETVARSELFX:
                EnemyEclInstr::SetVar(enemy, (EclVarId)instruction->args.alu.res, &enemy->position.x);
                break;
            case ECL_OPCODE_SETVARSELFY:
                EnemyEclInstr::SetVar(enemy, (EclVarId)instruction->args.alu.res, &enemy->position.y);
                break;
            case ECL_OPCODE_SETVARSELFZ:
                EnemyEclInstr::SetVar(enemy, (EclVarId)instruction->args.alu.res, &enemy->position.z);
                break;
            case ECL_OPCODE_MATHINTADD:
            case ECL_OPCODE_MATHFLOATADD:
                tmpVarId1 = (EclVarId)args->alu.arg1.id;
                tmpVarId2 = (EclVarId)args->alu.arg2.id;
                EnemyEclInstr::MathAdd(enemy, (EclVarId)instruction->args.alu.res, &tmpVarId1, &tmpVarId2);
                break;
            case ECL_OPCODE_MATHINC:
                tmpVarId1 = (EclVarId)instruction->args.alu.res;
                local_3c = EnemyEclInstr::GetVar(enemy, &tmpVarId1, NULL);
                *local_3c += 1;
                break;
            case ECL_OPCODE_MATHDEC:
                tmpVarId1 = (EclVarId)instruction->args.alu.res;
                local_40 = EnemyEclInstr::GetVar(enemy, &tmpVarId1, NULL);
                *local_40 -= 1;
                break;
            case ECL_OPCODE_MATHINTSUB:
            case ECL_OPCODE_MATHFLOATSUB:
                tmpVarId1 = (EclVarId)args->alu.arg1.id;
                tmpVarId2 = (EclVarId)args->alu.arg2.id;
                EnemyEclInstr::MathSub(enemy, (EclVarId)instruction->args.alu.res, &tmpVarId1, &tmpVarId2);
                break;
            case ECL_OPCODE_MATHINTMUL:
            case ECL_OPCODE_MATHFLOATMUL:
                tmpVarId1 = (EclVarId)args->alu.arg1.id;
                tmpVarId2 = (EclVarId)args->alu.arg2.id;
                EnemyEclInstr::MathMul(enemy, (EclVarId)instruction->args.alu.res, &tmpVarId1, &tmpVarId2);
                break;
            case ECL_OPCODE_MATHINTDIV:
            case ECL_OPCODE_MATHFLOATDIV:
                tmpVarId1 = (EclVarId)args->alu.arg1.id;
                tmpVarId2 = (EclVarId)args->alu.arg2.id;
                EnemyEclInstr::MathDiv(enemy, (EclVarId)instruction->args.alu.res, &tmpVarId1, &tmpVarId2);
                break;
            case ECL_OPCODE_MATHINTMOD:
            case ECL_OPCODE_MATHFLOATMOD:
                tmpVarId1 = (EclVarId)args->alu.arg1.id;
                tmpVarId2 = (EclVarId)args->alu.arg2.id;
                EnemyEclInstr::MathMod(enemy, (EclVarId)instruction->args.alu.res, &tmpVarId1, &tmpVarId2);
                break;
            case ECL_OPCODE_MATHATAN2:
                EnemyEclInstr::MathAtan2(enemy, (EclVarId)instruction->args.alu.res, (f32)args->alu.arg1.f32Param,
                                         (f32)args->alu.arg2.f32Param, (f32)args->alu.arg3.f32Param, (f32)args->alu.arg4.f32Param);
                break;
            case ECL_OPCODE_CMPINT:
                local_48 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)instruction->args.cmp.lhs.id, NULL);
                local_44 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)instruction->args.cmp.rhs.id, NULL);
                enemy->currentContext.compareRegister = local_48 == local_44 ? 0 : local_48 < local_44 ? -1 : 1;
                break;
            case ECL_OPCODE_CMPFLOAT:
                local_4c = EnemyEclInstr::GetVarFloatValue(enemy, (f32)instruction->args.cmp.lhs.f32Param, NULL);
                local_50 = EnemyEclInstr::GetVarFloatValue(enemy, (f32)instruction->args.cmp.rhs.f32Param, NULL);
                enemy->currentContext.compareRegister = local_4c == local_50 ? 0 : (local_4c < local_50 ? -1 : 1);
                break;
            case ECL_OPCODE_JUMPLSS:
                if (enemy->currentContext.compareRegister < 0)
                    goto HANDLE_JUMP;
                break;
            case ECL_OPCODE_JUMPLEQ:
                if (enemy->currentContext.compareRegister <= 0)
                    goto HANDLE_JUMP;
                break;
            case ECL_OPCODE_JUMPEQU:
                if (enemy->currentContext.compareRegister == 0)
                    goto HANDLE_JUMP;
                break;
            case ECL_OPCODE_JUMPGRE:
                if (enemy->currentContext.compareRegister > 0)
                    goto HANDLE_JUMP;
                break;
            case ECL_OPCODE_JUMPGEQ:
                if (enemy->currentContext.compareRegister >= 0)
                    goto HANDLE_JUMP;
                break;
            case ECL_OPCODE_JUMPNEQ:
                if (enemy->currentContext.compareRegister != 0)
                    goto HANDLE_JUMP;
                break;
            case ECL_OPCODE_CALL:
            HANDLE_CALL:
                local_14 = (i32)instruction->args.call.eclSub;
                enemy->currentContext.currentInstr = (EclRawInstr *)((u8 *)instruction + (i16)instruction->offsetToNext);
                if (enemy->flags.unk14 == 0)
                {
                    memcpy(&enemy->savedContextStack[enemy->stackDepth], &enemy->currentContext,
                           sizeof(EnemyEclContext));
                }
                g_EclManager.CallEclSub(&enemy->currentContext, (u16)local_14);
                if (enemy->flags.unk14 == 0 && enemy->stackDepth < 7)
                {
                    enemy->stackDepth++;
                }
                enemy->currentContext.var0 = (i32)instruction->args.call.var0;
                enemy->currentContext.float0 = (f32)instruction->args.call.float0;
                continue;
            case ECL_OPCODE_RET:
                if (enemy->flags.unk14)
                {
                    utils::DebugPrint2("error : no Stack Ret\n");
                }
                enemy->stackDepth--;
                memcpy(&enemy->currentContext, &enemy->savedContextStack[enemy->stackDepth], sizeof(EnemyEclContext));
                continue;
            case ECL_OPCODE_CALLLSS:
                local_14 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->call.cmpLhs, NULL);
                if (local_14 < (i32)args->call.cmpRhs)
                    goto HANDLE_CALL;
                break;
            case ECL_OPCODE_CALLLEQ:
                local_14 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->call.cmpLhs, NULL);
                if (local_14 <= (i32)args->call.cmpRhs)
                    goto HANDLE_CALL;
                break;
            case ECL_OPCODE_CALLEQU:
                local_14 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->call.cmpLhs, NULL);
                if (local_14 == (i32)args->call.cmpRhs)
                    goto HANDLE_CALL;
                break;
            case ECL_OPCODE_CALLGRE:
                local_14 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->call.cmpLhs, NULL);
                if (local_14 > (i32)args->call.cmpRhs)
                    goto HANDLE_CALL;
                break;
            case ECL_OPCODE_CALLGEQ:
                local_14 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->call.cmpLhs, NULL);
                if (local_14 >= (i32)args->call.cmpRhs)
                    goto HANDLE_CALL;
                break;
            case ECL_OPCODE_CALLNEQ:
                local_14 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->call.cmpLhs, NULL);
                if (local_14 != (i32)args->call.cmpRhs)
                    goto HANDLE_CALL;
                break;
            case ECL_OPCODE_ANMSETMAIN:
                g_AnmManager->SetAndExecuteScriptIdx(&enemy->primaryVm,
                                                     (i32)instruction->args.anmSetMain.scriptIdx + ANM_SCRIPT_ENEMY_START);
                break;
            case ECL_OPCODE_ANMSETSLOT:
                if (ARRAY_SIZE_SIGNED(enemy->vms) <= (i32)instruction->args.anmSetSlot.vmIdx)
                {
                    utils::DebugPrint2("error : sub anim overflow\n");
                }
                g_AnmManager->SetAndExecuteScriptIdx(&enemy->vms[(i32)instruction->args.anmSetSlot.vmIdx],
                                                     (i32)args->anmSetSlot.scriptIdx + ANM_SCRIPT_ENEMY_START);
                break;
            case ECL_OPCODE_MOVEPOSITION:
                enemy->position = instruction->args.move.pos;
                enemy->position.x = EnemyEclInstr::GetVarFloatValue(enemy, enemy->position.x, NULL);
                enemy->position.y = EnemyEclInstr::GetVarFloatValue(enemy, enemy->position.y, NULL);
                enemy->position.z = EnemyEclInstr::GetVarFloatValue(enemy, enemy->position.z, NULL);
                enemy->ClampPos();
                break;
            case ECL_OPCODE_MOVEAXISVELOCITY:
                enemy->axisSpeed = instruction->args.move.pos;
                enemy->axisSpeed.x = EnemyEclInstr::GetVarFloatValue(enemy, enemy->axisSpeed.x, NULL);
                enemy->axisSpeed.y = EnemyEclInstr::GetVarFloatValue(enemy, enemy->axisSpeed.y, NULL);
                enemy->axisSpeed.z = EnemyEclInstr::GetVarFloatValue(enemy, enemy->axisSpeed.z, NULL);
                enemy->flags.unk1 = 0;
                break;
            case ECL_OPCODE_MOVEVELOCITY:
                local_8 = instruction->args.move.pos;
                enemy->angle = EnemyEclInstr::GetVarFloatValue(enemy, local_8.x, NULL);
                enemy->speed = EnemyEclInstr::GetVarFloatValue(enemy, local_8.y, NULL);
                enemy->flags.unk1 = 1;
                break;
            case ECL_OPCODE_MOVEANGULARVELOCITY:
                local_8 = instruction->args.move.pos;
                enemy->angularVelocity = EnemyEclInstr::GetVarFloatValue(enemy, local_8.x, NULL);
                enemy->flags.unk1 = 1;
                break;
            case ECL_OPCODE_MOVEATPLAYER:
                local_8 = instruction->args.move.pos;
                enemy->angle = g_Player.AngleToPlayer(&enemy->position) + local_8.x;
                enemy->speed = EnemyEclInstr::GetVarFloatValue(enemy, local_8.y, NULL);
                enemy->flags.unk1 = 1;
                break;
            case ECL_OPCODE_MOVESPEED:
                local_8 = instruction->args.move.pos;
                enemy->speed = EnemyEclInstr::GetVarFloatValue(enemy, local_8.x, NULL);
                enemy->flags.unk1 = 1;
                break;
            case ECL_OPCODE_MOVEACCELERATION:
                local_8 = instruction->args.move.pos;
                enemy->acceleration = EnemyEclInstr::GetVarFloatValue(enemy, local_8.x, NULL);
                enemy->flags.unk1 = 1;
                break;
            case ECL_OPCODE_BULLETFANAIMED:
            case ECL_OPCODE_BULLETFAN:
            case ECL_OPCODE_BULLETCIRCLEAIMED:
            case ECL_OPCODE_BULLETCIRCLE:
            case ECL_OPCODE_BULLETOFFSETCIRCLEAIMED:
            case ECL_OPCODE_BULLETOFFSETCIRCLE:
            case ECL_OPCODE_BULLETRANDOMANGLE:
            case ECL_OPCODE_BULLETRANDOMSPEED:
            case ECL_OPCODE_BULLETRANDOM:
                local_54 = &instruction->args.bullet;
                local_58 = &enemy->bulletProps;
                local_58->sprite = (i16)local_54->sprite;
                local_58->aimMode = (i16)instruction->opCode - ECL_OPCODE_BULLETFANAIMED;
                local_58->count1 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)local_54->count1, NULL);
                local_58->count1 += enemy->BulletRankAmount1(g_GameManager.rank);
                if (local_58->count1 <= 0)
                {
                    local_58->count1 = 1;
                }

                local_58->count2 = EnemyEclInstr::GetVarValue(enemy, (EclVarId)local_54->count2, NULL);
                local_58->count2 += enemy->BulletRankAmount2(g_GameManager.rank);
                if (local_58->count2 <= 0)
                {
                    local_58->count2 = 1;
                }
                local_58->position = enemy->position + enemy->shootOffset;
                local_58->angle1 = EnemyEclInstr::GetVarFloatValue(enemy, (f32)local_54->angle1, NULL);
                local_58->angle1 = utils::AddNormalizeAngle(local_58->angle1, 0.0f);
                local_58->speed1 = EnemyEclInstr::GetVarFloatValue(enemy, (f32)local_54->speed1, NULL);
                if (local_58->speed1 != 0.0f)
                {
                    local_58->speed1 += enemy->BulletRankSpeed(g_GameManager.rank);
                    if (local_58->speed1 < 0.3f)
                    {
                        local_58->speed1 = 0.3;
                    }
                }
                local_58->angle2 = EnemyEclInstr::GetVarFloatValue(enemy, (f32)local_54->angle2, NULL);
                local_58->speed2 = EnemyEclInstr::GetVarFloatValue(enemy, (f32)local_54->speed2, NULL);
                local_58->speed2 += enemy->BulletRankSpeed(g_GameManager.rank) / 2.0f;
                if (local_58->speed2 < 0.3f)
                {
                    local_58->speed2 = 0.3f;
                }
                local_58->unk_4a = 0;
                local_58->flags = (i32)local_54->flags;
                local_14 = (i16)local_54->color;
                // TODO: Strict aliasing rule be like.
                local_58->spriteOffset = *EnemyEclInstr::GetVar(enemy, (EclVarId *)&local_14, NULL);
                if (enemy->flags.unk3 == 0)
                {
                    g_BulletManager.SpawnBulletPattern(local_58);
                }
                break;
            case ECL_OPCODE_BULLETEFFECTS:
                enemy->bulletProps.exInts[0] = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->bulletEffects.ivar1, NULL);
                enemy->bulletProps.exInts[1] = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->bulletEffects.ivar2, NULL);
                enemy->bulletProps.exInts[2] = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->bulletEffects.ivar3, NULL);
                enemy->bulletProps.exInts[3] = EnemyEclInstr::GetVarValue(enemy, (EclVarId)args->bulletEffects.ivar4, NULL);
                enemy->bulletProps.exFloats[0] =
                    EnemyEclInstr::GetVarFloatValue(enemy, (f32)args->bulletEffects.fvar1, NULL);
                enemy->bulletProps.exFloats[1] =
                    EnemyEclInstr::GetVarFloatValue(enemy, (f32)args->bulletEffects.fvar2, NULL);
                enemy->bulletProps.exFloats[2] =
                    EnemyEclInstr::GetVarFloatValue(enemy, (f32)args->bulletEffects.fvar3, NULL);
                enemy->bulletProps.exFloats[3] =
                    EnemyEclInstr::GetVarFloatValue(enemy, (f32)args->bulletEffects.fvar4, NULL);
                break;
            case ECL_OPCODE_ANMSETDEATH:
                local_5c = &instruction->args.anmSetDeath;
                enemy->deathAnm1 = local_5c->deathAnm1;
                enemy->deathAnm2 = local_5c->deathAnm2;
                enemy->deathAnm3 = local_5c->deathAnm3;
                break;
            case ECL_OPCODE_SHOOTINTERVAL:
                enemy->shootInterval = (i32)instruction->args.setInt;
                enemy->shootInterval += enemy->ShootInterval(g_GameManager.rank);
                enemy->shootIntervalTimer.SetCurrent(0);
                break;
            case ECL_OPCODE_SHOOTINTERVALDELAYED:
                enemy->shootInterval = (i32)instruction->args.setInt;
                enemy->shootInterval += enemy->ShootInterval(g_GameManager.rank);
                if (enemy->shootInterval != 0)
                {
                    enemy->shootIntervalTimer.SetCurrent(g_Rng.GetRandomU32InRange(enemy->shootInterval));
                }
                break;
            case ECL_OPCODE_SHOOTDISABLED:
                enemy->flags.unk3 = 1;
                break;
            case ECL_OPCODE_SHOOTENABLED:
                enemy->flags.unk3 = 0;
                break;
            case ECL_OPCODE_SHOOTNOW:
                enemy->bulletProps.position = enemy->position + enemy->shootOffset;
                g_BulletManager.SpawnBulletPattern(&enemy->bulletProps);
                break;
            case ECL_OPCODE_SHOOTOFFSET:
                enemy->shootOffset.x = EnemyEclInstr::GetVarFloatValue(enemy, (f32)args->move.pos.x, NULL);
                enemy->shootOffset.y = EnemyEclInstr::GetVarFloatValue(enemy, (f32)args->move.pos.y, NULL);
                enemy->shootOffset.z = EnemyEclInstr::GetVarFloatValue(enemy, (f32)args->move.pos.z, NULL);
                break;
            case ECL_OPCODE_LASERCREATE:
            case ECL_OPCODE_LASERCREATEAIMED:
                local_64 = &instruction->args.laser;
                local_60 = &enemy->laserProps;
                local_60->position = enemy->position + enemy->shootOffset;
                local_60->sprite = (i16)local_64->sprite;
                local_60->spriteOffset = (i16)local_64->color;
                local_60->angle = EnemyEclInstr::GetVarFloatValue(enemy, (f32)local_64->angle, NULL);
                local_60->speed = EnemyEclInstr::GetVarFloatValue(enemy, (f32)local_64->speed, NULL);
                local_60->startOffset = EnemyEclInstr::GetVarFloatValue(enemy, (f32)local_64->startOffset, NULL);
                local_60->endOffset = EnemyEclInstr::GetVarFloatValue(enemy, (f32)local_64->endOffset, NULL);
                local_60->startLength = EnemyEclInstr::GetVarFloatValue(enemy, (f32)local_64->startLength, NULL);
                local_60->width = (f32)local_64->width;
                local_60->startTime = (i32)local_64->startTime;
                local_60->duration = (i32)local_64->duration;
                local_60->stopTime = (i32)local_64->stopTime;
                local_60->grazeDelay = (i32)local_64->grazeDelay;
                local_60->grazeDistance = (i32)local_64->grazeDistance;
                local_60->flags = (i32)local_64->flags;
                if ((i16)instruction->opCode == ECL_OPCODE_LASERCREATEAIMED)
                {
                    local_60->type = 0;
                }
                else
                {
                    local_60->type = 1;
                }
                enemy->lasers[enemy->laserStore] = g_BulletManager.SpawnLaserPattern(local_60);
                break;
            case ECL_OPCODE_LASERINDEX:
                enemy->laserStore = EnemyEclInstr::GetVarValue(enemy, (EclVarId)instruction->args.alu.res, NULL);
                break;
            case ECL_OPCODE_LASERROTATE:
                if (enemy->lasers[(i32)instruction->args.laserOp.laserIdx] != NULL)
                {
                    enemy->lasers[(i32)instruction->args.laserOp.laserIdx]->angle +=
                        EnemyEclInstr::GetVarFloatValue(enemy, (f32)instruction->args.laserOp.arg1.x, NULL);
                }
                break;
            case ECL_OPCODE_LASERROTATEFROMPLAYER:
                if (enemy->lasers[(i32)instruction->args.laserOp.laserIdx] != NULL)
                {
                    enemy->lasers[(i32)instruction->args.laserOp.laserIdx]->angle =
                        g_Player.AngleToPlayer(&enemy->lasers[(i32)instruction->args.laserOp.laserIdx]->pos) +
                        EnemyEclInstr::GetVarFloatValue(enemy, (f32)instruction->args.laserOp.arg1.x, NULL);
                }
                break;
            case ECL_OPCODE_LASEROFFSET:
                if (enemy->lasers[(i32)instruction->args.laserOp.laserIdx] != NULL)
                {
                    tmpVec3 = instruction->args.laserOp.arg1;
                    enemy->lasers[(i32)instruction->args.laserOp.laserIdx]->pos = enemy->position + tmpVec3;
                }
                break;
            case ECL_OPCODE_LASERTEST:
                if (enemy->lasers[(i32)instruction->args.laserOp.laserIdx] != NULL &&
                    enemy->lasers[(i32)instruction->args.laserOp.laserIdx]->inUse)
                {
                    enemy->currentContext.compareRegister = 0;
                }
                else
                {
                    enemy->currentContext.compareRegister = 1;
                }
                break;
            case ECL_OPCODE_LASERCANCEL:
                if (enemy->lasers[(i32)instruction->args.laserOp.laserIdx] != NULL &&
                    enemy->lasers[(i32)instruction->args.laserOp.laserIdx]->inUse &&
                    enemy->lasers[(i32)instruction->args.laserOp.laserIdx]->state < 2)
                {
                    enemy->lasers[(i32)instruction->args.laserOp.laserIdx]->state = 2;
                    enemy->lasers[(i32)instruction->args.laserOp.laserIdx]->timer.SetCurrent(0);
                }
                break;
            case ECL_OPCODE_LASERCLEARALL:
                for (local_68 = 0; local_68 < ARRAY_SIZE_SIGNED(enemy->lasers); local_68++)
                {
                    enemy->lasers[local_68] = NULL;
                }
                break;
            case ECL_OPCODE_BOSSSET:
                if ((i32)instruction->args.setInt >= 0)
                {
                    g_EnemyManager.bosses[(i32)instruction->args.setInt] = enemy;
                    g_Gui.bossPresent = 1;
                    g_Gui.SetBossHealthBar(1.0f);
                    enemy->flags.isBoss = 1;
                    enemy->bossId = (i32)instruction->args.setInt;
                }
                else
                {
                    g_Gui.bossPresent = 0;
                    g_EnemyManager.bosses[enemy->bossId] = NULL;
                    enemy->flags.isBoss = 0;
                }
                break;
            case ECL_OPCODE_SPELLCARDEFFECT:
                local_6c = &instruction->args.spellcardEffect;
                enemy->effectArray[enemy->effectIdx] = g_EffectManager.SpawnParticles(
                    0xd, &enemy->position, 1, (ZunColor)g_EffectsColor[(i32)local_6c->effectColorId]);
                enemy->effectArray[enemy->effectIdx]->pos2 = local_6c->pos;
                enemy->effectDistance = (f32)local_6c->effectDistance;
                enemy->effectIdx++;
                break;
            case ECL_OPCODE_MOVEDIRTIMEDECELERATE:
                EnemyEclInstr::MoveDirTime(enemy, instruction);
                enemy->flags.unk2 = 1;
                break;
            case ECL_OPCODE_MOVEDIRTIMEDECELERATEFAST:
                EnemyEclInstr::MoveDirTime(enemy, instruction);
                enemy->flags.unk2 = 2;
                break;
            case ECL_OPCODE_MOVEDIRTIMEACCELERATE:
                EnemyEclInstr::MoveDirTime(enemy, instruction);
                enemy->flags.unk2 = 3;
                break;
            case ECL_OPCODE_MOVEDIRTIMEACCELERATEFAST:
                EnemyEclInstr::MoveDirTime(enemy, instruction);
                enemy->flags.unk2 = 4;
                break;
            case ECL_OPCODE_MOVEPOSITIONTIMELINEAR:
                EnemyEclInstr::MovePosTime(enemy, instruction);
                enemy->flags.unk2 = 0;
                break;
            case ECL_OPCODE_MOVEPOSITIONTIMEDECELERATE:
                EnemyEclInstr::MovePosTime(enemy, instruction);
                enemy->flags.unk2 = 1;
                break;
            case ECL_OPCODE_MOVEPOSITIONTIMEDECELERATEFAST:
                EnemyEclInstr::MovePosTime(enemy, instruction);
                enemy->flags.unk2 = 2;
                break;
            case ECL_OPCODE_MOVEPOSITIONTIMEACCELERATE:
                EnemyEclInstr::MovePosTime(enemy, instruction);
                enemy->flags.unk2 = 3;
                break;
            case ECL_OPCODE_MOVEPOSITIONTIMEACCELERATEFAST:
                EnemyEclInstr::MovePosTime(enemy, instruction);
                enemy->flags.unk2 = 4;
                break;
            case ECL_OPCODE_MOVETIMEDECELERATE:
                EnemyEclInstr::MoveTime(enemy, instruction);
                enemy->flags.unk2 = 1;
                break;
            case ECL_OPCODE_MOVETIMEDECELERATEFAST:
                EnemyEclInstr::MoveTime(enemy, instruction);
                enemy->flags.unk2 = 2;
                break;
            case ECL_OPCODE_MOVETIMEACCELERATE:
                EnemyEclInstr::MoveTime(enemy, instruction);
                enemy->flags.unk2 = 3;
                break;
            case ECL_OPCODE_MOVETIMEACCELERATEFAST:
                EnemyEclInstr::MoveTime(enemy, instruction);
                enemy->flags.unk2 = 4;
                break;
            case ECL_OPCODE_MOVEBOUNDSSET:
                enemy->lowerMoveLimit.x = (f32)instruction->args.moveBoundSet.lowerMoveLimit.x;
                enemy->lowerMoveLimit.y = (f32)instruction->args.moveBoundSet.lowerMoveLimit.y;
                enemy->upperMoveLimit.x = (f32)instruction->args.moveBoundSet.upperMoveLimit.x;
                enemy->upperMoveLimit.y = (f32)instruction->args.moveBoundSet.upperMoveLimit.y;
                enemy->flags.shouldClampPos = 1;
                break;
            case ECL_OPCODE_MOVEBOUNDSDISABLE:
                enemy->flags.shouldClampPos = 0;
                break;
            case ECL_OPCODE_MOVERAND:
                local_8 = instruction->args.move.pos;
                enemy->angle = g_Rng.GetRandomF32InRange((f32)local_8.y - (f32)local_8.x) + (f32)local_8.x;
                break;
            case ECL_OPCODE_MOVERANDINBOUND:
                local_8 = instruction->args.move.pos;
                enemy->angle = g_Rng.GetRandomF32InRange((f32)local_8.y - (f32)local_8.x) + (f32)local_8.x;
                if (enemy->position.x < enemy->lowerMoveLimit.x + 96.0f)
                {
                    if (enemy->angle > ZUN_PI / 2.0f)
                    {
                        enemy->angle = ZUN_PI - enemy->angle;
                    }
                    else if (enemy->angle < -ZUN_PI / 2.0f)
                    {
                        enemy->angle = -ZUN_PI - enemy->angle;
                    }
                }
                if (enemy->position.x > enemy->upperMoveLimit.x - 96.0f)
                {
                    if (enemy->angle < ZUN_PI / 2.0f && enemy->angle >= 0.0f)
                    {
                        enemy->angle = ZUN_PI - enemy->angle;
                    }
                    else if (enemy->angle > -ZUN_PI / 2.0f && enemy->angle <= 0.0f)
                    {
                        enemy->angle = -ZUN_PI - enemy->angle;
                    }
                }
                if (enemy->position.y < enemy->lowerMoveLimit.y + 48.0f && enemy->angle < 0.0f)
                {
                    enemy->angle = -enemy->angle;
                }
                if (enemy->position.y > enemy->upperMoveLimit.y - 48.0f && enemy->angle > 0.0f)
                {
                    enemy->angle = -enemy->angle;
                }
                break;
            case ECL_OPCODE_ANMSETPOSES:
                enemy->anmExDefaults = (i16)instruction->args.anmSetPoses.anmExDefault;
                enemy->anmExFarLeft = (i16)instruction->args.anmSetPoses.anmExFarLeft;
                enemy->anmExFarRight = (i16)instruction->args.anmSetPoses.anmExFarRight;
                enemy->anmExLeft = (i16)instruction->args.anmSetPoses.anmExLeft;
                enemy->anmExRight = (i16)instruction->args.anmSetPoses.anmExRight;
                enemy->anmExFlags = 0xff;
                break;
            case ECL_OPCODE_ENEMYSETHITBOX:
                enemy->hitboxDimensions.x = (f32)instruction->args.move.pos.x;
                enemy->hitboxDimensions.y = (f32)instruction->args.move.pos.y;
                enemy->hitboxDimensions.z = (f32)instruction->args.move.pos.z;
                break;
            case ECL_OPCODE_ENEMYFLAGCOLLISION:
                enemy->flags.unk7 = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_ENEMYFLAGCANTAKEDAMAGE:
                enemy->flags.unk10 = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_EFFECTSOUND:
                g_SoundPlayer.PlaySoundByIdx((SoundIdx)(i32)instruction->args.setInt);
                break;
            case ECL_OPCODE_ENEMYFLAGDEATH:
                enemy->flags.unk11 = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_DEATHCALLBACKSUB:
                enemy->deathCallbackSub = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_ENEMYINTERRUPTSET:
                enemy->interrupts[(i32)args->setInterrupt.interruptId] = (i32)args->setInterrupt.interruptSub;
                break;
            case ECL_OPCODE_ENEMYINTERRUPT:
                enemy->runInterrupt = (i32)instruction->args.setInt;
            HANDLE_INTERRUPT:
                instruction = (EclRawInstr *)enemy->currentContext.currentInstr;
                enemy->currentContext.currentInstr = (EclRawInstr *)((u8 *)instruction + (i16)instruction->offsetToNext);
                if (enemy->flags.unk14 == 0)
                {
                    memcpy(&enemy->savedContextStack[enemy->stackDepth], &enemy->currentContext,
                           sizeof(EnemyEclContext));
                }
                g_EclManager.CallEclSub(&enemy->currentContext, (i16)enemy->interrupts[enemy->runInterrupt]);
                if (enemy->stackDepth < ARRAY_SIZE_SIGNED(enemy->savedContextStack) - 1)
                {
                    enemy->stackDepth++;
                }
                enemy->runInterrupt = -1;
                continue;
            case ECL_OPCODE_ENEMYLIFESET:
                enemy->life = enemy->maxLife = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_SPELLCARDSTART:
                g_Gui.ShowSpellcard((i16)instruction->args.spellcardStart.spellcardSprite,
                                    instruction->args.spellcardStart.spellcardName);
                g_EnemyManager.spellcardInfo.isCapturing = 1;
                g_EnemyManager.spellcardInfo.isActive = 1;
                g_EnemyManager.spellcardInfo.idx = (i16)instruction->args.spellcardStart.spellcardId;
                g_EnemyManager.spellcardInfo.captureScore = g_SpellcardScore[g_EnemyManager.spellcardInfo.idx];
                g_BulletManager.TurnAllBulletsIntoPoints();
                g_Stage.spellcardState = RUNNING;
                g_Stage.ticksSinceSpellcardStarted = 0;
                enemy->bulletRankSpeedLow = -0.5f;
                enemy->bulletRankSpeedHigh = 0.5f;
                enemy->bulletRankAmount1Low = 0;
                enemy->bulletRankAmount1High = 0;
                enemy->bulletRankAmount2Low = 0;
                enemy->bulletRankAmount2High = 0;
                local_70 = &g_GameManager.catk[g_EnemyManager.spellcardInfo.idx];
                csum = 0;
                if (!g_GameManager.isInReplay)
                {
                    strcpy(local_70->name, instruction->args.spellcardStart.spellcardName);
                    local_74 = strlen(local_70->name);
                    while (0 < local_74)
                    {
                        csum += local_70->name[--local_74];
                    }
                    if (local_70->nameCsum != (u8)csum)
                    {
                        local_70->numSuccess = 0;
                        local_70->numAttempts = 0;
                        local_70->nameCsum = csum;
                    }
                    local_70->captureScore = g_EnemyManager.spellcardInfo.captureScore;
                    if (local_70->numAttempts < 9999)
                    {
                        local_70->numAttempts++;
                    }
                }
                break;
            case ECL_OPCODE_SPELLCARDEND:
                if (g_EnemyManager.spellcardInfo.isActive != 0)
                {
                    g_Gui.EndEnemySpellcard();
                    if (g_EnemyManager.spellcardInfo.isActive == 1)
                    {
                        scoreIncrease = g_BulletManager.DespawnBullets(12800, 1);
                        if (g_EnemyManager.spellcardInfo.isCapturing)
                        {
                            local_80 = &g_GameManager.catk[g_EnemyManager.spellcardInfo.idx];
                            local_88 = g_EnemyManager.spellcardInfo.captureScore >= 500000
                                           ? 500000 / 10
                                           : g_EnemyManager.spellcardInfo.captureScore / 10;
                            scoreIncrease =
                                g_EnemyManager.spellcardInfo.captureScore +
                                g_EnemyManager.spellcardInfo.captureScore * g_Gui.SpellcardSecondsRemaining() / 10;
                            g_Gui.ShowSpellcardBonus(scoreIncrease);
                            g_GameManager.score += scoreIncrease;
                            if (!g_GameManager.isInReplay)
                            {
                                local_80->numSuccess++;
                                // What. the. fuck?
                                // memmove(&local_80->nameCsum, &local_80->characterShotType, 4);
                                for (local_84 = 4; 0 < local_84; local_84 = local_84 + -1)
                                {
                                    ((u8 *)&local_80->nameCsum)[local_84 + 1] = ((u8 *)&local_80->nameCsum)[local_84];
                                }
                                local_80->characterShotType = g_GameManager.CharacterShotType();
                            }
                            g_GameManager.spellcardsCaptured++;
                        }
                    }
                    g_EnemyManager.spellcardInfo.isActive = 0;
                }
                g_Stage.spellcardState = NOT_RUNNING;
                break;
            case ECL_OPCODE_BOSSTIMERSET:
                enemy->bossTimer.SetCurrent((i32)instruction->args.setInt);
                break;
            case ECL_OPCODE_LIFECALLBACKTHRESHOLD:
                enemy->lifeCallbackThreshold = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_LIFECALLBACKSUB:
                enemy->lifeCallbackSub = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_TIMERCALLBACKTHRESHOLD:
                enemy->timerCallbackThreshold = (i32)instruction->args.setInt;
                enemy->bossTimer.SetCurrent(0);
                break;
            case ECL_OPCODE_TIMERCALLBACKSUB:
                enemy->timerCallbackSub = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_ENEMYFLAGINTERACTABLE:
                enemy->flags.unk6 = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_EFFECTPARTICLE:
                g_EffectManager.SpawnParticles((i32)instruction->args.effectParticle.effectId, &enemy->position,
                                               (i32)instruction->args.effectParticle.numParticles,
                                               (ZunColor)instruction->args.effectParticle.particleColor);
                break;
            case ECL_OPCODE_DROPITEMS:
                for (local_8c = 0; local_8c < (i32)instruction->args.setInt; local_8c++)
                {
                    local_98 = enemy->position;

                    g_Rng.GetRandomF32InBounds(&local_98.x, -72.0f, 72.0f);
                    g_Rng.GetRandomF32InBounds(&local_98.y, -72.0f, 72.0f);
                    if (g_GameManager.currentPower < 128)
                    {
                        g_ItemManager.SpawnItem(&local_98, local_8c == 0 ? ITEM_POWER_BIG : ITEM_POWER_SMALL, 0);
                    }
                    else
                    {
                        g_ItemManager.SpawnItem(&local_98, ITEM_POINT, 0);
                    }
                }
                break;
            case ECL_OPCODE_ANMFLAGROTATION:
                enemy->flags.unk13 = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_EXINSCALL:
                g_EclExInsn[(i32)instruction->args.setInt](enemy, instruction);
                break;
            case ECL_OPCODE_EXINSREPEAT:
                if ((i32)instruction->args.setInt >= 0)
                {
                    enemy->currentContext.funcSetFunc = g_EclExInsn[(i32)instruction->args.setInt];
                }
                else
                {
                    enemy->currentContext.funcSetFunc = NULL;
                }
                break;
            case ECL_OPCODE_TIMESET:
                enemy->currentContext.time.IncrementInline(
                    EnemyEclInstr::GetVarValue(enemy, (EclVarId)instruction->args.timeSet.timeToSet, NULL));
                break;
            case ECL_OPCODE_DROPITEMID:
                g_ItemManager.SpawnItem(&enemy->position, (ItemType)instruction->args.dropItem.itemId, 0);
                break;
            case ECL_OPCODE_STDUNPAUSE:
                g_Stage.unpauseFlag = 1;
                break;
            case ECL_OPCODE_BOSSSETLIFECOUNT:
                g_Gui.eclSetLives = instruction->args.GetBossLifeCount();
                g_GameManager.counat += 1800;
                break;
            case ECL_OPCODE_ENEMYCREATE:
                local_b0 = instruction->args.enemyCreate;
                tmpVec3 = local_b0.pos;
                tmpVec3.x = EnemyEclInstr::GetVarFloatValue(enemy, tmpVec3.x, NULL);
                tmpVec3.y = EnemyEclInstr::GetVarFloatValue(enemy, tmpVec3.y, NULL);
                tmpVec3.z = EnemyEclInstr::GetVarFloatValue(enemy, tmpVec3.z, NULL);
                g_EnemyManager.SpawnEnemy((i32)local_b0.subId, &tmpVec3, (i16)local_b0.life, (i16)local_b0.itemDrop, (i32)local_b0.score);
                break;
            case ECL_OPCODE_ENEMYKILLALL:
                for (local_b4 = &g_EnemyManager.enemies[0], local_b8 = 0;
                     local_b8 < ARRAY_SIZE_SIGNED(g_EnemyManager.enemies) - 1; local_b8++, local_b4++)
                {
                    if (!local_b4->flags.active)
                    {
                        continue;
                    }
                    if (local_b4->flags.isBoss)
                    {
                        continue;
                    }

                    local_b4->life = 0;
                    if (local_b4->flags.unk6 == 0 && 0 <= local_b4->deathCallbackSub)
                    {
                        g_EclManager.CallEclSub(&local_b4->currentContext, (i16)local_b4->deathCallbackSub);
                        local_b4->deathCallbackSub = -1;
                    }
                }
                break;
            case ECL_OPCODE_ANMINTERRUPTMAIN:
                enemy->primaryVm.pendingInterrupt = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_ANMINTERRUPTSLOT:
                enemy->vms[(i32)args->anmInterruptSlot.vmId].pendingInterrupt = (i32)args->anmInterruptSlot.interruptId;
                break;
            case ECL_OPCODE_BULLETCANCEL:
                g_BulletManager.TurnAllBulletsIntoPoints();
                break;
            case ECL_OPCODE_BULLETSOUND:
                if ((i32)instruction->args.bulletSound.bulletSfx >= 0)
                {
                    enemy->bulletProps.sfx = (SoundIdx)(i16)instruction->args.bulletSound.bulletSfx;
                    enemy->bulletProps.flags |= 0x200;
                }
                else
                {
                    enemy->bulletProps.flags &= 0xfffffdff;
                }
                break;
            case ECL_OPCODE_ENEMYFLAGDISABLECALLSTACK:
                enemy->flags.unk14 = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_BULLETRANKINFLUENCE:
                enemy->bulletRankSpeedLow = (f32)args->bulletRankInfluence.bulletRankSpeedLow;
                enemy->bulletRankSpeedHigh = (f32)args->bulletRankInfluence.bulletRankSpeedHigh;
                enemy->bulletRankAmount1Low = (i32)args->bulletRankInfluence.bulletRankAmount1Low;
                enemy->bulletRankAmount1High = (i32)args->bulletRankInfluence.bulletRankAmount1High;
                enemy->bulletRankAmount2Low = (i32)args->bulletRankInfluence.bulletRankAmount2Low;
                enemy->bulletRankAmount2High = (i32)args->bulletRankInfluence.bulletRankAmount2High;
                break;
            case ECL_OPCODE_ENEMYFLAGINVISIBLE:
                enemy->flags.unk15 = (i32)instruction->args.setInt;
                break;
            case ECL_OPCODE_BOSSTIMERCLEAR:
                enemy->timerCallbackSub = enemy->deathCallbackSub;
                enemy->bossTimer.SetCurrent(0);
                break;
            case ECL_OPCODE_SPELLCARDFLAGTIMEOUT:
                enemy->flags.unk16 = (i32)instruction->args.setInt;
                break;
            }
        NEXT_INSN:
            instruction = (EclRawInstr *)((u8 *)instruction + (i16)instruction->offsetToNext);
            goto YOLO;
        }
        else
        {
            switch (enemy->flags.unk1)
            {
            case 1:
                enemy->angle = utils::AddNormalizeAngle(enemy->angle, g_Supervisor.effectiveFramerateMultiplier *
                                                                          enemy->angularVelocity);
                enemy->speed = g_Supervisor.effectiveFramerateMultiplier * enemy->acceleration + enemy->speed;
                sincosmul(&enemy->axisSpeed, enemy->angle, enemy->speed);
                enemy->axisSpeed.z = 0.0;
                break;
            case 2:
                enemy->moveInterpTimer.Decrement(1);
                local_bc = enemy->moveInterpTimer.AsFramesFloat() / enemy->moveInterpStartTime;
                if (local_bc >= 1.0f)
                {
                    local_bc = 1.0f;
                }
                switch (enemy->flags.unk2)
                {
                case 0:
                    local_bc = 1.0f - local_bc;
                    break;
                case 1:
                    local_bc = 1.0f - local_bc * local_bc;
                    break;
                case 2:
                    local_bc = 1.0f - local_bc * local_bc * local_bc * local_bc;
                    break;
                case 3:
                    local_bc = 1.0f - local_bc;
                    local_bc *= local_bc;
                    break;
                case 4:
                    local_bc = 1.0f - local_bc;
                    local_bc = local_bc * local_bc * local_bc * local_bc;
                }
                enemy->axisSpeed = enemy->moveInterp * local_bc + enemy->moveInterpStartPos - enemy->position;
                enemy->angle = ZUN_ATAN2F(enemy->axisSpeed.y, enemy->axisSpeed.x);
                if (enemy->moveInterpTimer.current <= 0)
                {
                    enemy->flags.unk1 = 0;
                    enemy->position = enemy->moveInterpStartPos + enemy->moveInterp;
                    enemy->axisSpeed = ZunVec3(0.0f, 0.0f, 0.0f);
                }
                break;
            }
            if (0 < enemy->life)
            {
                if (0 < enemy->shootInterval)
                {
                    enemy->shootIntervalTimer.Tick();
                    if (enemy->shootIntervalTimer.current >= enemy->shootInterval)
                    {
                        enemy->bulletProps.position = enemy->position + enemy->shootOffset;
                        g_BulletManager.SpawnBulletPattern(&enemy->bulletProps);
                        enemy->shootIntervalTimer.InitializeForPopup();
                    }
                }
                if (0 <= enemy->anmExLeft)
                {
                    local_c0 = 0;
                    if (enemy->axisSpeed.x < 0.0f)
                    {
                        local_c0 = 1;
                    }
                    else if (enemy->axisSpeed.x > 0.0f)
                    {
                        local_c0 = 2;
                    }
                    if (enemy->anmExFlags != local_c0)
                    {
                        switch (local_c0)
                        {
                        case 0:
                            if (enemy->anmExFlags == 0xff)
                            {
                                g_AnmManager->SetAndExecuteScriptIdx(&enemy->primaryVm,
                                                                     enemy->anmExDefaults + ANM_OFFSET_ENEMY);
                            }
                            else if (enemy->anmExFlags == 1)
                            {
                                g_AnmManager->SetAndExecuteScriptIdx(&enemy->primaryVm,
                                                                     enemy->anmExFarLeft + ANM_OFFSET_ENEMY);
                            }
                            else
                            {
                                g_AnmManager->SetAndExecuteScriptIdx(&enemy->primaryVm,
                                                                     enemy->anmExFarRight + ANM_OFFSET_ENEMY);
                            }
                            break;
                        case 1:
                            g_AnmManager->SetAndExecuteScriptIdx(&enemy->primaryVm,
                                                                 enemy->anmExLeft + ANM_OFFSET_ENEMY);
                            break;
                        case 2:
                            g_AnmManager->SetAndExecuteScriptIdx(&enemy->primaryVm,
                                                                 enemy->anmExRight + ANM_OFFSET_ENEMY);
                            break;
                        }
                        enemy->anmExFlags = local_c0;
                    }
                }
                if (enemy->currentContext.funcSetFunc != NULL)
                {
                    enemy->currentContext.funcSetFunc(enemy, NULL);
                }
            }
            enemy->currentContext.currentInstr = instruction;
            enemy->currentContext.time.Tick();
            return ZUN_SUCCESS;
        }
    }
}
