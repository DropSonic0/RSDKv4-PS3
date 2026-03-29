#include "RetroEngine.hpp"
#include "BackgroundLoader.hpp"

#if RETRO_PLATFORM == RETRO_PS3
#include <sys/ppu_thread.h>
#include <sys/timer.h>
#include <sys/synchronization.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef S_IFMT
#define S_IFMT 0170000
#endif
#ifndef S_IFREG
#define S_IFREG 0100000
#endif
#ifndef S_ISREG
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#endif

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
int preloadDelayTimer = 0;
volatile bool abortPreload = false;

PreloadScene *preloadedData = nullptr;

#if RETRO_PLATFORM == RETRO_PS3
// Mutex for background thread safety
static sys_lwmutex_t preloadMutex __attribute__((aligned(16)));
static bool preloadMutexCreated = false;

sys_ppu_thread_t preloadThread;
bool preloadThreadRunning = false;
static bool preloadThreadNeedsJoin = false;

void PreloadThreadFunc(uint64_t arg);

// Helper to resolve modded and packed paths using native FILE I/O only
static bool ThreadedResolvePath(char *dest, const char *filePath, int *packID, int *offset, int *size, bool *encrypted) {
    char filePathBuf[0x100];
    strncpy(filePathBuf, filePath, 0xFF);
    filePathBuf[0xFF] = 0;
    bool forceFolder = false;
    bool isAbsolute = false;
    
    *packID = -1;
    *offset = 0;
    *size = 0;
    *encrypted = false;

#if RETRO_USE_MOD_LOADER
    char pathLower[0x100];
    int len = (int)strlen(filePath);
    if (len > 0xFF) len = 0xFF;
    for (int i = 0; i < len; i++) pathLower[i] = tolower(filePath[i]);
    pathLower[len] = 0;

    if (preloadMutexCreated) sys_lwmutex_lock(&preloadMutex, 0);
    for (int m = 0; m < (int)modList.size(); m++) {
        if (modList[m].active) {
            std::map<std::string, std::string>::const_iterator it = modList[m].fileMap.find(pathLower);
            if (it != modList[m].fileMap.end()) {
                strncpy(filePathBuf, it->second.c_str(), 0xFF);
                filePathBuf[0xFF] = 0;
                forceFolder = true;
                if (filePathBuf[0] == '/') isAbsolute = true;
                break;
            }
        }
    }
    if (preloadMutexCreated) sys_lwmutex_unlock(&preloadMutex);
    if (abortPreload) return false;

    if (forceUseScripts && !forceFolder) {
        if (strncasecmp(filePathBuf, "Data/Scripts/", 13) == 0 && (strcasestr(filePathBuf, ".txt"))) {
            forceFolder = true;
            char scriptPath[0x100];
            strncpy(scriptPath, filePathBuf + 5, 0xFF); // Skip "Data/"
            scriptPath[0xFF] = 0;
    for (int i = 0; scriptPath[i]; i++) if (scriptPath[i] == '\\') scriptPath[i] = '/';
            strncpy(filePathBuf, scriptPath, 0xFF);
        }
    }
#endif

    int fileIndex = -1;
    if (!forceFolder) {
        char pathLower2[0x100];
        int len2 = (int)strlen(filePath);
        if (len2 > 0xFF) len2 = 0xFF;
        for (int i = 0; i < len2; i++) pathLower2[i] = tolower(filePath[i]);
        pathLower2[len2] = 0;
        
        if (preloadMutexCreated) sys_lwmutex_lock(&preloadMutex, 0);
        fileIndex = CheckFileInfo(pathLower2);
        if (fileIndex != -1) {
            *packID = rsdkContainer.files[fileIndex].packID;
            *offset = rsdkContainer.files[fileIndex].offset;
            *size = rsdkContainer.files[fileIndex].filesize;
            *encrypted = rsdkContainer.files[fileIndex].encrypted;
            strncpy(dest, rsdkContainer.packNames[*packID], 0x1FF);
        }
        if (preloadMutexCreated) sys_lwmutex_unlock(&preloadMutex);
        if (abortPreload) return false;
    }

    if (fileIndex != -1 && !forceFolder) {
        return true;
    } else {
        char tmp[0x200];
        if (isAbsolute) {
            strncpy(tmp, filePathBuf, sizeof(tmp)-1);
            tmp[sizeof(tmp)-1] = 0;
        } else {
            snprintf(tmp, sizeof(tmp), "%s%s", gamePath, filePathBuf);
        }
        
        // Use native fopen for thread safety outside mutex
        FILE *f = fopen(tmp, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            *size = (int)ftell(f);
            fclose(f);
            strncpy(dest, tmp, 0x1FF);
            return true;
        }
        
        if (!isAbsolute) {
            snprintf(tmp, sizeof(tmp), "%s%s", BASE_PATH, filePathBuf);
            f = fopen(tmp, "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                *size = (int)ftell(f);
                fclose(f);
                strncpy(dest, tmp, 0x1FF);
                return true;
            }
        }
    }
    return false;
}

