#include "RetroEngine.hpp"

ushort *strPressStart       = NULL;
ushort *strTouchToStart     = NULL;
ushort *strStartGame        = NULL;
ushort *strTimeAttack       = NULL;
ushort *strAchievements     = NULL;
ushort *strLeaderboards     = NULL;
ushort *strHelpAndOptions   = NULL;
ushort *strSoundTest        = NULL;
ushort *str2PlayerVS        = NULL;
ushort *strSaveSelect       = NULL;
ushort *strPlayerSelect     = NULL;
ushort *strNoSave           = NULL;
ushort *strNewGame          = NULL;
ushort *strDelete           = NULL;
ushort *strDeleteMessage    = NULL;
ushort *strYes              = NULL;
ushort *strNo               = NULL;
ushort *strSonic            = NULL;
ushort *strTails            = NULL;
ushort *strKnuckles         = NULL;
ushort *strPause            = NULL;
ushort *strContinue         = NULL;
ushort *strRestart          = NULL;
ushort *strExit             = NULL;
ushort *strDevMenu          = NULL;
ushort *strRestartMessage   = NULL;
ushort *strExitMessage      = NULL;
ushort *strNSRestartMessage = NULL;
ushort *strNSExitMessage    = NULL;
ushort *strExitGame         = NULL;
ushort *strNetworkMessage   = NULL;
ushort *strStageList[16];
ushort *strSaveStageList[32];
ushort *strNewBestTime   = NULL;
ushort *strRecords       = NULL;
ushort *strNextAct       = NULL;
ushort *strPlay          = NULL;
ushort *strTotalTime     = NULL;
ushort *strInstructions  = NULL;
ushort *strSettings      = NULL;
ushort *strScreen        = NULL;
ushort *strStaffCredits  = NULL;
ushort *strAbout         = NULL;
ushort *strMusic         = NULL;
ushort *strSoundFX       = NULL;
ushort *strSpindash      = NULL;
ushort *strBoxArt        = NULL;
ushort *strControls      = NULL;
ushort *strFilters       = NULL;
ushort *strScalingMode   = NULL;
ushort *strAspectRatio   = NULL;
ushort *strManual        = NULL;
ushort *strOn            = NULL;
ushort *strOff           = NULL;
ushort *strCustomizeDPad = NULL;
ushort *strDPadSize      = NULL;
ushort *strDPadOpacity   = NULL;
ushort *strHelpText1     = NULL;
ushort *strHelpText2     = NULL;
ushort *strHelpText3     = NULL;
ushort *strHelpText4     = NULL;
ushort *strHelpText5     = NULL;
ushort *strVersionName   = NULL;
ushort *strPrivacy       = NULL;
ushort *strTerms         = NULL;

int stageStrCount = 0;

ushort stringStorage[STRSTORAGE_SIZE * STRING_SIZE];
int stringStorePos = 0;

int creditsListSize = 0;
ushort *strCreditsList[CREDITS_LIST_COUNT];
byte creditsType[CREDITS_LIST_COUNT];
float creditsAdvanceY[CREDITS_LIST_COUNT];

#include <string.h>

int FindStringToken(const char *string, const char *token, char stopID)
{
    int tokenCharID  = 0;
    bool tokenMatch  = true;
    int stringCharID = 0;
    int foundTokenID = 0;

    while (string[stringCharID]) {
        tokenCharID = 0;
        tokenMatch  = true;
        while (token[tokenCharID]) {
            if (!string[tokenCharID + stringCharID])
                return -1;

            if (string[tokenCharID + stringCharID] != token[tokenCharID])
                tokenMatch = false;

            ++tokenCharID;
        }
        if (tokenMatch && ++foundTokenID == stopID)
            return stringCharID;

        ++stringCharID;
    }
    return -1;
}

