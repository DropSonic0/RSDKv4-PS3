#include "RetroEngine.hpp"

#if RETRO_USE_MOD_LOADER || !RETRO_USE_ORIGINAL_CODE
char savePath[0x100];
#endif

char playerNames[PLAYER_COUNT][0x20];
byte playerCount = 0;

#if RETRO_USE_MOD_LOADER
std::vector<ModInfo> modList;
#if RETRO_PLATFORM == RETRO_PS3
std::vector<ModInfo> modInstallList;
#endif
int activeMod = -1;

char modsPath[0x100];

bool redirectSave = false;

char modTypeNames[OBJECT_COUNT][0x40];
char modScriptPaths[OBJECT_COUNT][0x40];
byte modScriptFlags[OBJECT_COUNT];
byte modObjCount = 0;

#if RETRO_PLATFORM != RETRO_PS3
#include <filesystem>
#include <locale>
#endif

#if RETRO_PLATFORM == RETRO_PS3
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef S_ISDIR
#ifdef S_IFDIR
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#else
#define S_ISDIR(m) (((m)&0170000) == 0040000)
#endif
#endif
#ifndef S_ISREG
#ifdef S_IFREG
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#else
#define S_ISREG(m) (((m)&0170000) == 0100000)
#endif
#endif
#endif

void OpenModMenu()
{
    Engine.gameMode      = ENGINE_INITMODMENU;
    Engine.modMenuCalled = true;
}

#if RETRO_PLATFORM != RETRO_PS3
#if RETRO_PLATFORM == RETRO_ANDROID
namespace fs = std::__fs::filesystem; // this is so we can avoid using c++17, which causes a ton of warnings w asio and looks ugly
#else
namespace fs = std::filesystem;
#endif

fs::path resolvePath(fs::path given)
{
    if (given.is_relative())
        given = fs::current_path() / given; // thanks for the weird syntax!
    for (auto &p : fs::directory_iterator{ given.parent_path() }) {
        char pbuf[0x100];
        char gbuf[0x100];
        auto pf   = p.path().filename();
        auto pstr = pf.string();
        StringLowerCase(pbuf, pstr.c_str());
        auto gf   = given.filename();
        auto gstr = gf.string();
        StringLowerCase(gbuf, gstr.c_str());
        if (StrComp(pbuf, gbuf)) {
            return p.path();
        }
    }
    return given; // might work might not!
}
#endif