// Standalone decryption implementation for background thread
struct DecryptionState {
    byte eStringPosA;
    byte eStringPosB;
    byte eStringNo;
    byte eNybbleSwap;
    byte encryptionStringA[0x10];
    byte encryptionStringB[0x10];
    int vFileSize;
};

static void BackgroundGenerateELoadKeys(uint key1, uint key2, byte *stringA, byte *stringB) {
    char buffer[0x20];
    uint hash[0x4];
    sprintf(buffer, "%u", key1);
    GenerateMD5FromString(buffer, (int)strlen(buffer), &hash[0], &hash[1], &hash[2], &hash[3]);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) stringA[i * 4 + j] = (hash[i] >> (8 * (j ^ 3))) & 0xFF;
    sprintf(buffer, "%u", key2);
    GenerateMD5FromString(buffer, (int)strlen(buffer), &hash[0], &hash[1], &hash[2], &hash[3]);
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) stringB[i * 4 + j] = (hash[i] >> (8 * (j ^ 3))) & 0xFF;
}

static inline int mulUnsignedHigh_BG(uint arg1, int arg2) { return (int)(((unsigned long long)arg1 * (unsigned long long)arg2) >> 32); }

static byte DecryptByte(DecryptionState *s, byte b) {
    const uint ENC_KEY_2 = 0x24924925;
    const uint ENC_KEY_1 = 0xAAAAAAAB;
    byte res = s->encryptionStringB[s->eStringPosB] ^ s->eStringNo ^ b;
    if (s->eNybbleSwap) res = ((res << 4) + (res >> 4)) & 0xFF;
    res ^= s->encryptionStringA[s->eStringPosA];
    s->eStringPosA++;
    s->eStringPosB++;
    if (s->eStringPosA <= 0x0F) {
        if (s->eStringPosB > 0x0C) {
            s->eStringPosB = 0;
            s->eNybbleSwap ^= 0x01;
        }
    } else if (s->eStringPosB <= 0x08) {
        s->eStringPosA = 0;
        s->eNybbleSwap ^= 0x01;
    } else {
        s->eStringNo = (s->eStringNo + 2) & 0x7F;
        int key1 = mulUnsignedHigh_BG(ENC_KEY_1, s->eStringNo);
        int key2 = mulUnsignedHigh_BG(ENC_KEY_2, s->eStringNo);
        int temp1 = key2 + (s->eStringNo - key2) / 2;
        int temp2 = (key1 >> 3) * 3;
        if (s->eNybbleSwap != 0) {
            s->eNybbleSwap = 0;
            s->eStringPosA = s->eStringNo - (temp1 >> 2) * 7;
            s->eStringPosB = s->eStringNo - (temp2 << 2) + 2;
        } else {
            s->eNybbleSwap = 1;
            s->eStringPosB = s->eStringNo - (temp1 >> 2) * 7;
            s->eStringPosA = s->eStringNo - (temp2 << 2) + 3;
        }
    }
    return res;
}