int FindLastStringToken(const char *string, const char *token)
{
    int tokenCharID  = 0;
    bool tokenMatch  = true;
    int stringCharID = 0;
    int foundTokenID = 0;
    int lastResult = -1;

    while (string[stringCharID]) {
        tokenCharID = 0;
        tokenMatch  = true;
        while (token[tokenCharID]) {
            if (!string[tokenCharID + stringCharID])
                return lastResult;

            if (string[tokenCharID + stringCharID] != token[tokenCharID])
                tokenMatch = false;

            ++tokenCharID;
        }
        if (tokenMatch)
            lastResult = stringCharID;

        ++stringCharID;
    }
    return lastResult;
}

int FindStringTokenUnicode(const ushort *string, const ushort *token, char stopID)
{
    int tokenCharID  = 0;
    bool tokenMatch  = true;
    int stringCharID = 0;
    int foundTokenID = 0;

    while (string[stringCharID]) {
        tokenCharID = 0;
        tokenMatch  = true;
        while (token[tokenCharID]) {
            if (!string[tokenCharID + stringCharID])
                return -1;

            if (string[tokenCharID + stringCharID] != token[tokenCharID])
                tokenMatch = false;

            ++tokenCharID;
        }
        if (tokenMatch && ++foundTokenID == stopID)
            return stringCharID;

        ++stringCharID;
    }
    return -1;
}

void ConvertIntegerToString(char *text, int value) { sprintf(text, "%d", value); }

void GenerateMD5FromString(const char *string, int len, uint *hash0, uint *hash1, uint *hash2, uint *hash3)
{
    #define ROTATE_LEFT(x, n) (((x) << (n)) | ((x) >> (32-(n))))

    static const uint K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a,
        0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340,
        0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8,
        0x676f02d9, 0x8d2a4c8a, 0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa,
        0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92,
        0xffeff47d, 0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

    static const uint S[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20, 5, 9,  14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};

    byte hashStream[0x400];

    *hash0 = 0x67452301;
    *hash1 = 0xefcdab89;
    *hash2 = 0x98badcfe;
    *hash3 = 0x10325476;
    memset(hashStream, 0, 0x400);

    uint length_bits = len * 8;
    int padded_length = length_bits + 1;
    while (padded_length % 512 != 448) {
        padded_length++;
    }

    memcpy(hashStream, string, len);

    padded_length /= 8;
    // Padding (only 0x80 is needed since memset(0) has been called on hashStream)
    hashStream[len] = 0x80;
    // Write length in little endian order
    for (int p = 0; p < 4; ++p) hashStream[padded_length + p] = (length_bits >> (8 * p));

    // Process blocks
    for (int block = 0; block < padded_length; block += 64) {
        uint A = *hash0;
        uint B = *hash1;
        uint C = *hash2;
        uint D = *hash3;
        for (int i = 0; i < 64; ++i) {
            uint res, idx;
            if (i < 0x10) {
                res = (B & C) | ((~B) & D);
                idx = i;
            } else if (i < 0x20) {
                res = (D & B) | ((~D) & C);
                idx = ((5 * i) + 1) % 16;
            } else if (i < 0x30) {
                res = B ^ C ^ D;
                idx = ((3 * i) + 5) % 16;
            } else {
                res = C ^ (B | (~D));
                idx = (7 * i) % 16;
            }
            uint streamVal = 0;
            // Convert to little endian
            for (int p = 0; p < 4; ++p) streamVal |= (hashStream[block + (idx * 4) + p] & 0xFF) << (8 * p);
            uint temp = D;
            D = C;
            C = B;
            B = B + ROTATE_LEFT((A + res + K[i] + streamVal), S[i]);
            A = temp;
        }
        *hash0 += A;
        *hash1 += B;
        *hash2 += C;
        *hash3 += D;
    }
}