void InitMods()
{
    modList.clear();
    modObjCount = 0;
    memset(modScriptFlags, 0, sizeof(modScriptFlags));
    forceUseScripts    = forceUseScripts_Config;
    skipStartMenu      = skipStartMenu_Config;
    disableFocusPause  = disableFocusPause_Config;
    redirectSave       = false;
    Engine.forceSonic1 = false;
    sprintf(savePath, "");

    char modBuf[0x400];
#if RETRO_PLATFORM == RETRO_PS3
    StrCopy(modBuf, modsPath);
    if (StrComp(modsPath, BASE_PATH)) {
        if (modBuf[StrLength(modBuf) - 1] != '/')
            StrAdd(modBuf, "/");
        StrAdd(modBuf, "mods");
    }
    else {
        // Area de datos de juego, quitar barra final para evitar //
        if (modBuf[StrLength(modBuf) - 1] == '/')
            modBuf[StrLength(modBuf) - 1] = '\0';
    }
#else
    sprintf(modBuf, "%smods", modsPath);
#endif

    PrintLog("InitMods: Scanning %s", modBuf);

#if RETRO_PLATFORM != RETRO_PS3
    fs::path modPath = resolvePath(modBuf);

    if (fs::exists(modPath) && fs::is_directory(modPath)) {
        std::string mod_config = modPath.string() + "/modconfig.ini";
        FileIO *configFile     = fOpen(mod_config.c_str(), "r");
        if (configFile) {
            fClose(configFile);
            IniParser modConfig(mod_config.c_str(), false);

            for (int m = 0; m < modConfig.items.size(); ++m) {
                bool active = false;
                ModInfo info;
                modConfig.GetBool("mods", modConfig.items[m].key, &active);
                if (LoadMod(&info, modPath.string(), modConfig.items[m].key, active))
                    modList.push_back(info);
            }
        }

        try {
            auto rdi = fs::directory_iterator(modPath);
            for (auto de : rdi) {
                if (de.is_directory()) {
                    fs::path modDirPath = de.path();

                    ModInfo info;

                    std::string modDir            = modDirPath.string().c_str();
                    const std::string mod_inifile = modDir + "/mod.ini";
                    std::string folder            = modDirPath.filename().string();

                    bool flag = true;
                    for (int m = 0; m < modList.size(); ++m) {
                        if (modList[m].folder == folder) {
                            flag = false;
                            break;
                        }
                    }

                    if (flag) {
                        if (LoadMod(&info, modPath.string(), modDirPath.filename().string(), false))
                            modList.push_back(info);
                    }
                }
            }
        } catch (fs::filesystem_error fe) {
            PrintLog("Mods Folder Scanning Error: ");
            PrintLog(fe.what());
        }
    }
#else
    DIR *dir = opendir(modBuf);
    if (dir) {
        char mod_config[0x400];
        snprintf(mod_config, sizeof(mod_config), "%s/modconfig.ini", modBuf);
        FileIO *configFile = fOpen(mod_config, "r");
        if (configFile) {
            fClose(configFile);
            IniParser modConfig(mod_config, false);

            for (int m = 0; m < (int)modConfig.items.size(); ++m) {
                bool active = false;
                modConfig.GetBool("mods", modConfig.items[m].key, &active);
                ModInfo info;
                if (LoadMod(&info, modBuf, modConfig.items[m].key, active)) {
                    modList.push_back(info);
                    PrintLog("InitMods: Loaded mod %s from config", info.name.c_str());
                }
            }
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (entry->d_name[0] == '.')
                continue;

            char fullPath[0x400];
            snprintf(fullPath, sizeof(fullPath), "%s/%s", modBuf, entry->d_name);

            struct stat st;
            if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
                bool flag = true;
                for (int m = 0; m < (int)modList.size(); ++m) {
                    if (modList[m].folder == entry->d_name) {
                        flag = false;
                        break;
                    }
                }

                if (flag) {
                    ModInfo info;
                    if (LoadMod(&info, modBuf, entry->d_name, false)) {
                        modList.push_back(info);
                        PrintLog("InitMods: Found mod folder %s", info.name.c_str());
                    }
                }
            }
        }
        closedir(dir);
    }
    else {
        PrintLog("InitMods: Failed to open mods directory %s", modBuf);
    }
