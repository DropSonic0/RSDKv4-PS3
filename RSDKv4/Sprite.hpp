#ifndef SPRITE_H
#define SPRITE_H

int AddGraphicsFile(const char *filePath);
void RemoveGraphicsFile(const char *filePath, int sheetID);

int LoadBMPFile(const char *filePath, byte sheetID);
int LoadGIFFile(const char *filePath, byte sheetID);
int LoadPVRFile(const char *filePath, byte sheetID);

extern byte *gifDataBuffer;
extern int gifDataPos;
void ReadGifPictureData(int width, int height, bool interlaced, byte *gfxData, int offset);

#endif // !SPRITE_H