static void ReadDecrypted(void *dest, int size, FILE *f, DecryptionState *s, bool encrypted) {
    if (size <= 0) return;
    if (encrypted) {
        byte stackBuf[512];
        byte *buf = (size <= 512) ? stackBuf : (byte*)malloc(size);
        size_t result = fread(buf, 1, size, f);
        for (size_t i = 0; i < result; i++) {
            byte d = DecryptByte(s, buf[i]);
            if (dest) ((byte*)dest)[i] = d;
        }
        if (buf != stackBuf) free(buf);
    } else {
        if (dest) fread(dest, 1, size, f);
        else fseek(f, size, SEEK_CUR);
    }
}

static void AddRelPath(char relPaths[PRELOAD_FILE_COUNT][0x100], int *count, const char *path) {
    if (*count >= PRELOAD_FILE_COUNT || !path || !path[0]) return;

    char normalized[0x100];
    strncpy(normalized, path, 0xFF);
    normalized[0xFF] = 0;
    for (int i = 0; normalized[i]; i++) {
        if (normalized[i] == '\\') normalized[i] = '/';
    }

    for (int i = 0; i < *count; i++) {
        if (strcasecmp(relPaths[i], normalized) == 0) return;
    }
    strncpy(relPaths[(*count)++], normalized, 0xFF);
}