#endif

    forceUseScripts    = forceUseScripts_Config;
    skipStartMenu      = skipStartMenu_Config;
    disableFocusPause  = disableFocusPause_Config;
    redirectSave       = false;
    Engine.forceSonic1 = false;
    sprintf(savePath, "");
    for (int m = 0; m < (int)modList.size(); ++m) {
        if (!modList[m].active)
            continue;
        if (modList[m].useScripts)
            forceUseScripts = true;
        if (modList[m].skipStartMenu)
            skipStartMenu = true;
        if (modList[m].disableFocusPause)
            disableFocusPause |= modList[m].disableFocusPause;
        if (modList[m].redirectSave) {
            sprintf(savePath, "%s", modList[m].savePath.c_str());
            redirectSave = true;
        }
        if (modList[m].forceSonic1)
            Engine.forceSonic1 = true;
    }

    ReadSaveRAMData();
    ReadUserdata();

    // Reset mod script metadata to prevent contamination between loads
}
bool LoadMod(ModInfo *info, std::string modsPath, std::string folder, bool active)
{
    if (!info)
        return false;

    info->fileMap.clear();
    info->name    = "";
    info->desc    = "";
    info->author  = "";
    info->version = "";
    info->folder  = "";
    info->active  = false;
    info->scanned = false;

    const std::string modDir = modsPath + "/" + folder;

    FileIO *f = fOpen((modDir + "/mod.ini").c_str(), "r");
    if (f) {
        fClose(f);
        IniParser modSettings((modDir + "/mod.ini").c_str(), false);

        info->name    = "Unnamed Mod";
        info->desc    = "";
        info->author  = "Unknown Author";
        info->version = "1.0.0";
        info->folder  = folder;

        char infoBuf[0x100];
        // Name
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Name", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->name = infoBuf;
        // Desc
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Description", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->desc = infoBuf;
        // Author
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Author", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->author = infoBuf;
        // Version
        StrCopy(infoBuf, "");
        modSettings.GetString("", "Version", infoBuf);
        if (!StrComp(infoBuf, ""))
            info->version = infoBuf;

        info->active = active;

        ScanModFolder(info);

        info->useScripts = false;
        modSettings.GetBool("", "TxtScripts", &info->useScripts);
        if (info->useScripts && info->active)
            forceUseScripts = true;

        info->skipStartMenu = false;
        modSettings.GetBool("", "SkipStartMenu", &info->skipStartMenu);
        if (info->skipStartMenu && info->active)
            skipStartMenu = true;

        info->disableFocusPause = false;
        modSettings.GetInteger("", "DisableFocusPause", &info->disableFocusPause);
        if (info->disableFocusPause && info->active)
            disableFocusPause |= info->disableFocusPause;

        info->redirectSave = false;
        modSettings.GetBool("", "RedirectSaveRAM", &info->redirectSave);
        if (info->redirectSave) {
            char path[0x100];
#if RETRO_PLATFORM == RETRO_PS3
            if (StrComp(modsPath.c_str(), BASE_PATH))
                sprintf(path, "mods/%s/", folder.c_str());
            else
                sprintf(path, "%s/", folder.c_str());
#else
            sprintf(path, "mods/%s/", folder.c_str());
#endif
            info->savePath = path;
        }

        info->forceSonic1 = false;
        modSettings.GetBool("", "ForceSonic1", &info->forceSonic1);
        if (info->forceSonic1 && info->active)
            Engine.forceSonic1 = true;

        return true;
    }
    return false;
}

#if RETRO_PLATFORM == RETRO_PS3
static char scanPathPool[16][0x400];
void ScanModFolder_Recursive(ModInfo *info, const char *path, const char *folderName, int depth)
{
    if (depth >= 16) return;

    DIR *dir = opendir(path);
    if (!dir)
        return;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        cellSysutilCheckCallback();
        if (entry->d_name[0] == '.')
            continue;

        char *fullPath = scanPathPool[depth];
        snprintf(fullPath, 0x400, "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(fullPath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                ScanModFolder_Recursive(info, fullPath, folderName, depth + 1);
            }
            else if (S_ISREG(st.st_mode)) {
                char modBuf[0x400];
                StrCopy(modBuf, fullPath);

                char folderTest[4][0x40];
                snprintf(folderTest[0], 0x40, "%s/", folderName);
                snprintf(folderTest[1], 0x40, "%s\\", folderName);
                StringLowerCase(folderTest[2], folderTest[0]);
                StringLowerCase(folderTest[3], folderTest[1]);

                int tokenPos = -1;
                for (int i = 0; i < 4; ++i) {
                    tokenPos = FindLastStringToken(modBuf, folderTest[i]);
                    if (tokenPos >= 0)
                        break;
                }

                if (tokenPos >= 0) {
                    char buffer[0x400];
                    for (int i = StrLength(modBuf); i >= tokenPos; --i) {
                        buffer[i - tokenPos] = modBuf[i] == '\\' ? '/' : modBuf[i];
                    }

                    std::string path(buffer);
                    char pathLower[0x400];
                    memset(pathLower, 0, sizeof(char) * 0x400);
                    for (int c = 0; c < (int)path.size(); ++c) {
                        pathLower[c] = tolower(path.c_str()[c]);
                    }

                    info->fileMap.insert(std::pair<std::string, std::string>(pathLower, modBuf));
                }
            }
        }
    }
    closedir(dir);
}
#endif

