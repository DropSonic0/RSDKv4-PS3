#include "RetroEngine.hpp"
#include "BackgroundLoader.hpp"

#if RETRO_PLATFORM == RETRO_PS3
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include <sys/synchronization.h>
#include <stdlib.h>
#include <ctype.h>
#endif

static char *retro_strcasestr(const char *haystack, const char *needle)
{
    if (!*needle) return (char *)haystack;
    for (; *haystack; haystack++) {
        if (toupper((unsigned char)*haystack) == toupper((unsigned char)*needle)) {
            const char *h, *n;
            for (h = haystack, n = needle; *h && *n; h++, n++) {
                if (toupper((unsigned char)*h) != toupper((unsigned char)*n)) break;
            }
            if (!*n) return (char *)haystack;
        }
    }
    return NULL;
}

#if RETRO_PLATFORM == RETRO_PS3 || !defined(_GNU_SOURCE)
#define strcasestr retro_strcasestr
#endif

int preloadStatus = PRELOAD_IDLE;
int preloadListID = -1;
int preloadStageID = -1;

PreloadScene *preloadedData = nullptr;

#if RETRO_PLATFORM == RETRO_PS3
// Mutex for background thread safety
static sys_lwmutex_t preloadMutex __attribute__((aligned(16)));
static bool preloadMutexCreated = false;

sys_ppu_thread_t preloadThread;
bool preloadThreadRunning = false;

void PreloadThreadFunc(uint64_t arg);
#endif

void InitBackgroundLoader()
{
#if RETRO_PLATFORM == RETRO_PS3
    if (!preloadMutexCreated) {
        sys_lwmutex_attribute_t mutexAttr;
        sys_lwmutex_attribute_initialize(mutexAttr);
        sys_lwmutex_create(&preloadMutex, &mutexAttr);
        preloadMutexCreated = true;
    }
    preloadedData = (PreloadScene*)memalign(16, sizeof(PreloadScene));
#else
    preloadedData = (PreloadScene*)malloc(sizeof(PreloadScene));
#endif
    if (preloadedData) {
        memset(preloadedData, 0, sizeof(PreloadScene));
        for (int i = 0; i < PRELOAD_FILE_COUNT; i++) {
#if RETRO_PLATFORM == RETRO_PS3
            preloadedData->files[i].buffer = (byte*)memalign(16, PRELOAD_BUF_SIZE);
#else
            preloadedData->files[i].buffer = (byte*)malloc(PRELOAD_BUF_SIZE);
#endif
            preloadedData->files[i].fileName[0] = 0;
            preloadedData->files[i].size = 0;
            preloadedData->files[i].encrypted = false;
        }
    }
    preloadStatus = PRELOAD_IDLE;
}

void StartStagePreload(int listID, int stageID)
{
    if (preloadStatus == PRELOAD_LOADING) return;
    
    if (preloadStatus == PRELOAD_READY && preloadListID == listID && preloadStageID == stageID) return;

    PrintLog("Background Loading START for %s - %s", stageListNames[listID], stageList[listID][stageID].name);

    preloadListID = listID;
    preloadStageID = stageID;
    preloadStatus = PRELOAD_LOADING;

#if RETRO_PLATFORM == RETRO_PS3
    if (preloadThreadRunning) {
        uint64_t exit_code;
        sys_ppu_thread_join(preloadThread, &exit_code);
        preloadThreadRunning = false;
    }
    preloadThreadRunning = true;
    sys_ppu_thread_create(&preloadThread, PreloadThreadFunc, 0, 1001, 0x40000, SYS_PPU_THREAD_CREATE_JOINABLE, "PreloadThread");
#else
    preloadStatus = PRELOAD_IDLE;
#endif
}

#if RETRO_PLATFORM == RETRO_PS3