void InitLocalizedStrings()
{
    memset(stringStorage, 0, STRING_SIZE * STRSTORAGE_SIZE * sizeof(ushort));
    stringStorePos = 0;

    FileInfo info;
    FileInfo *infoPtr = NULL;
    if (LoadFile("Data/Game/StringList.txt", &info)) {
        infoPtr = &info;
    }

    char langStr[0x4];
    switch (Engine.language) {
        case RETRO_EN: StrCopy(langStr, "en"); break;
        case RETRO_FR: StrCopy(langStr, "fr"); break;
        case RETRO_IT: StrCopy(langStr, "it"); break;
        case RETRO_DE: StrCopy(langStr, "de"); break;
        case RETRO_ES: StrCopy(langStr, "es"); break;
        case RETRO_JP: StrCopy(langStr, "ja"); break;
        case RETRO_PT: StrCopy(langStr, "pt"); break;
        case RETRO_RU: StrCopy(langStr, "ru"); break;
        case RETRO_KO: StrCopy(langStr, "ko"); break;
        case RETRO_ZH: StrCopy(langStr, "zh"); break;
        case RETRO_ZS: StrCopy(langStr, "zs"); break;
        default: break;
    }
    strPressStart     = ReadLocalizedString("PressStart", langStr, "Data/Game/StringList.txt", infoPtr);
    strTouchToStart   = ReadLocalizedString("TouchToStart", langStr, "Data/Game/StringList.txt", infoPtr);
    strStartGame      = ReadLocalizedString("StartGame", langStr, "Data/Game/StringList.txt", infoPtr);
    strTimeAttack     = ReadLocalizedString("TimeAttack", langStr, "Data/Game/StringList.txt", infoPtr);
    strAchievements   = ReadLocalizedString("Achievements", langStr, "Data/Game/StringList.txt", infoPtr);
    strLeaderboards   = ReadLocalizedString("Leaderboards", langStr, "Data/Game/StringList.txt", infoPtr);
    strHelpAndOptions = ReadLocalizedString("HelpAndOptions", langStr, "Data/Game/StringList.txt", infoPtr);

    // SoundTest & StageTest, both unused
    strSoundTest = ReadLocalizedString("SoundTest", langStr, "Data/Game/StringList.txt", infoPtr);
    // strStageTest      = ReadLocalizedString("StageTest", langStr, "Data/Game/StringList.txt", infoPtr);

    str2PlayerVS      = ReadLocalizedString("TwoPlayerVS", langStr, "Data/Game/StringList.txt", infoPtr);
    strSaveSelect     = ReadLocalizedString("SaveSelect", langStr, "Data/Game/StringList.txt", infoPtr);
    strPlayerSelect   = ReadLocalizedString("PlayerSelect", langStr, "Data/Game/StringList.txt", infoPtr);
    strNoSave         = ReadLocalizedString("NoSave", langStr, "Data/Game/StringList.txt", infoPtr);
    strNewGame        = ReadLocalizedString("NewGame", langStr, "Data/Game/StringList.txt", infoPtr);
    strDelete         = ReadLocalizedString("Delete", langStr, "Data/Game/StringList.txt", infoPtr);
    strDeleteMessage  = ReadLocalizedString("DeleteSavedGame", langStr, "Data/Game/StringList.txt", infoPtr);
    strYes            = ReadLocalizedString("Yes", langStr, "Data/Game/StringList.txt", infoPtr);
    strNo             = ReadLocalizedString("No", langStr, "Data/Game/StringList.txt", infoPtr);
    strSonic          = ReadLocalizedString("Sonic", langStr, "Data/Game/StringList.txt", infoPtr);
    strTails          = ReadLocalizedString("Tails", langStr, "Data/Game/StringList.txt", infoPtr);
    strKnuckles       = ReadLocalizedString("Knuckles", langStr, "Data/Game/StringList.txt", infoPtr);
    strPause          = ReadLocalizedString("Pause", langStr, "Data/Game/StringList.txt", infoPtr);
    strContinue       = ReadLocalizedString("Continue", langStr, "Data/Game/StringList.txt", infoPtr);
    strRestart        = ReadLocalizedString("Restart", langStr, "Data/Game/StringList.txt", infoPtr);
    strExit           = ReadLocalizedString("Exit", langStr, "Data/Game/StringList.txt", infoPtr);
    strDevMenu        = ReadLocalizedString("DevMenu", "en", "Data/Game/StringList.txt", infoPtr);
    strRestartMessage = ReadLocalizedString("RestartMessage", langStr, "Data/Game/StringList.txt", infoPtr);
    strExitMessage    = ReadLocalizedString("ExitMessage", langStr, "Data/Game/StringList.txt", infoPtr);
    if (Engine.language == RETRO_JP) {
        strNSRestartMessage = ReadLocalizedString("NSRestartMessage", "ja", "Data/Game/StringList.txt", infoPtr);
        strNSExitMessage    = ReadLocalizedString("NSExitMessage", "ja", "Data/Game/StringList.txt", infoPtr);
    }
    else {
        strNSRestartMessage = ReadLocalizedString("RestartMessage", langStr, "Data/Game/StringList.txt", infoPtr);
        strNSExitMessage    = ReadLocalizedString("ExitMessage", langStr, "Data/Game/StringList.txt", infoPtr);
    }
    strExitGame       = ReadLocalizedString("ExitGame", langStr, "Data/Game/StringList.txt", infoPtr);
    strNetworkMessage = ReadLocalizedString("NetworkMessage", langStr, "Data/Game/StringList.txt", infoPtr);
    for (int i = 0; i < 16; ++i) {
        char buffer[0x10];
        sprintf(buffer, "StageName%d", i + 1);

        strStageList[i] = ReadLocalizedString(buffer, "en", "Data/Game/StringList.txt", infoPtr);
    }

    stageStrCount = 0;
    for (int i = 0; i < 32; ++i) {
        char buffer[0x20];
        sprintf(buffer, "SaveStageName%d", i + 1);

        strSaveStageList[i] = ReadLocalizedString(buffer, "en", "Data/Game/StringList.txt", infoPtr);
        if (!strSaveStageList[i])
            break;
        stageStrCount++;
    }
    strNewBestTime   = ReadLocalizedString("NewBestTime", langStr, "Data/Game/StringList.txt", infoPtr);
    strRecords       = ReadLocalizedString("Records", langStr, "Data/Game/StringList.txt", infoPtr);
    strNextAct       = ReadLocalizedString("NextAct", langStr, "Data/Game/StringList.txt", infoPtr);
    strPlay          = ReadLocalizedString("Play", langStr, "Data/Game/StringList.txt", infoPtr);
    strTotalTime     = ReadLocalizedString("TotalTime", langStr, "Data/Game/StringList.txt", infoPtr);
    strInstructions  = ReadLocalizedString("Instructions", langStr, "Data/Game/StringList.txt", infoPtr);
    strSettings      = ReadLocalizedString("Settings", langStr, "Data/Game/StringList.txt", infoPtr);
    strScreen        = ReadLocalizedString("Screen", langStr, "Data/Game/StringList.txt", infoPtr);
    strStaffCredits  = ReadLocalizedString("StaffCredits", langStr, "Data/Game/StringList.txt", infoPtr);
    strAbout         = ReadLocalizedString("About", langStr, "Data/Game/StringList.txt", infoPtr);
    strMusic         = ReadLocalizedString("Music", langStr, "Data/Game/StringList.txt", infoPtr);
    strSoundFX       = ReadLocalizedString("SoundFX", langStr, "Data/Game/StringList.txt", infoPtr);
    strSpindash      = ReadLocalizedString("SpinDash", langStr, "Data/Game/StringList.txt", infoPtr);
    strBoxArt        = ReadLocalizedString("BoxArt", langStr, "Data/Game/StringList.txt", infoPtr);
    strControls      = ReadLocalizedString("Controls", langStr, "Data/Game/StringList.txt", infoPtr);
    strFilters       = ReadLocalizedString("Filters", langStr, "Data/Game/StringList.txt", infoPtr);
    strScalingMode   = ReadLocalizedString("ScalingMode", langStr, "Data/Game/StringList.txt", infoPtr);
    strAspectRatio   = ReadLocalizedString("AspectRatio", langStr, "Data/Game/StringList.txt", infoPtr);
    strManual        = ReadLocalizedString("Manual", langStr, "Data/Game/StringList.txt", infoPtr);
    strOn            = ReadLocalizedString("On", langStr, "Data/Game/StringList.txt", infoPtr);
    strOff           = ReadLocalizedString("Off", langStr, "Data/Game/StringList.txt", infoPtr);
    strCustomizeDPad = ReadLocalizedString("CustomizeDPad", langStr, "Data/Game/StringList.txt", infoPtr);
    strDPadSize      = ReadLocalizedString("DPadSize", langStr, "Data/Game/StringList.txt", infoPtr);
    strDPadOpacity   = ReadLocalizedString("DPadOpacity", langStr, "Data/Game/StringList.txt", infoPtr);
    strHelpText1     = ReadLocalizedString("HelpText1", langStr, "Data/Game/StringList.txt", infoPtr);
    strHelpText2     = ReadLocalizedString("HelpText2", langStr, "Data/Game/StringList.txt", infoPtr);
    strHelpText3     = ReadLocalizedString("HelpText3", langStr, "Data/Game/StringList.txt", infoPtr);
    strHelpText4     = ReadLocalizedString("HelpText4", langStr, "Data/Game/StringList.txt", infoPtr);
    strHelpText5     = ReadLocalizedString("HelpText5", langStr, "Data/Game/StringList.txt", infoPtr);
    strVersionName   = ReadLocalizedString("Version", langStr, "Data/Game/StringList.txt", infoPtr);
    strPrivacy       = ReadLocalizedString("Privacy", langStr, "Data/Game/StringList.txt", infoPtr);
    strTerms         = ReadLocalizedString("Terms", langStr, "Data/Game/StringList.txt", infoPtr);
    // strMoreGames         = ReadLocalizedString("MoreGames", langStr, "Data/Game/StringList.txt", infoPtr);

    // Video Filter options
    // strVideoFilter       = ReadLocalizedString("VideoFilter", langStr, "Data/Game/StringList.txt", infoPtr);
    // strSharp             = ReadLocalizedString("Sharp", langStr, "Data/Game/StringList.txt", infoPtr);
    // strSmooth            = ReadLocalizedString("Smooth", langStr, "Data/Game/StringList.txt", infoPtr);
    // strNostalgic         = ReadLocalizedString("Nostalgic", langStr, "Data/Game/StringList.txt", infoPtr);

    // Login With Facebook
    // strFBLogin = ReadLocalizedString("LoginWithFacebook", langStr, "Data/Game/StringList.txt", infoPtr);

    // Unused Control Modes
    // strControlMethod = ReadLocalizedString("ControlMethod", langStr, "Data/Game/StringList.txt", infoPtr);
    // strSwipeAndTap   = ReadLocalizedString("SwipeAndTap", langStr, "Data/Game/StringList.txt", infoPtr);
    // strVirtualDPad   = ReadLocalizedString("VirtualDPad", langStr, "Data/Game/StringList.txt", infoPtr);

    if (infoPtr)
        CloseFile();

    ReadCreditsList("Data/Game/CreditsMobile.txt");
}
ushort *ReadLocalizedString(const char *stringName, const char *language, const char *filePath, FileInfo *info)
{
    FileInfo localInfo;
    FileInfo *fileInfo = info;
    if (!fileInfo) {
        fileInfo = &localInfo;
    }

    ushort strName[0x40];
    ushort langName[0x8];
    ushort lineBuffer[0x200];

    memset(strName, 0, 0x40 * sizeof(ushort));
    memset(langName, 0, 0x8 * sizeof(ushort));
    memset(lineBuffer, 0, 0x200 * sizeof(ushort));

    int strNamePos = 0;
    while (stringName[strNamePos]) {
        strName[strNamePos] = stringName[strNamePos];
        strNamePos++;
    }
    strName[strNamePos++] = ':';
    strName[strNamePos]   = 0;

    int langNamePos = 0;
    for (langNamePos = 0; langNamePos < 4; ++langNamePos) {
        if (!language[langNamePos])
            break;
        else
            langName[langNamePos] = language[langNamePos];
    }

    langName[langNamePos++] = ':';
    langName[langNamePos]   = 0;

    bool fileOpened = false;
    if (info) {
        SetFileInfo(info);
        SetFilePosition(0);
        fileOpened = true;
    }
    else if (LoadFile(filePath, fileInfo)) {
        fileOpened = true;
    }

    if (fileOpened) {
        int readMode   = 0;
        ushort curChar = 0;
        int charID     = 0;
        byte flag      = 0;
        while (!ReachedEndOfFile()) {
            switch (readMode) {
                case 0:
                    ReadStringLineUnicode(lineBuffer);
                    if (!FindStringTokenUnicode(lineBuffer, langName, 1)) {
                        int tPos = FindStringTokenUnicode(lineBuffer, strName, 1);
                        if (tPos == 3)
                            flag = true;
                        readMode = tPos == 3;
                    }
                    break;
                case 1:
                    FileRead(fileBuffer, sizeof(ushort));
                    curChar = fileBuffer[0] + (fileBuffer[1] << 8);
                    if (curChar > '\n' && curChar != '\r') {
                        stringStorage[stringStorePos + charID++] = 0;
                        if (!info)
                            CloseFile();

                        int pos = stringStorePos;
                        stringStorePos += charID;
                        return &stringStorage[pos];
                    }
                    else if (curChar == '\t') {
                        if (flag) {
                            flag     = true;
                            readMode = 2;
                        }
                        else {
                            readMode                                 = 2;
                            stringStorage[stringStorePos + charID++] = '\n';
                        }
                    }
                    break;
                case 2:
                    FileRead(fileBuffer, sizeof(ushort));
                    curChar = fileBuffer[0] + (fileBuffer[1] << 8);
                    if (curChar != '\t') {
                        stringStorage[stringStorePos + charID++] = curChar;
                        if (curChar == '\r' || curChar == '\n') {
                            flag     = false;
                            readMode = 1;
                        }
                    }
                    break;
            }
        }
        if (!info)
            CloseFile();
    }

    PrintLog("Failed to load string... (%s, %s)", language, stringName);
    return NULL;
}