#if RETRO_PLATFORM == RETRO_PS3
static byte copyBuffer[0x4000];
static char srcPathPool[16][0x400];
static char dstPathPool[16][0x400];

void CopyDirectory(const char *src, const char *dst, int depth)
{
    if (depth >= 16) {
        PrintLog("CopyDirectory: Maximum depth reached!");
        return;
    }

    DIR *dir = opendir(src);
    if (!dir) {
        PrintLog("CopyDirectory: Failed to open source dir: %s", src);
        return;
    }

    mkdir(dst, 0777);

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char *srcPath = srcPathPool[depth];
        char *dstPath = dstPathPool[depth];
        snprintf(srcPath, 0x400, "%s/%s", src, entry->d_name);
        snprintf(dstPath, 0x400, "%s/%s", dst, entry->d_name);

        struct stat st;
        if (stat(srcPath, &st) == 0) {
            if (S_ISDIR(st.st_mode)) {
                CopyDirectory(srcPath, dstPath, depth + 1);
            }
            else if (S_ISREG(st.st_mode)) {
                FILE *srcFile = fopen(srcPath, "rb");
                if (srcFile) {
                    FILE *dstFile = fopen(dstPath, "wb");
                    if (dstFile) {
                        size_t bytesRead;
                        while ((bytesRead = fread(copyBuffer, 1, sizeof(copyBuffer), srcFile)) > 0) {
                            cellSysutilCheckCallback();
                            fwrite(copyBuffer, 1, bytesRead, dstFile);
                        }
                        fclose(dstFile);
                    }
                    else {
                        PrintLog("CopyDirectory: Failed to open destination file: %s", dstPath);
                    }
                    fclose(srcFile);
                }
                else {
                    PrintLog("CopyDirectory: Failed to open source file: %s", srcPath);
                }
            }
        }
    }
    closedir(dir);
}

bool InstallMod(ModInfo *info)
{
    if (!info || info->basePath.empty())
        return false;

    PrintLog("Installing mod: %s from %s", info->name.c_str(), info->basePath.c_str());

    char srcPath[0x400];
    snprintf(srcPath, sizeof(srcPath), "%s/%s", info->basePath.c_str(), info->folder.c_str());

    char modsDir[0x400];
#if RETRO_PLATFORM == RETRO_PS3
    if (StrComp(modsPath, BASE_PATH))
        snprintf(modsDir, sizeof(modsDir), "%smods", modsPath);
    else {
        StrCopy(modsDir, modsPath);
        if (modsDir[StrLength(modsDir) - 1] == '/')
            modsDir[StrLength(modsDir) - 1] = '\0';
    }
#else
    snprintf(modsDir, sizeof(modsDir), "%smods", modsPath);
#endif

    mkdir(modsDir, 0777);

    char dstPath[0x400];
    snprintf(dstPath, sizeof(dstPath), "%s/%s", modsDir, info->folder.c_str());

    CopyDirectory(srcPath, dstPath, 0);

    PrintLog("Installation complete, reloading mods...");
    InitMods();
    return true;
}

void InitModInstallList()
{
    modInstallList.clear();

    std::vector<std::string> searchPaths;
    searchPaths.push_back("/dev_hdd0/packages");
    for (int i = 0; i <= 6; ++i) {
        char usbPath[32];
        snprintf(usbPath, sizeof(usbPath), "/dev_usb00%d", i);
        searchPaths.push_back(usbPath);

        snprintf(usbPath, sizeof(usbPath), "/dev_usb00%d/mods", i);
        searchPaths.push_back(usbPath);

        snprintf(usbPath, sizeof(usbPath), "/dev_usb00%d/MODS", i);
        searchPaths.push_back(usbPath);
    }

    for (int i = 0; i < (int)searchPaths.size(); ++i) {
        const char *packagesPath = searchPaths[i].c_str();
        DIR *dir = opendir(packagesPath);
        
        if (dir) {
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] == '.')
                    continue;

                char fullPath[0x400];
                snprintf(fullPath, sizeof(fullPath), "%s/%s", packagesPath, entry->d_name);

                struct stat st;
                if (stat(fullPath, &st) == 0 && S_ISDIR(st.st_mode)) {
                    ModInfo info;
                    if (LoadMod(&info, packagesPath, entry->d_name, false)) {
                        bool duplicate = false;
                        for (int m = 0; m < (int)modInstallList.size(); ++m) {
                            if (modInstallList[m].name == info.name && modInstallList[m].folder == info.folder) {
                                duplicate = true;
                                break;
                            }
                        }

                        if (!duplicate) {
                            info.basePath = packagesPath;
                            modInstallList.push_back(info);
                            PrintLog("Mod encontrado en %s: %s", packagesPath, info.name.c_str());
                        }
                    }
                }
            }
            closedir(dir);
        }
    }
}
#endif