// Helper to resolve modded and packed paths using native FILE I/O only
static bool ThreadedResolvePath(char *dest, const char *filePath, int *packID, int *offset, int *size, bool *encrypted) {
    if (preloadMutexCreated) sys_lwmutex_lock(&preloadMutex, 0);

    char filePathBuf[0x100];
    strcpy(filePathBuf, filePath);
    bool forceFolder = false;
    
    *packID = -1;
    *offset = 0;
    *size = 0;
    *encrypted = false;

#if RETRO_USE_MOD_LOADER
    char pathLower[0x100];
    int len = (int)strlen(filePath);
    for (int i = 0; i < len; i++) pathLower[i] = tolower(filePath[i]);
    pathLower[len] = 0;

    for (int m = 0; m < (int)modList.size(); m++) {
        if (modList[m].active) {
            std::map<std::string, std::string>::const_iterator it = modList[m].fileMap.find(pathLower);
            if (it != modList[m].fileMap.end()) {
                strcpy(filePathBuf, it->second.c_str());
                forceFolder = true;
                break;
            }
        }
    }

    if (forceUseScripts && !forceFolder) {
        if (strncmp(filePathBuf, "Data/Scripts/", 13) == 0 && (strstr(filePathBuf, ".txt") || strstr(filePathBuf, ".TXT"))) {
            forceFolder = true;
            char scriptPath[0x100];
            strcpy(scriptPath, filePathBuf + 5); // Skip "Data/"
            strcpy(filePathBuf, scriptPath);
        }
    }
#endif

    int fileIndex = -1;
    if (!forceFolder) {
        char pathLower2[0x100];
        int len2 = (int)strlen(filePath);
        for (int i = 0; i < len2; i++) pathLower2[i] = tolower(filePath[i]);
        pathLower2[len2] = 0;
        fileIndex = CheckFileInfo(pathLower2);
    }

    if (fileIndex != -1 && !forceFolder) {
        *packID = rsdkContainer.files[fileIndex].packID;
        *offset = rsdkContainer.files[fileIndex].offset;
        *size = rsdkContainer.files[fileIndex].filesize;
        *encrypted = rsdkContainer.files[fileIndex].encrypted;
        strcpy(dest, rsdkContainer.packNames[*packID]);
        if (preloadMutexCreated) sys_lwmutex_unlock(&preloadMutex);
        return true;
    } else {
        char tmp[0x200];
        sprintf(tmp, "%s%s", gamePath, filePathBuf);
        // Use native fopen for thread safety
        FILE *f = fopen(tmp, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            *size = ftell(f);
            fclose(f);
            strcpy(dest, tmp);
            if (preloadMutexCreated) sys_lwmutex_unlock(&preloadMutex);
            return true;
        }
        
        sprintf(tmp, "%s%s", BASE_PATH, filePathBuf);
        f = fopen(tmp, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            *size = ftell(f);
            fclose(f);
            strcpy(dest, tmp);
            if (preloadMutexCreated) sys_lwmutex_unlock(&preloadMutex);
            return true;
        }
    }
    if (preloadMutexCreated) sys_lwmutex_unlock(&preloadMutex);
    return false;
}