void ReadCreditsList(const char *filePath)
{
    FileInfo info;
    if (LoadFile(filePath, &info)) {
        creditsListSize = 0;

        char dest[0x100];
        float advance = 24.0;
        if (!ReachedEndOfFile()) {
            while (creditsListSize < CREDITS_LIST_COUNT) {
                ReadCreditsLine(dest);

                if (dest[0] != '[' || dest[2] != ']') {
                    advance += 24.0;
                }
                else {
                    int strPos   = 0;
                    char curChar = dest[strPos + 3];
                    while (curChar) {
                        stringStorage[stringStorePos + strPos] = curChar;
                        strPos++;
                        curChar = dest[strPos + 3];
                    }
                    stringStorage[stringStorePos + strPos++] = 0;

                    switch (dest[1]) {
                        default:
                        case '0': creditsType[creditsListSize] = 0; break;
                        case '1': creditsType[creditsListSize] = 1; break;
                        case '2': creditsType[creditsListSize] = 2; break;
                        case '3': creditsType[creditsListSize] = 3; break;
                    }

                    creditsAdvanceY[creditsListSize] = advance;

                    strCreditsList[creditsListSize++] = &stringStorage[stringStorePos];
                    stringStorePos += strPos;
                    advance = 24.0;
                }

                if (ReachedEndOfFile())
                    break;
            }
        }

        CloseFile();
    }
}