void ScanModFolder(ModInfo *info)
{
    if (!info || !info->active || info->scanned)
        return;

    char modBuf[0x400];
#if RETRO_PLATFORM == RETRO_PS3
    StrCopy(modBuf, modsPath);
    if (StrComp(modsPath, BASE_PATH)) {
        if (modBuf[StrLength(modBuf) - 1] != '/')
            StrAdd(modBuf, "/");
        StrAdd(modBuf, "mods");
    }
    else {
        if (modBuf[StrLength(modBuf) - 1] == '/')
            modBuf[StrLength(modBuf) - 1] = '\0';
    }
#else
    sprintf(modBuf, "%smods", modsPath);
#endif

#if RETRO_PLATFORM != RETRO_PS3
    fs::path modPath = resolvePath(modBuf);

    const std::string modDir = modPath.string() + "/" + info->folder;

    info->fileMap.clear();

    // Check for Data/ replacements
    fs::path dataPath = resolvePath(modDir + "/Data");

    if (fs::exists(dataPath) && fs::is_directory(dataPath)) {
        try {
            auto data_rdi = fs::recursive_directory_iterator(dataPath);
            for (auto &data_de : data_rdi) {
                if (data_de.is_regular_file()) {
                    char modBuf[0x100];
                    StrCopy(modBuf, data_de.path().string().c_str());
                    char folderTest[4][0x10] = {
                        "Data/",
                        "Data\\",
                        "data/",
                        "data\\",
                    };
                    int tokenPos = -1;
                    for (int i = 0; i < 4; ++i) {
                        tokenPos = FindLastStringToken(modBuf, folderTest[i]);
                        if (tokenPos >= 0)
                            break;
                    }

                    if (tokenPos >= 0) {
                        char buffer[0x80];
                        for (int i = StrLength(modBuf); i >= tokenPos; --i) {
                            buffer[i - tokenPos] = modBuf[i] == '\\' ? '/' : modBuf[i];
                        }

                        // PrintLog(modBuf);
                        std::string path(buffer);
                        std::string modPath(modBuf);
                        char pathLower[0x100];
                        memset(pathLower, 0, sizeof(char) * 0x100);
                        for (int c = 0; c < path.size(); ++c) {
                            pathLower[c] = tolower(path.c_str()[c]);
                        }

                        info->fileMap.insert(std::pair<std::string, std::string>(pathLower, modBuf));
                    }
                }
            }
        } catch (fs::filesystem_error fe) {
            PrintLog("Data Folder Scanning Error: ");
            PrintLog(fe.what());
        }
    }

    // Check for Bytecode/ replacements
    fs::path bytecodePath = resolvePath(modDir + "/Bytecode");

    if (fs::exists(bytecodePath) && fs::is_directory(bytecodePath)) {
        try {
            auto data_rdi = fs::recursive_directory_iterator(bytecodePath);
            for (auto &data_de : data_rdi) {
                if (data_de.is_regular_file()) {
                    char modBuf[0x100];
                    StrCopy(modBuf, data_de.path().string().c_str());
                    char folderTest[4][0x10] = {
                        "Bytecode/",
                        "Bytecode\\",
                        "bytecode/",
                        "bytecode\\",
                    };
                    int tokenPos = -1;
                    for (int i = 0; i < 4; ++i) {
                        tokenPos = FindLastStringToken(modBuf, folderTest[i]);
                        if (tokenPos >= 0)
                            break;
                    }

                    if (tokenPos >= 0) {
                        char buffer[0x80];
                        for (int i = StrLength(modBuf); i >= tokenPos; --i) {
                            buffer[i - tokenPos] = modBuf[i] == '\\' ? '/' : modBuf[i];
                        }

                        // PrintLog(modBuf);
                        std::string path(buffer);
                        std::string modPath(modBuf);
                        char pathLower[0x100];
                        memset(pathLower, 0, sizeof(char) * 0x100);
                        for (int c = 0; c < path.size(); ++c) {
                            pathLower[c] = tolower(path.c_str()[c]);
                        }

                        info->fileMap.insert(std::pair<std::string, std::string>(pathLower, modBuf));
                    }
                }
            }
        } catch (fs::filesystem_error fe) {
            PrintLog("Bytecode Folder Scanning Error: ");
            PrintLog(fe.what());
        }
    }
#else
    char modDir[0x400];
    snprintf(modDir, sizeof(modDir), "%s/%s", modBuf, info->folder.c_str());

    info->fileMap.clear();

    char dataPath[0x400];
    snprintf(dataPath, sizeof(dataPath), "%s/Data", modDir);
    ScanModFolder_Recursive(info, dataPath, "Data", 0);

    char bytecodePath[0x400];
    snprintf(bytecodePath, sizeof(bytecodePath), "%s/Bytecode", modDir);
    ScanModFolder_Recursive(info, bytecodePath, "Bytecode", 0);
#endif
    info->scanned = true;
}