static void GetScriptsFromConfig(const char *relPath, char scriptPaths[PRELOAD_FILE_COUNT][0x100], int *count) {
    char fullPath[0x200];
    int packID = -1;
    int offset = 0;
    int size = 0;
    bool encrypted = false;

    if (!ThreadedResolvePath(fullPath, relPath, &packID, &offset, &size, &encrypted)) return;

    FILE *f = fopen(fullPath, "rb");
    if (!f) return;
    fseek(f, offset, SEEK_SET);

    DecryptionState s;
    if (encrypted) {
        s.vFileSize = size;
        BackgroundGenerateELoadKeys(size, (size >> 1) + 1, s.encryptionStringA, s.encryptionStringB);
        s.eStringNo = (size & 0x1FC) >> 2;
        s.eStringPosA = 0;
        s.eStringPosB = 8;
        s.eNybbleSwap = 0;
    }

    byte buf[0x200];
    if (retro_strcasestr(relPath, "GameConfig.bin")) {
        ReadDecrypted(buf, 1, f, &s, encrypted); // Title
        ReadDecrypted(NULL, buf[0], f, &s, encrypted);
        ReadDecrypted(buf, 1, f, &s, encrypted); // Description
        ReadDecrypted(NULL, buf[0], f, &s, encrypted);
        ReadDecrypted(NULL, 0x60 * 3, f, &s, encrypted); // Palette
        
        ReadDecrypted(buf, 1, f, &s, encrypted); // Object count
        int objCount = buf[0];
        if (objCount > OBJECT_COUNT) objCount = OBJECT_COUNT;
        
        char (*objNames)[0x100] = (char (*)[0x100])malloc(objCount * 0x100);
        if (!objNames) { fclose(f); return; }

        for (int i = 0; i < objCount; i++) {
            if (abortPreload) { free(objNames); fclose(f); return; }
            ReadDecrypted(buf, 1, f, &s, encrypted);
            int len = buf[0];
            if (len > 0xFF) len = 0xFF;
            ReadDecrypted(objNames[i], len, f, &s, encrypted);
            objNames[i][len] = 0;
        }

        // Read script paths (TxtScripts mode second list)
        for (int i = 0; i < objCount; i++) {
            if (abortPreload) { free(objNames); fclose(f); return; }
            ReadDecrypted(buf, 1, f, &s, encrypted);
            int len = buf[0];
            if (len > 0) {
                char scriptName[0x100];
                if (len > 0xFF) len = 0xFF;
                ReadDecrypted(scriptName, len, f, &s, encrypted);
                scriptName[len] = 0;

                char scriptPath[0x100];
                if (retro_strcasestr(scriptName, ".txt"))
                    snprintf(scriptPath, sizeof(scriptPath), "Data/Scripts/%s", scriptName);
                else
                    snprintf(scriptPath, sizeof(scriptPath), "Data/Scripts/%s.txt", scriptName);
                AddRelPath(scriptPaths, count, scriptPath);
            }
            else {
                // Fallback for Global scripts - they usually have explicit paths, but just in case
                char scriptPath[0x100];
                snprintf(scriptPath, sizeof(scriptPath), "Data/Scripts/Global/%s.txt", objNames[i]);
                AddRelPath(scriptPaths, count, scriptPath);
            }
        }
        free(objNames);
        // Read SFX Paths
        ReadDecrypted(buf, 1, f, &s, encrypted);
        int sfxCount = buf[0];
        // Skip SFX Names
        for (int i = 0; i < sfxCount; i++) {
            ReadDecrypted(buf, 1, f, &s, encrypted);
            ReadDecrypted(NULL, buf[0], f, &s, encrypted);
        }
        // Skip SFX Paths
        for (int i = 0; i < sfxCount; i++) {
            ReadDecrypted(buf, 1, f, &s, encrypted);
            ReadDecrypted(NULL, buf[0], f, &s, encrypted);
        }
        // Skip Global Variables
        ReadDecrypted(buf, 1, f, &s, encrypted);
        int varCount = buf[0];
        for (int i = 0; i < varCount; i++) {
            ReadDecrypted(buf, 1, f, &s, encrypted);
            ReadDecrypted(NULL, buf[0], f, &s, encrypted);
            ReadDecrypted(NULL, 4, f, &s, encrypted);
        }
    } else {
        ReadDecrypted(buf, 1, f, &s, encrypted); // Load Globals
        // StageConfig palette uses 0x20 * 3 bytes (indices 0x60-0x7F)
        ReadDecrypted(NULL, 0x20 * 3, f, &s, encrypted); // Palette
        ReadDecrypted(buf, 1, f, &s, encrypted); // SFX count
        int sfxCount = buf[0];
        // Skip SFX Names
        for (int i = 0; i < sfxCount; i++) {
            ReadDecrypted(buf, 1, f, &s, encrypted);
            ReadDecrypted(NULL, buf[0], f, &s, encrypted);
        }
        // Skip SFX Paths
        for (int i = 0; i < sfxCount; i++) {
            ReadDecrypted(buf, 1, f, &s, encrypted);
            ReadDecrypted(NULL, buf[0], f, &s, encrypted);
        }
        
        ReadDecrypted(buf, 1, f, &s, encrypted); // Object count
        int objCount = buf[0];
        if (objCount > OBJECT_COUNT) objCount = OBJECT_COUNT;

        char (*objNames)[0x100] = (char (*)[0x100])malloc(objCount * 0x100);
        if (!objNames) { fclose(f); return; }

        for (int i = 0; i < objCount; i++) {
            if (abortPreload) { free(objNames); fclose(f); return; }
            ReadDecrypted(buf, 1, f, &s, encrypted);
            int len = buf[0];
            if (len > 0xFF) len = 0xFF;
            ReadDecrypted(objNames[i], len, f, &s, encrypted);
            objNames[i][len] = 0;
        }

        char folder[0x80];
        folder[0] = 0;
        const char *p1 = retro_strcasestr(relPath, "Stages/");
        if (p1) {
            p1 += 7;
            const char *p2 = strchr(p1, '/');
            if (p2) {
                int flen = p2 - p1;
                if (flen > 0x7F) flen = 0x7F;
                strncpy(folder, p1, flen);
                folder[flen] = 0;
            }
        }

        // Read script paths (TxtScripts mode second list)
        for (int i = 0; i < objCount; i++) {
            if (abortPreload) { free(objNames); fclose(f); return; }
            ReadDecrypted(buf, 1, f, &s, encrypted);
            int len = buf[0];
            if (len > 0) {
                char scriptName[0x100];
                if (len > 0xFF) len = 0xFF;
                ReadDecrypted(scriptName, len, f, &s, encrypted);
                scriptName[len] = 0;

                char scriptPath[0x100];
                if (retro_strcasestr(scriptName, ".txt"))
                    snprintf(scriptPath, sizeof(scriptPath), "Data/Scripts/%s", scriptName);
                else
                    snprintf(scriptPath, sizeof(scriptPath), "Data/Scripts/%s.txt", scriptName);
                AddRelPath(scriptPaths, count, scriptPath);
            }
            else if (folder[0]) {
                // Fallback for Stage scripts: use folder and object name convention
                char scriptPath[0x100];
                snprintf(scriptPath, sizeof(scriptPath), "Data/Scripts/%s/%s.txt", folder, objNames[i]);
                AddRelPath(scriptPaths, count, scriptPath);
            }
        }
        free(objNames);
    }
    fclose(f);
}

