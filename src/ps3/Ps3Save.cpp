#ifdef __PS3__
#include "Ps3Save.hpp"
#include <cell/sysmodule.h>
#include <sysutil/sysutil_savedata.h>
#include <sysutil/sysutil_common.h>
#include <cell/cell_fs.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/memory.h>
#include <sys/timer.h>
#include "GamePaths.hpp"
#include "FileSystem.hpp"
#include "utils.hpp"

#ifndef CELL_SYSMODULE_SAVEDATA
#define CELL_SYSMODULE_SAVEDATA 0x000b
#endif

extern "C" void* memalign(size_t boundary, size_t size);

namespace Ps3Save
{

static bool s_initialized = false;
static const char* s_dirName = "TH06PORT0-SAVE"; 

static char s_syncFileList[18][32];
static int s_syncFileCount = 0;
static int s_currentFileIdx = 0;
static bool s_isSaving = true;
static void* s_fileBuf = NULL;
static CellSaveDataSetBuf s_setBuf;
static bool s_isUtilityRunning = false;
static void* s_newIconBuf = NULL;

void Init()
{
    if (s_initialized) return;
    printf("Ps3Save: Init\n");
    cellSysmoduleLoadModule(CELL_SYSMODULE_SAVEDATA);
    cellSaveDataEnableOverlay(1);
    
    // Ensure working directories exist
    cellFsMkdir("/dev_hdd0/game/TH06PORT0/USRDIR", 0777);
    cellFsMkdir("/dev_hdd0/game/TH06PORT0/USRDIR/replay", 0777);

    memset(&s_setBuf, 0, sizeof(s_setBuf));
    s_setBuf.bufSize = 2 * 1024 * 1024;
    s_setBuf.dirListMax = 1;
    s_setBuf.fileListMax = 32;
    s_setBuf.buf = memalign(64, s_setBuf.bufSize);
    
    s_initialized = true;
}

void Shutdown()
{
    if (!s_initialized) return;
    printf("Ps3Save: Shutdown\n");
    if (s_setBuf.buf) { free(s_setBuf.buf); s_setBuf.buf = NULL; }
    if (s_newIconBuf) { free(s_newIconBuf); s_newIconBuf = NULL; }
    s_initialized = false;
}

static void PrepareSyncList()
{
    s_syncFileCount = 0;
    strcpy(s_syncFileList[s_syncFileCount++], "ICON0.PNG");
    strcpy(s_syncFileList[s_syncFileCount++], "th06.cfg");
    strcpy(s_syncFileList[s_syncFileCount++], "score.dat");
    for (int i = 1; i <= 15; ++i) {
        sprintf(s_syncFileList[s_syncFileCount++], "replay_th6_%02d.rpy", i);
    }
}

static void fixed_cb(CellSaveDataCBResult *cbResult, CellSaveDataListGet *get, CellSaveDataFixedSet *set)
{
    printf("Ps3Save: fixed_cb, found=%u\n", get->dirNum);
    static CellSaveDataNewDataIcon s_newDataIcon;
    memset(cbResult, 0, sizeof(CellSaveDataCBResult));
    memset(set, 0, sizeof(CellSaveDataFixedSet));

    set->dirName = (char*)s_dirName;

    if (s_isSaving) {
        memset(&s_newDataIcon, 0, sizeof(s_newDataIcon));
        s_newDataIcon.title = (char*)"Touhou Koumakyou";
        
        char iconPath[512];
        snprintf(iconPath, sizeof(iconPath), "%sICON0.PNG", GamePaths::GetUserPath());
        FILE* f = fopen(iconPath, "rb");
        if (!f) {
            snprintf(iconPath, sizeof(iconPath), "/dev_hdd0/game/TH06PORT0/USRDIR/ICON0.PNG");
            f = fopen(iconPath, "rb");
        }

        if (f) {
            fseek(f, 0, SEEK_END);
            size_t size = ftell(f);
            fseek(f, 0, SEEK_SET);
            if (s_newIconBuf) free(s_newIconBuf);
            s_newIconBuf = memalign(64, (size + 63) & ~63);
            if (s_newIconBuf) {
                fread(s_newIconBuf, 1, size, f);
                s_newDataIcon.iconBuf = s_newIconBuf;
                s_newDataIcon.iconBufSize = (unsigned int)size;
                set->newIcon = &s_newDataIcon;
                printf("Ps3Save: Provided icon (%u bytes)\n", (unsigned int)size);
            }
            fclose(f);
        }
    }
    cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
}

static void stat_cb(CellSaveDataCBResult *cbResult, CellSaveDataStatGet *get, CellSaveDataStatSet *set)
{
    printf("Ps3Save: stat_cb, isNew=%u\n", get->isNewData);
    static CellSaveDataSystemFileParam s_sysParam;
    memset(cbResult, 0, sizeof(CellSaveDataCBResult));
    memset(set, 0, sizeof(CellSaveDataStatSet));

    if (s_isSaving) {
        memset(&s_sysParam, 0, sizeof(s_sysParam));
        strncpy(s_sysParam.title, "Touhou Koumakyou", CELL_SAVEDATA_SYSP_TITLE_SIZE - 1);
        strncpy(s_sysParam.subTitle, "the Embodiment of Scarlet Devil", CELL_SAVEDATA_SYSP_SUBTITLE_SIZE - 1);
        strncpy(s_sysParam.detail, "Saved Data", CELL_SAVEDATA_SYSP_DETAIL_SIZE - 1);
        s_sysParam.attribute = CELL_SAVEDATA_ATTR_NORMAL;
        
        set->setParam = &s_sysParam;
        if (get->isNewData == CELL_SAVEDATA_ISNEWDATA_YES) {
            printf("Ps3Save: Detected New Data, using RECREATE_YES\n");
            set->reCreateMode = CELL_SAVEDATA_RECREATE_YES;
        } else {
            set->reCreateMode = CELL_SAVEDATA_RECREATE_NO;
        }
    }

    cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
    s_currentFileIdx = 0;
    PrepareSyncList();
}

static void file_cb(CellSaveDataCBResult *cbResult, CellSaveDataFileGet *get, CellSaveDataFileSet *set)
{
    memset(cbResult, 0, sizeof(CellSaveDataCBResult));
    memset(set, 0, sizeof(CellSaveDataFileSet));

    if (s_fileBuf) {
        if (!s_isSaving && s_currentFileIdx > 0 && get->excSize > 0) {
            const char* ps3Fname = s_syncFileList[s_currentFileIdx - 1];
            if (strcmp(ps3Fname, "ICON0.PNG") != 0) {
                char usrdirFname[64];
                if (strncmp(ps3Fname, "replay_", 7) == 0) sprintf(usrdirFname, "replay/%s", ps3Fname + 7);
                else strcpy(usrdirFname, ps3Fname);
                
                char fullPath[512];
                snprintf(fullPath, sizeof(fullPath), "%s%s", GamePaths::GetUserPath(), usrdirFname);
                
                GamePaths::EnsureParentDir(fullPath);
                FILE* f = fopen(fullPath, "wb");
                if (f) {
                    fwrite(s_fileBuf, 1, get->excSize, f);
                    fclose(f);
                    printf("Ps3Save: Restored %s\n", ps3Fname);
                }
            }
        }
        free(s_fileBuf);
        s_fileBuf = NULL;
    }

    while (s_currentFileIdx < s_syncFileCount) {
        const char* ps3Fname = s_syncFileList[s_currentFileIdx];
        char fullPath[512];
        bool isIcon = (strcmp(ps3Fname, "ICON0.PNG") == 0);
        
        if (isIcon) {
            snprintf(fullPath, sizeof(fullPath), "%sICON0.PNG", GamePaths::GetUserPath());
        } else {
            char usrdirFname[64];
            if (strncmp(ps3Fname, "replay_", 7) == 0) sprintf(usrdirFname, "replay/%s", ps3Fname + 7);
            else strcpy(usrdirFname, ps3Fname);
            snprintf(fullPath, sizeof(fullPath), "%s%s", GamePaths::GetUserPath(), usrdirFname);
        }

        if (s_isSaving) {
            FILE* f = fopen(fullPath, "rb");
            if (f) {
                printf("Ps3Save: Saving %s\n", ps3Fname);
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                s_fileBuf = memalign(64, (size + 63) & ~63);
                if (s_fileBuf) {
                    fread(s_fileBuf, 1, size, f);
                    set->fileOperation = CELL_SAVEDATA_FILEOP_WRITE;
                    if (isIcon) {
                        set->fileType = CELL_SAVEDATA_FILETYPE_CONTENT_ICON0;
                        set->fileName = NULL;
                    } else {
                        set->fileType = CELL_SAVEDATA_FILETYPE_NORMALFILE;
                        set->fileName = (char*)ps3Fname;
                    }
                    
                    set->fileSize = (unsigned int)size;
                    set->fileBufSize = (unsigned int)((size + 63) & ~63);
                    set->fileBuf = s_fileBuf;
                    cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
                    fclose(f);
                    s_currentFileIdx++;
                    return;
                }
                fclose(f);
            }
        } else {
            if (!isIcon) {
                printf("Ps3Save: Reading %s\n", ps3Fname);
                set->fileOperation = CELL_SAVEDATA_FILEOP_READ;
                set->fileType = CELL_SAVEDATA_FILETYPE_NORMALFILE;
                set->fileName = (char*)ps3Fname;
                set->fileBufSize = 1024 * 1024;
                s_fileBuf = memalign(64, set->fileBufSize);
                if (s_fileBuf) {
                    set->fileBuf = s_fileBuf;
                    cbResult->result = CELL_SAVEDATA_CBRESULT_OK_NEXT;
                    s_currentFileIdx++;
                    return;
                }
            }
        }
        s_currentFileIdx++;
    }
    
    cbResult->result = CELL_SAVEDATA_CBRESULT_OK_LAST;
    s_isUtilityRunning = false;
}

bool LoadFromNative()
{
    Init();
    printf("Ps3Save: LoadFromNative\n");
    s_isSaving = false;
    s_currentFileIdx = 0;
    if (s_fileBuf) { free(s_fileBuf); s_fileBuf = NULL; }
    
    sys_memory_container_t container = SYS_MEMORY_CONTAINER_ID_INVALID;
    sys_memory_container_create(&container, 2 * 1024 * 1024);

    CellSaveDataSetList setList;
    memset(&setList, 0, sizeof(setList));
    setList.sortType = CELL_SAVEDATA_SORTTYPE_MODIFIEDTIME;
    setList.sortOrder = CELL_SAVEDATA_SORTORDER_DESCENT;

    s_isUtilityRunning = true;
    int ret = cellSaveDataFixedLoad2(CELL_SAVEDATA_VERSION_OLD, &setList, &s_setBuf, fixed_cb, stat_cb, file_cb, container, NULL);

    if (ret != CELL_OK) {
        printf("Ps3Save: cellSaveDataFixedLoad2 error 0x%x\n", ret);
        s_isUtilityRunning = false;
        if (container != SYS_MEMORY_CONTAINER_ID_INVALID) sys_memory_container_destroy(container);
        return false;
    }

    while (s_isUtilityRunning) {
        cellSysutilCheckCallback();
        sys_timer_usleep(1000);
    }

    if (container != SYS_MEMORY_CONTAINER_ID_INVALID) sys_memory_container_destroy(container);
    return true;
}

bool SaveToNative()
{
    Init();
    printf("Ps3Save: SaveToNative\n");
    s_isSaving = true;
    s_currentFileIdx = 0;
    if (s_fileBuf) { free(s_fileBuf); s_fileBuf = NULL; }

    sys_memory_container_t container = SYS_MEMORY_CONTAINER_ID_INVALID;
    sys_memory_container_create(&container, 2 * 1024 * 1024);

    CellSaveDataSetList setList;
    memset(&setList, 0, sizeof(setList));
    setList.sortType = CELL_SAVEDATA_SORTTYPE_MODIFIEDTIME;
    setList.sortOrder = CELL_SAVEDATA_SORTORDER_DESCENT;

    s_isUtilityRunning = true;
    int ret = cellSaveDataFixedSave2(CELL_SAVEDATA_VERSION_OLD, &setList, &s_setBuf, fixed_cb, stat_cb, file_cb, container, NULL);

    if (ret != CELL_OK) {
        printf("Ps3Save: cellSaveDataFixedSave2 error 0x%x\n", ret);
        s_isUtilityRunning = false;
        if (container != SYS_MEMORY_CONTAINER_ID_INVALID) sys_memory_container_destroy(container);
        return false;
    }

    while (s_isUtilityRunning) {
        cellSysutilCheckCallback();
        sys_timer_usleep(1000);
    }

    if (container != SYS_MEMORY_CONTAINER_ID_INVALID) sys_memory_container_destroy(container);
    return true;
}

} // namespace Ps3Save
#endif