void GetModHash(char *hashBuf)
{
    if (!hashBuf)
        return;
    hashBuf[0] = 0;

#if RETRO_USE_MOD_LOADER
    std::string modStr = "";
    for (int m = 0; m < (int)modList.size(); ++m) {
        if (modList[m].active) {
            modStr += modList[m].folder;
            modStr += "|";
        }
    }

    if (modStr != "") {
        uint h[4];
        GenerateMD5FromString(modStr.c_str(), (int)modStr.length(), &h[0], &h[1], &h[2], &h[3]);
        sprintf(hashBuf, "%08x%08x%08x%08x", h[0], h[1], h[2], h[3]);
    }
#endif
}

void SaveMods()
{
    char modBuf[0x400];
#if RETRO_PLATFORM == RETRO_PS3
    StrCopy(modBuf, modsPath);
    if (StrComp(modsPath, BASE_PATH)) {
        if (modBuf[StrLength(modBuf) - 1] != '/')
            StrAdd(modBuf, "/");
        StrAdd(modBuf, "mods");
    }
    else {
        if (modBuf[StrLength(modBuf) - 1] == '/')
            modBuf[StrLength(modBuf) - 1] = '\0';
    }
#else
    sprintf(modBuf, "%smods", modsPath);
#endif
#if RETRO_PLATFORM != RETRO_PS3
    fs::path modPath = resolvePath(modBuf);

    if (fs::exists(modPath) && fs::is_directory(modPath)) {
        std::string mod_config = modPath.string() + "/modconfig.ini";
        IniParser modConfig;

        for (int m = 0; m < modList.size(); ++m) {
            ModInfo *info = &modList[m];

            modConfig.SetBool("mods", info->folder.c_str(), info->active);
        }

        modConfig.Write(mod_config.c_str(), false);
    }
#else
    DIR *dir = opendir(modBuf);
    if (dir) {
        closedir(dir);
        char mod_config[0x400];
        snprintf(mod_config, sizeof(mod_config), "%s/modconfig.ini", modBuf);
        IniParser modConfig;

        for (int m = 0; m < (int)modList.size(); ++m) {
            ModInfo *info = &modList[m];

            modConfig.SetBool("mods", info->folder.c_str(), info->active);
        }

        modConfig.Write(mod_config, false);
    }
#endif
}

