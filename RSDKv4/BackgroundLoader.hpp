#ifndef BACKGROUNDLOADER_H
#define BACKGROUNDLOADER_H

#include "RetroEngine.hpp"

#define PRELOAD_FILE_COUNT (112)
#define PRELOAD_BUF_SIZE   (0x40000) // 256KB per buffer, 28MB total

enum PreloadStatus {
    PRELOAD_IDLE,
    PRELOAD_LOADING,
    PRELOAD_READY,
};

struct PreloadFile {
    char fileName[0x100];
    byte *buffer;
    int size;
    bool encrypted;
};

struct PreloadScene {
    int listID;
    int stageID;
    char folder[0x100];
    PreloadFile files[PRELOAD_FILE_COUNT];
};

extern int preloadStatus;
extern int preloadListID;
extern int preloadStageID;
extern int preloadDelayTimer;
extern volatile bool abortPreload;
extern PreloadScene *preloadedData;

void InitBackgroundLoader();
void StartStagePreload(int listID, int stageID);
void AbortPreload();
void CheckStagePreload();
bool IsScenePreloaded(int listID, int stageID);
byte* GetPreloadedFile(const char *fileName, int *size, bool *encrypted);

#endif // BACKGROUNDLOADER_H