void PreloadThreadFunc(uint64_t arg)
{
    (void)arg;
    if (!preloadedData) { preloadStatus = PRELOAD_IDLE; sys_ppu_thread_exit(0); }

    char folder[0x100];
    char stageID[0x10];
    strcpy(folder, stageList[preloadListID][preloadStageID].folder);
    strcpy(stageID, stageList[preloadListID][preloadStageID].id);
    strcpy(preloadedData->folder, folder);

    // Clean old files
    for (int i=0; i<PRELOAD_FILE_COUNT; i++) {
        preloadedData->files[i].fileName[0] = 0;
        preloadedData->files[i].size = 0;
        preloadedData->files[i].encrypted = false;
    }

    // List of files to preload
    char relPaths[PRELOAD_FILE_COUNT][0x100];
    for (int i=0; i<PRELOAD_FILE_COUNT; i++) relPaths[i][0] = 0;

    int pathIdx = 0;
    // Sonic 1 folder name mappings
    const char *folderMap[] = { "Zone01", "GHZ", "Zone02", "MZ", "Zone03", "SYZ", "Zone04", "LZ", "Zone05", "SLZ", "Zone06", "SBZ" };

    if (forceUseScripts) {
        sprintf(relPaths[pathIdx++], "Data/Stages/%s/StageConfig.bin", folder);
        sprintf(relPaths[pathIdx++], "Data/Stages/%s/128x128Tiles.bin", folder);
        sprintf(relPaths[pathIdx++], "Data/Stages/%s/CollisionMasks.bin", folder);
        sprintf(relPaths[pathIdx++], "Data/Stages/%s/Act%s.bin", folder, stageID);
        sprintf(relPaths[pathIdx++], "Data/Stages/%s/Backgrounds.bin", folder);
        sprintf(relPaths[pathIdx++], "Data/Stages/%s/16x16Tiles.gif", folder);
        sprintf(relPaths[pathIdx++], "Scripts/Global/StageSetup.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Players/PlayerObject.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/HUD.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/TitleCard.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/Ring.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/Monitor.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/ActFinish.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/RingSparkle.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/Explosion.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/AnimalPrison.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/SignPost.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/LampPost.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/DeathEvent.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/Invincibility.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/BlueShield.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Global/MusicEvent.txt");
        sprintf(relPaths[pathIdx++], "Scripts/Players/TailsObject.txt");

        // Try mapping ZoneXX to standard Sonic 1 directories for Setup.txt
        for (int m = 0; m < 12; m += 2) {
            if (strcasecmp(folder, folderMap[m]) == 0) {
                sprintf(relPaths[pathIdx++], "Scripts/%s/%sSetup.txt", folderMap[m+1], folderMap[m+1]);
                break;
            }
        }
    }
    else {
        sprintf(relPaths[0], "Data/Stages/%s/StageConfig.bin", folder);
        sprintf(relPaths[1], "Bytecode/%s.bin", folder);
        sprintf(relPaths[2], "Data/Stages/%s/128x128Tiles.bin", folder);
        sprintf(relPaths[3], "Data/Stages/%s/CollisionMasks.bin", folder);
        sprintf(relPaths[4], "Data/Stages/%s/Act%s.bin", folder, stageID);
        sprintf(relPaths[5], "Data/Stages/%s/Backgrounds.bin", folder);
        sprintf(relPaths[6], "Data/Stages/%s/16x16Tiles.gif", folder);
        sprintf(relPaths[7], "Bytecode/GlobalCode.bin");
        relPaths[8][0] = 0;
    }

    for (int i = 0; i < PRELOAD_FILE_COUNT; i++) {
        if (relPaths[i][0] == 0) continue;
        
        char fullPath[0x200];
        int packID = -1;
        int offset = 0;
        int size = 0;
        bool encrypted = false;

        if (ThreadedResolvePath(fullPath, relPaths[i], &packID, &offset, &size, &encrypted)) {
            PrintLog("Threaded Preload opening: %s (PackID: %d, Offset: %d, Encrypted: %s)", fullPath, packID, offset, encrypted ? "YES" : "NO");

            FILE *f = fopen(fullPath, "rb");
            if (f) {
                fseek(f, offset, SEEK_SET);
                
                if (size > 0 && size <= PRELOAD_BUF_SIZE) {
                    // Throttled read
                    size_t remaining = size;
                    byte *ptr = preloadedData->files[i].buffer;
                    while (remaining > 0) {
                        size_t toRead = remaining > 16384 ? 16384 : remaining;
                        fread(ptr, 1, toRead, f);
                        ptr += toRead;
                        remaining -= toRead;
                        sys_timer_usleep(5000); // 5ms delay per block
                    }

                    preloadedData->files[i].size = (int)size;
                    preloadedData->files[i].encrypted = encrypted;
                    strcpy(preloadedData->files[i].fileName, relPaths[i]);
                    PrintLog("Threaded Preload successful: %s (%d bytes)", relPaths[i], preloadedData->files[i].size);
                }
                fclose(f);
            } else {
                PrintLog("Threaded Preload FAILED to open: %s", fullPath);
            }
        } else {
            PrintLog("Threaded Preload FAILED to resolve: %s", relPaths[i]);
        }
        sys_timer_usleep(50000); // 50ms delay between files
    }

    PrintLog("Background Loading READY for %s - %s", stageListNames[preloadListID], stageList[preloadListID][preloadStageID].name);
    preloadStatus = PRELOAD_READY;
    sys_ppu_thread_exit(0);
}
#endif

byte* GetPreloadedFile(const char *fileName, int *size, bool *encrypted)
{
    if (preloadStatus != PRELOAD_READY || !preloadedData) return nullptr;
    
    // Normalize requested filename (case-insensitive and strip "Data/")
    const char *searchName = fileName;
    if (strncasecmp(fileName, "Data/", 5) == 0) searchName = fileName + 5;
    int reqLen = strlen(searchName);

    for (int i = 0; i < PRELOAD_FILE_COUNT; i++) {
        if (preloadedData->files[i].fileName[0] == 0) continue;
        
        // Normalize preloaded path (strip "Data/")
        const char *preName = preloadedData->files[i].fileName;
        if (strncasecmp(preName, "Data/", 5) == 0) preName += 5;
        int preLen = strlen(preName);

        if (preLen >= reqLen) {
            const char *preEnd = preName + (preLen - reqLen);
            // Case-insensitive match on the end of the path
            if (strcasecmp(preEnd, searchName) == 0) {
                // Ensure it's an exact file match (start of string or preceded by /)
                if (preLen == reqLen || (preEnd > preName && (preEnd[-1] == '/' || preEnd[-1] == '\\'))) {
                    if (size) *size = preloadedData->files[i].size;
                    if (encrypted) *encrypted = preloadedData->files[i].encrypted;
                    return preloadedData->files[i].buffer;
                }
            }
        }
    }
    return nullptr;
}