void RefreshEngine()
{
    for (int m = 0; m < (int)modList.size(); ++m) ScanModFolder(&modList[m]);

    modObjCount = 0;
    memset(modScriptFlags, 0, sizeof(modScriptFlags));

    // Reload entire engine
    Engine.LoadGameConfig("Data/Game/GameConfig.bin");
#if RETRO_USING_SDL2
    if (Engine.window) {
        char gameTitle[0x40];
        sprintf(gameTitle, "%s%s", Engine.gameWindowText, Engine.usingDataFile_Config ? "" : " (Using Data Folder)");
        SDL_SetWindowTitle(Engine.window, gameTitle);
    }
#elif RETRO_USING_SDL1
    char gameTitle[0x40];
    sprintf(gameTitle, "%s%s", Engine.gameWindowText, Engine.usingDataFile_Config ? "" : " (Using Data Folder)");
    SDL_WM_SetCaption(gameTitle, NULL);
#endif

    ClearMeshData();
    ClearTextures(true);

    nativeEntityCountBackup = 0;
    memset(backupEntityList, 0, sizeof(backupEntityList));
    memset(objectEntityBackup, 0, sizeof(objectEntityBackup));

    nativeEntityCountBackupS = 0;
    memset(backupEntityListS, 0, sizeof(backupEntityListS));
    memset(objectEntityBackupS, 0, sizeof(objectEntityBackupS));

    for (int i = 0; i < FONTLIST_COUNT; ++i) {
        fontList[i].count = 2;
    }

    ReleaseStageSfx();
    ReleaseGlobalSfx();
    LoadGlobalSfx();
    InitLocalizedStrings();

    for (nativeEntityPos = 0; nativeEntityPos < nativeEntityCount; ++nativeEntityPos) {
        NativeEntity *entity = &objectEntityBank[activeEntityList[nativeEntityPos]];
        entity->eventCreate(entity);
    }

    forceUseScripts    = forceUseScripts_Config;
    skipStartMenu      = skipStartMenu_Config;
    disableFocusPause  = disableFocusPause_Config;
    redirectSave       = false;
    Engine.forceSonic1 = false;
    sprintf(savePath, "");
    for (int m = 0; m < (int)modList.size(); ++m) {
        if (!modList[m].active)
            continue;
        if (modList[m].useScripts)
            forceUseScripts = true;
        if (modList[m].skipStartMenu)
            skipStartMenu = true;
        if (modList[m].disableFocusPause)
            disableFocusPause |= modList[m].disableFocusPause;
        if (modList[m].redirectSave) {
            sprintf(savePath, "%s", modList[m].savePath.c_str());
            redirectSave = true;
        }
        if (modList[m].forceSonic1)
            Engine.forceSonic1 = true;
    }

    Engine.gameType = GAME_SONIC2;
    if (strstr(Engine.gameWindowText, "Sonic 1") || Engine.forceSonic1) {
        Engine.gameType = GAME_SONIC1;
    }

    achievementCount = 0;
    if (Engine.gameType == GAME_SONIC1) {
        AddAchievement("Ramp Ring Acrobatics",
                       "Without touching the ground,\rcollect all the rings in a\rtrapezoid formation in Green\rHill Zone Act 1");
        AddAchievement("Blast Processing", "Clear Green Hill Zone Act 1\rin under 30 seconds");
        AddAchievement("Secret of Marble Zone", "Travel though a secret\rroom in Marbale Zone Act 3");
        AddAchievement("Block Buster", "Break 16 blocks in a row\rwithout stopping");
        AddAchievement("Ring King", "Collect 200 Rings");
        AddAchievement("Secret of Labyrinth Zone", "Activate and ride the\rhidden platform in\rLabyrinth Zone Act 1");
        AddAchievement("Flawless Pursuit", "Clear the boss in Labyrinth\rZone without getting hurt");
        AddAchievement("Bombs Away", "Defeat the boss in Starlight Zone\rusing only the see-saw bombs");
        AddAchievement("Hidden Transporter", "Collect 50 Rings and take the hidden transporter path\rin Scrap Brain Act 2");
        AddAchievement("Chaos Connoisseur", "Collect all the chaos\remeralds");
        AddAchievement("One For the Road", "As a parting gift, land a\rfinal hit on Dr. Eggman's\rescaping Egg Mobile");
        AddAchievement("Beat The Clock", "Clear the Time Attack\rmode in less than 45\rminutes");
    }
    else if (Engine.gameType == GAME_SONIC2) {
        AddAchievement("Quick Run", "Complete Emerald Hill\rZone Act 1 in under 35\rseconds");
        AddAchievement("100% Chemical Free", "Complete Chemical Plant\rwithout going underwater");
        AddAchievement("Early Bird Special", "Collect all the Chaos\rEmeralds before Chemical\rPlant");
        AddAchievement("Superstar", "Complete any Act as\rSuper Sonic");
        AddAchievement("Hit it Big", "Get a jackpot on the Casino Night slot machines");
        AddAchievement("Bop Non-stop", "Defeat any boss in 8\rconsecutive hits without\rtouching he ground");
        AddAchievement("Perfectionist", "Get a Perfect Bonus by\rcollecting every Ring in an\rAct");
        AddAchievement("A Secret Revealed", "Find and complete\rHidden Palace Zone");
        AddAchievement("Head 2 Head", "Win a 2P Versus race\ragainst a friend");
        AddAchievement("Metropolis Master", "Complete Any Metropolis\rZone Act without getting\rhurt");
        AddAchievement("Scrambled Egg", "Defeat Dr. Eggman's Boss\rAttack mode in under 7\rminutes");
        AddAchievement("Beat the Clock", "Complete the Time Attack\rmode in less than 45\rminutes");
    }

    SaveMods();

    ReadSaveRAMData();
    ReadUserdata();
}