static void GetGlobalScripts(char scriptPaths[PRELOAD_FILE_COUNT][0x100], int *count) {
    char globalDir[0x200];
    snprintf(globalDir, sizeof(globalDir), "%sScripts/Global", gamePath);

    DIR *dir = opendir(globalDir);
    if (dir) {
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (abortPreload) break;
            if (entry->d_name[0] == '.') continue;
            
            char fullPath[0x400];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", globalDir, entry->d_name);
            struct stat st_buf;
            if (::stat(fullPath, &st_buf) == 0 && S_ISREG(st_buf.st_mode)) {
                if (retro_strcasestr(entry->d_name, ".txt")) {
                    char relPath[0x100];
                    snprintf(relPath, sizeof(relPath), "Data/Scripts/Global/%s", entry->d_name);
                    AddRelPath(scriptPaths, count, relPath);
                }
            }
        }
        closedir(dir);
    }

#if RETRO_USE_MOD_LOADER
    if (preloadMutexCreated) sys_lwmutex_lock(&preloadMutex, 0);
    for (int m = 0; m < (int)modList.size(); m++) {
        if (modList[m].active) {
            for (std::map<std::string, std::string>::const_iterator it = modList[m].fileMap.begin(); it != modList[m].fileMap.end(); ++it) {
                if (abortPreload) break;
                const char *p = it->first.c_str();
                if (retro_strcasestr(p, "scripts/global/") && (retro_strcasestr(p, ".txt"))) {
                    const char *match = retro_strcasestr(p, "scripts/global/");
                    char normalizedPath[0x100];
                    snprintf(normalizedPath, sizeof(normalizedPath), "Data/%s", match);
                    normalizedPath[0] = 'D'; normalizedPath[5] = 'S'; normalizedPath[13] = 'G';
                    AddRelPath(scriptPaths, count, normalizedPath);
                }
            }
        }
        if (abortPreload) break;
    }
    if (preloadMutexCreated) sys_lwmutex_unlock(&preloadMutex);
#endif
}
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
    preloadDelayTimer = 0;
}

void StartStagePreload(int listID, int stageID)
{
    if (preloadStatus == PRELOAD_LOADING || preloadThreadRunning) return;
    
    if (preloadStatus == PRELOAD_READY && preloadListID == listID && preloadStageID == stageID) return;

#if RETRO_PLATFORM == RETRO_PS3
    if (preloadThreadNeedsJoin) {
        AbortPreload();
    }
#endif

    PrintLog("Background Loading START for %s - %s", stageListNames[listID], stageList[listID][stageID].name);

    preloadListID = listID;
    preloadStageID = stageID;
    preloadStatus = PRELOAD_LOADING;
    abortPreload = false;

#if RETRO_PLATFORM == RETRO_PS3
    preloadThreadRunning = true;
    preloadThreadNeedsJoin = true;
    sys_ppu_thread_create(&preloadThread, PreloadThreadFunc, 0, 1001, 0x10000, SYS_PPU_THREAD_CREATE_JOINABLE, "PreloadThread");
#else
    preloadStatus = PRELOAD_IDLE;
#endif
}