bool IsScenePreloaded(int listID, int stageID)
{
#if RETRO_PLATFORM == RETRO_PS3
    if (preloadThreadRunning) {
        if (preloadStatus == PRELOAD_LOADING) return false;
        uint64_t exit_code;
        sys_ppu_thread_join(preloadThread, &exit_code);
        preloadThreadRunning = false;
    }
#endif
    return (preloadStatus == PRELOAD_READY && preloadListID == listID && preloadStageID == stageID);
}

int creditsSequenceIdx = -1;
char lastFolder[0x100] = "";

void CheckStagePreload()
{
    if (preloadStatus == PRELOAD_LOADING) return;

    bool isTitle = false;
    const char *currentFolder = "";
    if (activeStageList == STAGELIST_PRESENTATION) {
        currentFolder = stageList[STAGELIST_PRESENTATION][stageListPosition].folder;
        if (strcasecmp(currentFolder, "Title") == 0 || strcasecmp(currentFolder, "TITLE") == 0 || strcasecmp(currentFolder, "PRESENTATION/TITLE") == 0 || strcasestr(currentFolder, "TITLE") != NULL) {
            isTitle = true;
        }
    } else if (activeStageList == STAGELIST_REGULAR) {
        currentFolder = stageList[STAGELIST_REGULAR][stageListPosition].folder;
    }

    if (isTitle) {
        if (preloadStatus == PRELOAD_IDLE) { StartStagePreload(STAGELIST_REGULAR, 0); }
        creditsSequenceIdx = -1;
    }
    else if (activeStageList == STAGELIST_REGULAR) {
        if (stageListPosition + 1 < stageListCount[STAGELIST_REGULAR]) {
            if (strcasecmp(stageList[STAGELIST_REGULAR][stageListPosition].folder, stageList[STAGELIST_REGULAR][stageListPosition + 1].folder) != 0) {
                if (preloadStatus == PRELOAD_IDLE) { StartStagePreload(STAGELIST_REGULAR, stageListPosition + 1); }
            }
        }
    }
    else if (activeStageList == STAGELIST_PRESENTATION && (strcasecmp(currentFolder, "Credits") == 0 || strcasecmp(currentFolder, "STAFF CREDITS") == 0)) {
        // Trigger preloading for the sequence during credits. 
        // We use preloadStatus == PRELOAD_IDLE as the primary trigger to advance.
        if (preloadStatus == PRELOAD_IDLE) {
            if (creditsSequenceIdx < 7) creditsSequenceIdx++;
            else creditsSequenceIdx = 0;

            int targetStage = -1;
            const char* targets[] = { "ZONE01", "ZONE02", "ZONE03", "ZONE04", "ZONE05", "ZONE06", "ZONE06", "ZONE01" };
            const int acts[] = { 1, 2, 3, 3, 3, 1, 2, 1 };
            if (creditsSequenceIdx >= 0 && creditsSequenceIdx < 8) {
                const char* targetFolder = targets[creditsSequenceIdx];
                int targetAct = acts[creditsSequenceIdx];
                for (int i = 0; i < stageListCount[STAGELIST_REGULAR]; i++) {
                    int actNum = 1; ConvertStringToInteger(stageList[STAGELIST_REGULAR][i].id, &actNum);
                    if (actNum == targetAct && (strcasecmp(stageList[STAGELIST_REGULAR][i].folder, targetFolder) == 0 || strcasestr(stageList[STAGELIST_REGULAR][i].name, targetFolder))) {
                         targetStage = i; break;
                    }
                }
                if (targetStage != -1) { StartStagePreload(STAGELIST_REGULAR, targetStage); }
            }
        }
    } else {
        creditsSequenceIdx = -1;
    }
    strcpy(lastFolder, currentFolder);
}