void GetModCount() { scriptEng.checkResult = (int)modList.size(); }
void GetModName(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].name.c_str());
}

void GetModDescription(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].desc.c_str());
}

void GetModAuthor(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].author.c_str());
}

void GetModVersion(int *textMenu, int *highlight, uint *id, int *unused)
{
    if (*id >= modList.size())
        return;

    TextMenu *menu                       = &gameMenu[*textMenu];
    menu->entryHighlight[menu->rowCount] = *highlight;
    AddTextMenuEntry(menu, modList[*id].version.c_str());
}

void GetModActive(uint *id, int *unused)
{
    scriptEng.checkResult = false;
    if (*id >= modList.size())
        return;

    scriptEng.checkResult = modList[*id].active;
}

void SetModActive(uint *id, int *active)
{
    if (*id >= modList.size())
        return;

    modList[*id].active = *active;
    if (modList[*id].active)
        ScanModFolder(&modList[*id]);
}

void MoveMod(uint *id, int *up)
{
    if (!id || !up)
        return;

    int preOption = *id;
    int option    = preOption + (*up ? -1 : 1);
    if (option < 0 || preOption < 0)
        return;

    if (option >= (int)modList.size() || preOption >= (int)modList.size())
        return;

    ModInfo swap       = modList[preOption];
    modList[preOption] = modList[option];
    modList[option]    = swap;
}

#endif

#if RETRO_USE_MOD_LOADER || !RETRO_USE_ORIGINAL_CODE
int GetSceneID(byte listID, const char *sceneName)
{
    if (listID >= 3)
        return -1;

    char scnName[0x40];
    int scnPos = 0;
    int pos    = 0;
    while (sceneName[scnPos]) {
        if (sceneName[scnPos] != ' ')
            scnName[pos++] = sceneName[scnPos];
        ++scnPos;
    }
    scnName[pos] = 0;

    for (int s = 0; s < stageListCount[listID]; ++s) {
        char nameBuffer[0x40];

        scnPos = 0;
        pos    = 0;
        while (stageList[listID][s].name[scnPos]) {
            if (stageList[listID][s].name[scnPos] != ' ')
                nameBuffer[pos++] = stageList[listID][s].name[scnPos];
            ++scnPos;
        }
        nameBuffer[pos] = 0;

        if (StrComp(scnName, nameBuffer)) {
            return s;
        }
    }
    return -1;
}
#endif