void AbortPreload()
{
    if (preloadStatus == PRELOAD_IDLE && !preloadThreadRunning) return;
    
    abortPreload = true;
    PrintLog("Background Loading ABORT requested");

#if RETRO_PLATFORM == RETRO_PS3
    if (preloadThreadNeedsJoin) {
        uint64_t exit_code;
        sys_ppu_thread_join(preloadThread, &exit_code);
        preloadThreadNeedsJoin = false;
        preloadThreadRunning = false;
    }
#endif
    preloadStatus = PRELOAD_IDLE;
}

#if RETRO_PLATFORM == RETRO_PS3
void PreloadThreadFunc(uint64_t arg)
{
    (void)arg;
    if (!preloadedData) { preloadStatus = PRELOAD_IDLE; preloadThreadRunning = false; sys_ppu_thread_exit(0); }

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
    // Use heap to avoid stack overflow with reduced thread stack size
    typedef char PathBuf[0x100];
    PathBuf *relPaths = (PathBuf*)malloc(sizeof(PathBuf) * PRELOAD_FILE_COUNT);
    if (!relPaths) { preloadStatus = PRELOAD_IDLE; preloadThreadRunning = false; sys_ppu_thread_exit(0); }
    for (int i=0; i<PRELOAD_FILE_COUNT; i++) relPaths[i][0] = 0;

    int pathIdx = 0;
    char modHash[33];
    GetModHash(modHash);

    // Common files
    snprintf(relPaths[pathIdx++], 0x100, "Data/Stages/%s/StageConfig.bin", folder);
    snprintf(relPaths[pathIdx++], 0x100, "Data/Stages/%s/128x128Tiles.bin", folder);
    snprintf(relPaths[pathIdx++], 0x100, "Data/Stages/%s/CollisionMasks.bin", folder);
    snprintf(relPaths[pathIdx++], 0x100, "Data/Stages/%s/Act%s.bin", folder, stageID);
    snprintf(relPaths[pathIdx++], 0x100, "Data/Stages/%s/Backgrounds.bin", folder);
    snprintf(relPaths[pathIdx++], 0x100, "Data/Stages/%s/16x16Tiles.gif", folder);

    // Check for Stage Bytecode
    char stageBytecodePath[0x100];
    if (modHash[0])
        snprintf(stageBytecodePath, sizeof(stageBytecodePath), "Bytecode/%s_%s.bin", folder, modHash);
    else
        snprintf(stageBytecodePath, sizeof(stageBytecodePath), "Bytecode/%s_Retail.bin", folder);

    char fullPath_s[0x200];
    snprintf(fullPath_s, sizeof(fullPath_s), "%s%s", gamePath, stageBytecodePath);
    FILE *f_test_s = fopen(fullPath_s, "rb");
    bool sBCExists = false;
    if (f_test_s) {
        fclose(f_test_s);
        sBCExists = true;
    }

    if (forceUseScripts && !sBCExists) {
        // Preload scripts from StageConfig.bin (Stage-specific scripts)
        char stageConfigPath[0x100];
        snprintf(stageConfigPath, sizeof(stageConfigPath), "Data/Stages/%s/StageConfig.bin", folder);
        GetScriptsFromConfig(stageConfigPath, relPaths, &pathIdx);
    }
    else {
        snprintf(relPaths[pathIdx++], 0x100, "%s", stageBytecodePath);
    }

    // Check for Global Bytecode
    char globalBytecodePath[0x100];
    if (modHash[0])
        snprintf(globalBytecodePath, sizeof(globalBytecodePath), "Bytecode/GlobalCode_%s.bin", modHash);
    else
        snprintf(globalBytecodePath, sizeof(globalBytecodePath), "Bytecode/GlobalCode_Retail.bin");

    char fullPath_g[0x200];
    snprintf(fullPath_g, sizeof(fullPath_g), "%s%s", gamePath, globalBytecodePath);
    FILE *f_test_g = fopen(fullPath_g, "rb");
    bool gBCExists = false;
    if (f_test_g) {
        fclose(f_test_g);
        gBCExists = true;
    }

    if (forceUseScripts && !gBCExists) {
        // Preload all global scripts
        GetGlobalScripts(relPaths, &pathIdx);
        // Preload scripts from GameConfig.bin (Global scripts)
        GetScriptsFromConfig("Data/Game/GameConfig.bin", relPaths, &pathIdx);
    }
    else {
        snprintf(relPaths[pathIdx++], 0x100, "%s", globalBytecodePath);
    }

    relPaths[pathIdx][0] = 0;

    for (int i = 0; i < PRELOAD_FILE_COUNT; i++) {
        if (abortPreload) break;
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
                        if (abortPreload) break;
                        size_t toRead = remaining > 16384 ? 16384 : remaining;
                        fread(ptr, 1, toRead, f);
                        ptr += toRead;
                        remaining -= toRead;
                        // No sleep during transition to keep main thread fast
                    }

                    if (!abortPreload) {
                        preloadedData->files[i].size = (int)size;
                        preloadedData->files[i].encrypted = encrypted;
                        strncpy(preloadedData->files[i].fileName, relPaths[i], 0xFF);
                        preloadedData->files[i].fileName[0xFF] = 0;
                        PrintLog("Threaded Preload successful: %s (%d bytes)", relPaths[i], preloadedData->files[i].size);
                    } else {
                        preloadedData->files[i].fileName[0] = 0;
                        preloadedData->files[i].size = 0;
                    }
                }
                fclose(f);
            } else {
                PrintLog("Threaded Preload FAILED to open: %s", fullPath);
            }
        } else {
            PrintLog("Threaded Preload FAILED to resolve: %s", relPaths[i]);
        }
    }

    if (abortPreload) {
        PrintLog("Background Loading ABORTED for %s - %s (Partial ready)", stageListNames[preloadListID], stageList[preloadListID][preloadStageID].name);
    } else {
        PrintLog("Background Loading READY for %s - %s", stageListNames[preloadListID], stageList[preloadListID][preloadStageID].name);
    }
    preloadStatus = PRELOAD_READY;
    
    free(relPaths);
    preloadThreadRunning = false;
    sys_ppu_thread_exit(0);
}
#endif

byte* GetPreloadedFile(const char *fileName, int *size, bool *encrypted)
{
    if (!preloadedData) return nullptr;

    char searchNameBuf[0x100];
    strncpy(searchNameBuf, fileName, 0xFF);
    searchNameBuf[0xFF] = 0;
    for (int i = 0; searchNameBuf[i]; i++) {
        if (searchNameBuf[i] == '\\') searchNameBuf[i] = '/';
    }
    
    // Normalize requested filename (case-insensitive and strip "Data/")
    const char *searchName = searchNameBuf;
    if (strncasecmp(searchName, "Data/", 5) == 0) searchName += 5;
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
                if (preLen == reqLen || (preEnd > preName && (preEnd[-1] == '/'))) {
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
        return false;
    }
#endif
    return (preloadStatus == PRELOAD_READY && preloadListID == listID && preloadStageID == stageID);
}

int creditsSequenceIdx = -1;
char lastFolder[0x100] = "";

void CheckStagePreload()
{
    if (preloadStatus == PRELOAD_LOADING || Engine.gameMode == ENGINE_DEVMENU) return;

    if (preloadDelayTimer > 0) {
        preloadDelayTimer--;
        return;
    }

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
