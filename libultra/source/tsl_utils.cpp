/********************************************************************************
 * File: tsl_utils.cpp
 * Author: ppkantorski
 * Description: 
 *   'tsl_utils.cpp' provides the implementation of various utility functions
 *   defined in 'tsl_utils.hpp' for the Ultrahand Overlay project. This source file
 *   includes functionality for system checks, input handling, time-based interpolation,
 *   and other application-specific features essential for operating custom overlays
 *   on the Nintendo Switch.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository:
 *   GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay
 *
 *   Note: This notice is integral to the project's documentation and must not be 
 *   altered or removed.
 *
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2023-2026 ppkantorski
 ********************************************************************************/

#include <tsl_utils.hpp>

#include <cstdlib>
extern "C" { // assertion override
    void __assert_func(const char *_file, int _line, const char *_func, const char *_expr ) {
        abort();
    }
}

namespace ult {

    bool correctFrameSize; // for detecting the correct Overlay display size

    u16 DefaultFramebufferWidth = 448;            ///< Width of the framebuffer
    u16 DefaultFramebufferHeight = 720;           ///< Height of the framebuffer

    std::unordered_map<std::string, std::string> translationCache;
    
    std::unordered_map<u64, OverlayCombo> g_entryCombos;
    std::atomic<bool> launchingOverlay(false);
    std::atomic<bool> settingsInitialized(false);
    std::atomic<bool> currentForeground(false);
    //std::mutex simulatedNextPageMutex;

    // Helper function to read file content into a string
    bool readFileContent(const std::string& filePath, std::string& content) {
        FILE* file = fopen(filePath.c_str(), "r");
        if (!file) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to open JSON file: " + filePath);
            #endif
            return false;
        }
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), file) != nullptr) {
            content += buffer;
        }
        fclose(file);
        
        return true;
    }
    
    // Helper function to parse JSON-like content into a map
    void parseJsonContent(const std::string& content, std::unordered_map<std::string, std::string>& result) {
        size_t pos = 0;
        size_t keyStart, keyEnd, colonPos, valueStart, valueEnd;
        std::string key, value;
        
        auto normalizeNewlines = [](std::string &s) {
            size_t n = 0;
            while ((n = s.find("\\n", n)) != std::string::npos) {
                s.replace(n, 2, "\n");
                n += 1;
            }
        };
        
        while ((pos = content.find('"', pos)) != std::string::npos) {
            keyStart = pos + 1;
            keyEnd = content.find('"', keyStart);
            if (keyEnd == std::string::npos) break;
            
            key = content.substr(keyStart, keyEnd - keyStart);
            colonPos = content.find(':', keyEnd);
            if (colonPos == std::string::npos) break;
            
            valueStart = content.find('"', colonPos);
            valueEnd = content.find('"', valueStart + 1);
            if (valueStart == std::string::npos || valueEnd == std::string::npos) break;
            
            value = content.substr(valueStart + 1, valueEnd - valueStart - 1);
            
            // Convert escaped newlines (\\n) into real ones
            normalizeNewlines(key);
            normalizeNewlines(value);
            
            result[key] = value;
            
            key.clear();
            value.clear();
            pos = valueEnd + 1; // Move to next pair
        }
    }
    
    // Function to parse JSON key-value pairs into a map
    bool parseJsonToMap(const std::string& filePath, std::unordered_map<std::string, std::string>& result) {
        std::string content;
        if (!readFileContent(filePath, content)) {
            return false;
        }
        
        parseJsonContent(content, result);
        return true;
    }
    
    // Function to load translations from a JSON-like file into the translation cache
    bool loadTranslationsFromJSON(const std::string& filePath) {
        return parseJsonToMap(filePath, translationCache);
    }

    // Function to clear the translation cache
    void clearTranslationCache() {
        translationCache.clear();
        translationCache.rehash(0);
    }
    
    
    u16 activeHeaderHeight = 97;

    bool consoleIsDocked() {
        Result rc = apmInitialize();
        if (R_FAILED(rc)) return false;
        
        ApmPerformanceMode perfMode = ApmPerformanceMode_Invalid;
        rc = apmGetPerformanceMode(&perfMode);
        apmExit();
        
        return R_SUCCEEDED(rc) && (perfMode == ApmPerformanceMode_Boost);
    }
    
    //static bool pminfoInitialized = false;
    //static u64 lastPid = 0;
    //static u64 lastTid = 0;
    //
    //std::string getTitleIdAsString() {
    //    Result rc;
    //    u64 pid = 0;
    //    u64 tid = 0;
    //
    //    // Get the current application PID
    //    rc = pmdmntGetApplicationProcessId(&pid);
    //    if (R_FAILED(rc) || pid == 0) {
    //        return NULL_STR;
    //    }
    //
    //    // If it's the same PID as last time, return cached TID
    //    if (pid == lastPid && lastTid != 0) {
    //        char cachedTidStr[17];
    //        snprintf(cachedTidStr, sizeof(cachedTidStr), "%016lX", lastTid);
    //        return std::string(cachedTidStr);
    //    }
    //
    //    // Initialize pminfo if not already
    //    if (!pminfoInitialized) {
    //        rc = pminfoInitialize();
    //        if (R_FAILED(rc)) {
    //            return NULL_STR;
    //        }
    //        pminfoInitialized = true;
    //    }
    //
    //    // Retrieve the TID (Program ID)
    //    rc = pminfoGetProgramId(&tid, pid);
    //    if (R_FAILED(rc)) {
    //        return NULL_STR;
    //    }
    //
    //    lastPid = pid;
    //    lastTid = tid;
    //
    //    char titleIdStr[17];
    //    snprintf(titleIdStr, sizeof(titleIdStr), "%016lX", tid);
    //    return std::string(titleIdStr);
    //}
    
    //std::string getProcessIdAsString() {
    //    u64 pid = 0;
    //    if (R_FAILED(pmdmntGetApplicationProcessId(&pid)))
    //        return NULL_STR;
    //    
    //    char pidStr[21];  // Max u64 is 20 digits + null terminator
    //    snprintf(pidStr, sizeof(pidStr), "%llu", pid);
    //    return std::string(pidStr);
    //}

    std::string getBuildIdAsString() {
        u64 pid = 0;
        if (R_FAILED(pmdmntGetApplicationProcessId(&pid)))
            return NULL_STR;
        
        if (R_FAILED(ldrDmntInitialize()))
            return NULL_STR;
        
        LoaderModuleInfo moduleInfos[2];
        s32 count = 0;
        Result rc = ldrDmntGetProcessModuleInfo(pid, moduleInfos, 2, &count);
        ldrDmntExit();
        
        if (R_FAILED(rc) || count == 0)
            return NULL_STR;
        
        u64 buildid;
        std::memcpy(&buildid, moduleInfos[1].build_id, sizeof(u64));
        
        char buildIdStr[17];
        snprintf(buildIdStr, sizeof(buildIdStr), "%016lX", __builtin_bswap64(buildid));
        return std::string(buildIdStr);
    }
    

    std::string getTitleIdAsString() {
        u64 pid = 0, tid = 0;
        if (R_FAILED(pmdmntGetApplicationProcessId(&pid)) ||
            R_FAILED(pmdmntGetProgramId(&tid, pid)))
            return NULL_STR;
    
        char tidStr[17];
        snprintf(tidStr, sizeof(tidStr), "%016lX", tid);
        return std::string(tidStr);
    }

    
    std::string lastTitleID;
    std::atomic<bool> resetForegroundCheck(false); // initialize as true


    

    std::atomic<bool> internalTouchReleased(true);
    u32 layerEdge = 0;
    bool useRightAlignment = false;
    bool useSwipeToOpen = true;
    bool useLaunchCombos = true;
    bool useNotifications = false;
    bool useStartupNotification = false;
    bool useSoundEffects = true;
    bool useHapticFeedback = false;
    bool usePageSwap = false;
    bool useDynamicLogo = true;
    bool useSelectionBG = true;
    bool useSelectionText = true;
    bool useSelectionValue = false;

    std::atomic<bool> noClickableItems{false};
    
    #if IS_LAUNCHER_DIRECTIVE
    std::atomic<bool> overlayLaunchRequested{false};
    std::string requestedOverlayPath;
    std::string requestedOverlayArgs;
    std::mutex overlayLaunchMutex;
    #endif

    // Define the duration boundaries (for smooth scrolling)
    //const std::chrono::milliseconds initialInterval = std::chrono::milliseconds(67);  // Example initial interval
    //const std::chrono::milliseconds shortInterval = std::chrono::milliseconds(10);    // Short interval after long hold
    //const std::chrono::milliseconds transitionPoint = std::chrono::milliseconds(2000); // Point at which the shortest interval is reached
    
    // Function to interpolate between two durations
    //std::chrono::milliseconds interpolateDuration(std::chrono::milliseconds start, std::chrono::milliseconds end, float t) {
    //    using namespace std::chrono;
    //    auto interpolated = start.count() + static_cast<long long>((end.count() - start.count()) * t);
    //    return milliseconds(interpolated);
    //}
    
    
    
    //#include <filesystem> // Comment out filesystem
    
    // CUSTOM SECTION START
    //float backWidth, selectWidth, nextPageWidth;
    std::atomic<float> backWidth;
    std::atomic<float> selectWidth;
    std::atomic<float> nextPageWidth;
    std::atomic<bool> inMainMenu{false};
    std::atomic<bool> inHiddenMode{false};
    std::atomic<bool> inSettingsMenu{false};
    std::atomic<bool> inSubSettingsMenu{false};
    std::atomic<bool> inOverlaysPage{false};
    std::atomic<bool> inPackagesPage{false};

    std::atomic<bool> hasNextPageButton{false};
    
    bool firstBoot = true; // for detecting first boot
    bool reloadingBoot = false; // for detecting reloading boots
    
    //std::unordered_map<std::string, std::string> hexSumCache;
    
    // Define an atomic bool for interpreter completion
    std::atomic<bool> threadFailure(false);
    std::atomic<bool> runningInterpreter(false);
    std::atomic<bool> shakingProgress(true);
    
    std::atomic<bool> isHidden(false);
    std::atomic<bool> externalAbortCommands(false);
    
    //bool progressAnimation = false;
    bool disableTransparency = false;
    //bool useCustomWallpaper = false;
    //bool useMemoryExpansion = false;
    bool useOpaqueScreenshots = false;
    
    std::atomic<bool> onTrackBar(false);
    std::atomic<bool> allowSlide(false);
    std::atomic<bool> unlockedSlide(false);
    
    

    void atomicToggle(std::atomic<bool>& b) {
        bool expected = b.load(std::memory_order_relaxed);
        for (;;) {
            const bool desired = !expected;
            if (b.compare_exchange_weak(expected, desired,
                                        std::memory_order_acq_rel,
                                        std::memory_order_relaxed)) {
                break; // success
            }
            // expected has been updated with the current value on failure; loop continues
        }
    }

    
    bool updateMenuCombos = false;
    
    
    //void convertComboToUnicode(std::string& combo);


    std::array<KeyInfo, 18> KEYS_INFO = {{
        { HidNpadButton_L, "L", "\uE0E4" }, { HidNpadButton_R, "R", "\uE0E5" },
        { HidNpadButton_ZL, "ZL", "\uE0E6" }, { HidNpadButton_ZR, "ZR", "\uE0E7" },
        { HidNpadButton_AnySL, "SL", "\uE0E8" }, { HidNpadButton_AnySR, "SR", "\uE0E9" },
        { HidNpadButton_Left, "DLEFT", "\uE0ED" }, { HidNpadButton_Up, "DUP", "\uE0EB" },
        { HidNpadButton_Right, "DRIGHT", "\uE0EE" }, { HidNpadButton_Down, "DDOWN", "\uE0EC" },
        { HidNpadButton_A, "A", "\uE0E0" }, { HidNpadButton_B, "B", "\uE0E1" },
        { HidNpadButton_X, "X", "\uE0E2" }, { HidNpadButton_Y, "Y", "\uE0E3" },
        // ASAP: Change button style.
        { HidNpadButton_StickL, "LS", "\uE104" }, { HidNpadButton_StickR, "RS", "\uE105" },
        { HidNpadButton_Minus, "MINUS", "\uE0F2" }, { HidNpadButton_Plus, "PLUS", "\uE0F1" }
    }};

    std::unordered_map<std::string, std::string> createButtonCharMap() {
        std::unordered_map<std::string, std::string> map;
        for (const auto& keyInfo : KEYS_INFO) {
            map[keyInfo.name] = keyInfo.glyph;
        }
        return map;
    }
    
    std::unordered_map<std::string, std::string> buttonCharMap = createButtonCharMap();
    
    
    void convertComboToUnicode(std::string& combo) {
        // Quick check to see if the string contains a '+'
        if (combo.find('+') == std::string::npos) {
            return;  // No '+' found, nothing to modify
        }

        // Exit early if the combo contains any spaces
        if (combo.find(' ') != std::string::npos) {
            return;  // Spaces found, return without modifying
        }
        
        std::string unicodeCombo;
        bool modified = false;
        size_t start = 0;
        const size_t length = combo.length();
        size_t end = 0;  // Moved outside the loop
        std::string token;  // Moved outside the loop
        auto it = buttonCharMap.end();  // Initialize iterator once outside the loop
        
        // Iterate through the combo string and split by '+'
        for (size_t i = 0; i <= length; ++i) {
            if (i == length || combo[i] == '+') {
                // Get the current token (trimmed)
                end = i;  // Reuse the end variable
                while (start < end && std::isspace(combo[start])) start++;  // Trim leading spaces
                while (end > start && std::isspace(combo[end - 1])) end--;  // Trim trailing spaces
                
                token = combo.substr(start, end - start);  // Reuse the token variable
                it = buttonCharMap.find(token);  // Reuse the iterator
                
                if (it != buttonCharMap.end()) {
                    unicodeCombo += it->second;  // Append the mapped Unicode value
                    modified = true;
                } else {
                    unicodeCombo += token;  // Append the original token if not found
                }
                
                if (i != length) {
                    unicodeCombo += "+";  // Only append '+' if we're not at the end
                }
                
                start = i + 1;  // Move to the next token
            }
        }
        
        // If a modification was made, update the original combo
        if (modified) {
            combo = unicodeCombo;
        }
    }
    
    
    CONSTEXPR_STRING std::string whiteColor = "FFFFFF";
    CONSTEXPR_STRING std::string blackColor = "000000";
    CONSTEXPR_STRING std::string greyColor = "AAAAAA";
    
    std::atomic<bool> languageWasChanged{false};

    // --- Variable declarations (no inline init = no constructor code per-variable) ---
    #if IS_LAUNCHER_DIRECTIVE
    std::string ENGLISH;
    std::string SPANISH;
    std::string FRENCH;
    std::string GERMAN;
    std::string JAPANESE;
    std::string KOREAN;
    std::string ITALIAN;
    std::string DUTCH;
    std::string PORTUGUESE;
    std::string RUSSIAN;
    std::string UKRAINIAN;
    std::string POLISH;
    std::string SIMPLIFIED_CHINESE;
    std::string TRADITIONAL_CHINESE;
    std::string OVERLAYS;
    std::string OVERLAYS_ABBR;
    std::string OVERLAY;
    std::string HIDDEN_OVERLAYS;
    std::string PACKAGES;
    std::string PACKAGE;
    std::string HIDDEN_PACKAGES;
    std::string HIDDEN;
    std::string HIDE_OVERLAY;
    std::string HIDE_PACKAGE;
    std::string LAUNCH_ARGUMENTS;
    std::string FORCE_AMS110_SUPPORT;
    std::string QUICK_LAUNCH;
    std::string BOOT_COMMANDS;
    std::string EXIT_COMMANDS;
    std::string ERROR_LOGGING;
    std::string COMMANDS;
    std::string SETTINGS;
    std::string FAVORITE;
    std::string MAIN_SETTINGS;
    std::string UI_SETTINGS;
    std::string WIDGET;
    std::string WIDGET_ITEMS;
    std::string WIDGET_SETTINGS;
    std::string CLOCK;
    std::string BATTERY;
    std::string SOC_TEMPERATURE;
    std::string PCB_TEMPERATURE;
    std::string BACKDROP;
    std::string DYNAMIC_COLORS;
    std::string CENTER_ALIGNMENT;
    std::string EXTENDED_BACKDROP;
    std::string MISCELLANEOUS;
    std::string MENU_SETTINGS;
    std::string USER_GUIDE;
    std::string SHOW_HIDDEN;
    std::string SHOW_DELETE;
    std::string SHOW_UNSUPPORTED;
    std::string PAGE_SWAP;
    std::string RIGHT_SIDE_MODE;
    std::string OVERLAY_VERSIONS;
    std::string PACKAGE_VERSIONS;
    std::string CLEAN_VERSIONS;
    std::string KEY_COMBO;
    std::string MODE;
    std::string LAUNCH_MODES;
    std::string LANGUAGE;
    std::string OVERLAY_INFO;
    std::string SOFTWARE_UPDATE;
    std::string UPDATE_ULTRAHAND;
    std::string SYSTEM;
    std::string DEVICE_INFO;
    std::string FIRMWARE;
    std::string BOOTLOADER;
    std::string HARDWARE;
    std::string MEMORY;
    std::string VENDOR;
    std::string MODEL;
    std::string STORAGE;
    std::string OVERLAY_MEMORY;
    std::string NOT_ENOUGH_MEMORY;
    std::string WALLPAPER_SUPPORT_DISABLED;
    std::string SOUND_SUPPORT_DISABLED;
    std::string WALLPAPER_SUPPORT_ENABLED;
    std::string SOUND_SUPPORT_ENABLED;
    std::string EXIT_OVERLAY_SYSTEM;
    std::string ULTRAHAND_ABOUT;
    std::string ULTRAHAND_CREDITS_START;
    std::string ULTRAHAND_CREDITS_END;
    std::string LOCAL_IP;
    std::string WALLPAPER;
    std::string THEME;
    std::string SOUNDS;
    std::string DEFAULT;
    std::string ROOT_PACKAGE;
    std::string SORT_PRIORITY;
    std::string OPTIONS;
    std::string FAILED_TO_OPEN;
    std::string LAUNCH_COMBOS;
    std::string STARTUP_NOTIFICATION;
    std::string EXTERNAL_NOTIFICATIONS;
    std::string HAPTIC_FEEDBACK;
    std::string OPAQUE_SCREENSHOTS;
    std::string PACKAGE_INFO;
    std::string _TITLE;
    std::string _VERSION;
    std::string _CREATOR;
    std::string _ABOUT;
    std::string _CREDITS;
    std::string USERGUIDE_OFFSET;
    std::string SETTINGS_MENU;
    std::string SCRIPT_OVERLAY;
    std::string STAR_FAVORITE;
    std::string APP_SETTINGS;
    std::string ON_MAIN_MENU;
    std::string ON_A_COMMAND;
    std::string ON_OVERLAY_PACKAGE;
    std::string FEATURES;
    std::string SWIPE_TO_OPEN;
    std::string THEME_SETTINGS;
    std::string DYNAMIC_LOGO;
    std::string SELECTION_BACKGROUND;
    std::string SELECTION_TEXT;
    std::string SELECTION_VALUE;
    std::string LIBULTRAHAND_TITLES;
    std::string LIBULTRAHAND_VERSIONS;
    std::string PACKAGE_TITLES;
    std::string ULTRAHAND_HAS_STARTED;
    std::string ULTRAHAND_HAS_RESTARTED;
    std::string NEW_UPDATE_IS_AVAILABLE;
    std::string DELETE_PACKAGE;
    std::string DELETE_OVERLAY;
    std::string SELECTION_IS_EMPTY;
    std::string FORCED_SUPPORT_WARNING;
    std::string TASK_IS_COMPLETE;
    std::string TASK_HAS_FAILED;
    std::string REBOOT_TO;
    std::string CFWBOOT;
    std::string OFWBOOT;
    std::string ANDROID;
    std::string LAKKA;
    std::string UBUNTU;
    std::string REBOOT;
    std::string SHUTDOWN;

    /* ASAP Packages */
    std::string UPDATE_ASAP_1;
    std::string UPDATE_ASAP_2;
    std::string UPDATE_ASAP_3;
    std::string UPDATE_ASAP_4;
    // Informations
    std::string CFWBOOT_TITLE;
    std::string HEKATE_TITLE;
    std::string L4T_TITLE;
    std::string VOLUME_INFO;
    std::string LAUNCHER_INFO;
    std::string CFWBOOT_INFO;
    std::string HEKATE_INFO;
    std::string BOOT_ENTRY;
    // Configs
    std::string MAINMENU_INFO;
    std::string INFO;
    std::string ABOUT_INFO;
    std::string OLD_DEVICE;
    std::string NEW_DEVICE;
    std::string DF_THEME;
    std::string UPDATE_ULTRA_INFO;
    std::string UPDATE_TEST_WARN;
    std::string HIGHLIGHT_LINE;
    std::string UPDATE_TEST_INFO;
    std::string UPDATE_TEST_NOTICE;
    // ETC.
    std::string OVERRIDE_SELECTION;
    std::string AUTO_SELECTION;
    // Package-System Clock+
    std::string ENABLED;
    std::string SCPLUS_INFO;
    std::string SC_STATUS;
    std::string LDR_TOOL;
    std::string LDR_INFO;
    std::string TOOL_WARNING_1;
    std::string TOOL_WARNING_2;
    std::string CPU_VOLTAGE;
    std::string CPU_INFO;
    std::string CPU_CHART;
    std::string USED;
    // Package-Extra Setting+
    std::string ES_DISABLED;
    std::string CD_LAUNCHER;
    std::string INSTALL_PATH;
    std::string INSTALL_VER;
    std::string RBOOT_OPTION;
    std::string CD_LAUNCHER_INFO;
    std::string AND_LCR_INFO;
    std::string UBU_LCR_INFO;
    std::string SET_OS_NAND;
    std::string REBOOT_BOOTLOADER;
    std::string USB_HS_INFO;
    std::string DDR_INFO;
    std::string UNSURE_INFO;
    std::string SET_UBU_VER;
    std::string EXTRA_SETTING;
    std::string CFW_CFG;
    std::string CFW_CFG_INFO;
    std::string DEFAULT_NAND;
    std::string DEFAULT_NAND_INFO;
    std::string NO_EMUNAND;
    std::string ADD_ENTRY;
    std::string ADD_ENTRY_INFO;
    std::string MOON_LAUNCHER;
    std::string MOON_ENTRY;
    std::string MOON_LAUNCHER_INFO;
    std::string MOON_LAUNCHER_WARN_1;
    std::string MOON_LAUNCHER_WARN_2;
    std::string MOON_CFW;
    std::string MOON_STOCK;
    std::string MOON_ANDROID;
    std::string MOON_LAKKA;
    std::string MOON_UBUNTU;
    std::string SYSTEM_CFG;
    std::string SYSTEM_CFG_INFO;
    std::string HEKATE_BOOT;
    std::string HEKATE_BOOT_INFO;
    std::string HEKATE_BACKLIGHT_INFO;
    std::string HEKATE_BACKLIGHT;
    std::string AUTO_HOS_OFF_SEC;
    std::string AUTO_HOS_OFF_VAL;
    std::string HEKATE_AUTO_HOS_OFF;
    std::string BOOT_WAIT_SEC;
    std::string BOOT_WAIT_VAL;
    std::string HEKATE_BOOT_WAIT;
    std::string BOOT_WAIT_SECOND;
    std::string AUTOBOOT_CFW_SEC;
    std::string AUTOBOOT_CFW_VAL;
    std::string HEKATE_AUTOBOOT;
    std::string SYSTEM_PAGE;
    std::string DMNT_CHEAT;
    std::string DMNT_TOGGLE;
    std::string NO_GC_INFO;
    std::string NO_GC_WARN;
    std::string NO_GC;
    std::string REC_INFO;
    std::string REC_WARN;
    std::string SYSMMC_ONLY;
    std::string ENABLED_EMUMMC;
    std::string EXOSPHERE_INFO;
    std::string EXOSPHERE;
    std::string DNS_INFO;
    std::string DNS_MITM;
    std::string APPLY_RESET_SEC;
    std::string APPLY_RESET_VAL;
    std::string APPLY_CFG;
    std::string RESET_CFG;
    std::string HB_MENU;
    std::string HB_MENU_INFO;
    std::string FULL_MEMORY;
    std::string FULL_MEMORY_INFO;
    std::string FULL_MEMORY_REC;
    std::string FULL_MEMORY_FORWARDER_VAL;
    std::string FULL_MEMORY_FORWARDER_CTN;
    std::string FULL_MEMORY_KEY;
    std::string APPLET_MEMORY;
    std::string APPLET_MEMORY_INFO;
    std::string APPLET_MEMORY_VAL;
    std::string APPLET_MEMORY_KEY;
    std::string APPLET_HB_MENU_ICON;
    std::string APPLET_HB_MENU_KEY;
    std::string S_LANG_PATCH;
    std::string S_TRANSLATE;
    std::string S_LANG;
    std::string APPLET_ALBUM;
    std::string APPLET_USER;
    std::string APPLET_KEY_HOLD;
    std::string APPLET_KEY_CLICK;
    std::string SD_CLEANUP;
    std::string SD_CLEANUP_INFO;
    std::string NORMAL_DEVICE;
    std::string PATCH_DEVICE;
    std::string RAM_PATCH_WARN;
    std::string RAM_PATCH;
    // Package-Quick Guide+
    std::string QUICK_GUIDE_INFO;
    std::string QUICK_GUIDE;
    std::string KEYMAP_INFO;
    std::string KEY_MOVE;
    std::string KEY_MENU;
    std::string KEY_SELECT;
    std::string KEYGAP_1;
    std::string KEYGAP_2;
    std::string KEYGAP_3;
    std::string PACK_INFO;
    std::string USEFUL;
    std::string HIDE_SPHAIRA;
    std::string FORWARDER_SPHAIRA;
    std::string APP_INSTALL;
    std::string PC_INSTALL;
    std::string FTP_INSTALL;
    std::string MTP_INSTALL;
    std::string USB_INSTALL;
    std::string HDD_INSTALL_1;
    std::string HDD_INSTALL_2;
    std::string ERROR_INFO;
    std::string ALBUM_INFO;
    std::string ALBUM_ERROR_1;
    std::string ALBUM_ERROR_2;
    std::string CPUDEAD_INFO;
    std::string CPU_ERROR;
    std::string SYSTEM_ERROR;
    std::string MODULE_ERROR;
    std::string MODULE_INFO_1;
    std::string MODULE_INFO_2;
    std::string MODULE_INFO_3;
    std::string MODULE_INFO_4;
    std::string SYSNAND_INFO;
    std::string EMUNAND_INFO;
    std::string NORMAL_ERROR;
    std::string APP_MODULE_ERROR;
    std::string SWITCH_MODULE_ERROR;
    std::string PREV_PAGE;
    std::string NEXT_PAGE;
    std::string VER_ERR;
    std::string SD_ERR;
    std::string STORAGE_ERR;
    std::string MISC_ERR;
    std::string NSS_ERR;
    std::string SERVER_ERR;
    std::string NETWORK_ERR;
    std::string FORWARDER_ERR;
    std::string FORWARDER_INFO;
    std::string APP_ERR;
    std::string APP_MODULE;
    std::string SCROLL;
    std::string SWITCH_MODULE;
    // Overlays-Custom infomation
    std::string OVLSYSMODULE;
    std::string EDIZON;
    std::string EMUIIBO;
    std::string FIZEAU;
    std::string NXFANCONTROL;
    std::string FPSLOCKER;
    std::string LDNMITM;
    std::string QUICKNTP;
    std::string REVERSENX;
    std::string STATUSMONITOR;
    std::string STUDIOUSPANCAKE;
    std::string SYSCLK;
    std::string SYSDVR;
    std::string SYSPATCH;
    #endif

    std::string INCOMPATIBLE_WARNING;
    std::string SYSTEM_RAM;
    std::string FREE;
    std::string DEFAULT_CHAR_WIDTH;
    std::string UNAVAILABLE_SELECTION;
    std::string ON;
    std::string OFF;
    std::string OK;
    std::string BACK;
    std::string HIDE;
    std::string CANCEL;
    std::string GAP_1;
    std::string GAP_2;

    std::atomic<float> halfGap = 0.0f;

    #if USING_WIDGET_DIRECTIVE
    std::string SUNDAY;
    std::string MONDAY;
    std::string TUESDAY;
    std::string WEDNESDAY;
    std::string THURSDAY;
    std::string FRIDAY;
    std::string SATURDAY;
    std::string JANUARY;
    std::string FEBRUARY;
    std::string MARCH;
    std::string APRIL;
    std::string MAY;
    std::string JUNE;
    std::string JULY;
    std::string AUGUST;
    std::string SEPTEMBER;
    std::string OCTOBER;
    std::string NOVEMBER;
    std::string DECEMBER;
    std::string SUN;
    std::string MON;
    std::string TUE;
    std::string WED;
    std::string THU;
    std::string FRI;
    std::string SAT;
    std::string JAN;
    std::string FEB;
    std::string MAR;
    std::string APR;
    std::string MAY_ABBR;
    std::string JUN;
    std::string JUL;
    std::string AUG;
    std::string SEP;
    std::string OCT;
    std::string NOV;
    std::string DEC;
    #endif


    // --- Single source-of-truth table ---

    struct LangEntry { std::string* var; const char* key; const char* defaultVal; };

    static constexpr LangEntry LANG_TABLE[] = {
        #if IS_LAUNCHER_DIRECTIVE
        {&ENGLISH,                    "ENGLISH",                    "영어"},
        {&SPANISH,                    "SPANISH",                    "스페인어"},
        {&FRENCH,                     "FRENCH",                     "프랑스어"},
        {&GERMAN,                     "GERMAN",                     "독일어"},
        {&JAPANESE,                   "JAPANESE",                   "일본어"},
        {&KOREAN,                     "KOREAN",                     "한국어"},
        {&ITALIAN,                    "ITALIAN",                    "이탈리아어"},
        {&DUTCH,                      "DUTCH",                      "네덜란드어"},
        {&PORTUGUESE,                 "PORTUGUESE",                 "포루투갈어"},
        {&RUSSIAN,                    "RUSSIAN",                    "러시아어"},
        {&UKRAINIAN,                  "UKRAINIAN",                  "우크라이나어"},
        {&POLISH,                     "POLISH",                     "폴란드어"},
        {&SIMPLIFIED_CHINESE,         "SIMPLIFIED_CHINESE",         "중국어 (간체)"},
        {&TRADITIONAL_CHINESE,        "TRADITIONAL_CHINESE",        "중국어 (번체)"},
        {&OVERLAYS,                   "OVERLAYS",                   "오버레이"},
        {&OVERLAYS_ABBR,              "OVERLAYS_ABBR",              "오버레이"},
        {&OVERLAY,                    "OVERLAY",                    "오버레이"},
        {&HIDDEN_OVERLAYS,            "HIDDEN_OVERLAYS",            "Hidden Overlays"},
        {&PACKAGES,                   "PACKAGES",                   "Package+"},
        {&PACKAGE,                    "PACKAGE",                    "패키지"},
        {&HIDDEN_PACKAGES,            "HIDDEN_PACKAGES",            "숨겨진 패키지"},
        {&HIDDEN,                     "HIDDEN",                     "숨겨진 항목"},
        {&HIDE_OVERLAY,               "HIDE_OVERLAY",               "오버레이 숨기기"},
        {&HIDE_PACKAGE,               "HIDE_PACKAGE",               "패키지 숨기기"},
        {&LAUNCH_ARGUMENTS,           "LAUNCH_ARGUMENTS",           "인수 실행"},
        {&FORCE_AMS110_SUPPORT,       "FORCE_AMS110_SUPPORT",       "버전 강제 지원"},
        {&QUICK_LAUNCH,               "QUICK_LAUNCH",               "빠른 실행"},
        {&BOOT_COMMANDS,              "BOOT_COMMANDS",              "Boot 커맨드"},
        {&EXIT_COMMANDS,              "EXIT_COMMANDS",              "Exit 커맨드"},
        {&ERROR_LOGGING,              "ERROR_LOGGING",              "오류 로깅"},
        {&COMMANDS,                   "COMMANDS",                   "커맨드"},
        {&SETTINGS,                   "SETTINGS",                   "설정"},
        {&FAVORITE,                   "FAVORITE",                   "즐겨찾기"},
        {&MAIN_SETTINGS,              "MAIN_SETTINGS",              "메인 설정"},
        {&UI_SETTINGS,                "UI_SETTINGS",                "UI 변경"},
        {&WIDGET,                     "WIDGET",                     "위젯"},
        {&WIDGET_ITEMS,               "WIDGET_ITEMS",               "위젯 아이템"},
        {&WIDGET_SETTINGS,            "WIDGET_SETTINGS",            "위젯 설정"},
        {&CLOCK,                      "CLOCK",                      "시각"},
        {&BATTERY,                    "BATTERY",                    "배터리"},
        {&SOC_TEMPERATURE,            "SOC_TEMPERATURE",            "SoC 온도"},
        {&PCB_TEMPERATURE,            "PCB_TEMPERATURE",            "PCB 온도"},
        {&BACKDROP,                   "BACKDROP",                   "배경"},
        {&DYNAMIC_COLORS,             "DYNAMIC_COLORS",             "동적 색상"},
        {&CENTER_ALIGNMENT,           "CENTER_ALIGNMENT",           "위젯 가운데 정렬"},
        {&EXTENDED_BACKDROP,          "EXTENDED_BACKDROP",          "위젯 배경 확장"},
        {&MISCELLANEOUS,              "MISCELLANEOUS",              "기타"},
        {&MENU_SETTINGS,              "MENU_SETTINGS",              "메뉴 설정"},
        {&USER_GUIDE,                 "USER_GUIDE",                 "사용 설명서"},
        {&SHOW_HIDDEN,                "SHOW_HIDDEN",                "숨김 항목 표시"},
        {&SHOW_DELETE,                "SHOW_DELETE",                "삭제 옵션 표시"},
        {&SHOW_UNSUPPORTED,           "SHOW_UNSUPPORTED",           "미지원 항목 표시"},
        {&PAGE_SWAP,                  "PAGE_SWAP",                  "페이지 교체"},
        {&RIGHT_SIDE_MODE,            "RIGHT_SIDE_MODE",            "우측 배치"},
        {&OVERLAY_VERSIONS,           "OVERLAY_VERSIONS",           "오버레이 버전"},
        {&PACKAGE_VERSIONS,           "PACKAGE_VERSIONS",           "패키지 버전"},
        {&CLEAN_VERSIONS,             "CLEAN_VERSIONS",             "정리된 버전"},
        {&KEY_COMBO,                  "KEY_COMBO",                  "키 조합"},
        {&MODE,                       "MODE",                       "모드"},
        {&LAUNCH_MODES,               "LAUNCH_MODES",               "모드"},
        {&LANGUAGE,                   "LANGUAGE",                   "언어"},
        {&OVERLAY_INFO,               "OVERLAY_INFO",               "오버레이 정보"},
        {&SOFTWARE_UPDATE,            "SOFTWARE_UPDATE",            "업데이트"},
        {&UPDATE_ULTRAHAND,           "UPDATE_ULTRAHAND",           "업데이트"},
        {&SYSTEM,                     "SYSTEM",                     "시스템 정보"},
        {&DEVICE_INFO,                "DEVICE_INFO",                "기기 상세"},
        {&FIRMWARE,                   "FIRMWARE",                   "펌웨어"},
        {&BOOTLOADER,                 "BOOTLOADER",                 "부트로더"},
        {&HARDWARE,                   "HARDWARE",                   "하드웨어"},
        {&MEMORY,                     "MEMORY",                     "메모리"},
        {&VENDOR,                     "VENDOR",                     "제조사"},
        {&MODEL,                      "MODEL",                      "P/N"},
        {&STORAGE,                    "STORAGE",                    "저장소"},
        {&OVERLAY_MEMORY,             "OVERLAY_MEMORY",             "오버레이 메모리"},
        {&NOT_ENOUGH_MEMORY,          "NOT_ENOUGH_MEMORY",          "메모리가 부족합니다."},
        {&WALLPAPER_SUPPORT_DISABLED, "WALLPAPER_SUPPORT_DISABLED", "배경화면 비활성화"},
        {&SOUND_SUPPORT_DISABLED,     "SOUND_SUPPORT_DISABLED",     "사운드 비활성화"},
        {&WALLPAPER_SUPPORT_ENABLED,  "WALLPAPER_SUPPORT_ENABLED",  "배경화면 활성화"},
        {&SOUND_SUPPORT_ENABLED,      "SOUND_SUPPORT_ENABLED",      "사운드 활성화"},
        {&EXIT_OVERLAY_SYSTEM,        "EXIT_OVERLAY_SYSTEM",        "오버레이 종료"},
        {&ULTRAHAND_ABOUT,            "ULTRAHAND_ABOUT",            "Ultrahand는 명령어, 오버레이, 단축키 및 고급 시스템 상호작용을 위한 커스터마이징 시스템입니다."},
        {&ULTRAHAND_CREDITS_START,    "ULTRAHAND_CREDITS_START",    "Special thanks to "},
        {&ULTRAHAND_CREDITS_END,      "ULTRAHAND_CREDITS_END",      " and many others. ♥"},
        {&LOCAL_IP,                   "LOCAL_IP",                   "로컬 IP"},
        {&WALLPAPER,                  "WALLPAPER",                  "배경"},
        {&THEME,                      "THEME",                      "테마"},
        {&SOUNDS,                     "SOUNDS",                     "사운드"},
        {&DEFAULT,                    "DEFAULT",                    "기본"},
        {&ROOT_PACKAGE,               "ROOT_PACKAGE",               "기본 패키지"},
        {&SORT_PRIORITY,              "SORT_PRIORITY",              "우선순위 정렬"},
        {&OPTIONS,                    "OPTIONS",                    "옵션"},
        {&FAILED_TO_OPEN,             "FAILED_TO_OPEN",             "파일 열기 실패"},
        {&LAUNCH_COMBOS,              "LAUNCH_COMBOS",              "실행 버튼"},
        {&STARTUP_NOTIFICATION,       "STARTUP_NOTIFICATION",       "시작 알림"},
        {&EXTERNAL_NOTIFICATIONS,     "EXTERNAL_NOTIFICATIONS",     "외부 알림"},
        {&HAPTIC_FEEDBACK,            "HAPTIC_FEEDBACK",            "햅틱 피드백"},
        {&OPAQUE_SCREENSHOTS,         "OPAQUE_SCREENSHOTS",         "스크린샷 배경"},
        {&PACKAGE_INFO,               "PACKAGE_INFO",               "패키지 정보"},
        {&_TITLE,                     "_TITLE",                     "이름"},
        {&_VERSION,                   "_VERSION",                   "정보"},
        {&_CREATOR,                   "_CREATOR",                   "개발자"},
        {&_ABOUT,                     "_ABOUT",                     "설명"},
        {&_CREDITS,                   "_CREDITS",                   "기여자"},
        {&USERGUIDE_OFFSET,           "USERGUIDE_OFFSET",           "177"},
        {&SETTINGS_MENU,              "SETTINGS_MENU",              "설정 메뉴"},
        {&SCRIPT_OVERLAY,             "SCRIPT_OVERLAY",             "오버레이 스크립트"},
        {&STAR_FAVORITE,              "STAR_FAVORITE",              "즐겨찾기"},
        {&APP_SETTINGS,               "APP_SETTINGS",               "앱 설정"},
        {&ON_MAIN_MENU,               "ON_MAIN_MENU",               "메인 메뉴"},
        {&ON_A_COMMAND,               "ON_A_COMMAND",               "커맨드 위에서"},
        {&ON_OVERLAY_PACKAGE,         "ON_OVERLAY_PACKAGE",         "오버레이/패키지 위에서"},
        {&FEATURES,                   "FEATURES",                   "이펙트"},
        {&SWIPE_TO_OPEN,              "SWIPE_TO_OPEN",              "밀어서 열기"},
        {&THEME_SETTINGS,             "THEME_SETTINGS",             "테마 설정"},
        {&DYNAMIC_LOGO,               "DYNAMIC_LOGO",               "동적 로고"},
        {&SELECTION_BACKGROUND,       "SELECTION_BACKGROUND",       "선택 배경"},
        {&SELECTION_TEXT,             "SELECTION_TEXT",             "선택 텍스트"},
        {&SELECTION_VALUE,            "SELECTION_VALUE",            "선택 값"},
        {&LIBULTRAHAND_TITLES,        "LIBULTRAHAND_TITLES",        "libultrahand 제목"},
        {&LIBULTRAHAND_VERSIONS,      "LIBULTRAHAND_VERSIONS",      "libultrahand 버전"},
        {&PACKAGE_TITLES,             "PACKAGE_TITLES",             "패키지 제목"},
        {&ULTRAHAND_HAS_STARTED,      "ULTRAHAND_HAS_STARTED",      "Ultrahand 시작됨"},
        {&ULTRAHAND_HAS_RESTARTED,    "ULTRAHAND_HAS_RESTARTED",    "Ultrahand 재시작됨"},
        {&NEW_UPDATE_IS_AVAILABLE,    "NEW_UPDATE_IS_AVAILABLE",    "새로운 버전이 있습니다!"},
        {&DELETE_PACKAGE,             "DELETE_PACKAGE",             "패키지 삭제"},
        {&DELETE_OVERLAY,             "DELETE_OVERLAY",             "오버레이 삭제"},
        {&SELECTION_IS_EMPTY,         "SELECTION_IS_EMPTY",         "비어 있음"},
        {&FORCED_SUPPORT_WARNING,     "FORCED_SUPPORT_WARNING",     "강제 지원은 위험할 수 있습니다"},
        {&TASK_IS_COMPLETE,           "TASK_IS_COMPLETE",           "작업 완료!"},
        {&TASK_HAS_FAILED,            "TASK_HAS_FAILED",            "작업 실패"},
        {&REBOOT_TO,                  "REBOOT_TO",                  "다시 시작 · 시스템 종료"},
        {&CFWBOOT,                    "CFWBOOT",                    "에뮤/시스낸드 (커펌)"},
        {&OFWBOOT,                    "OFWBOOT",                    "시스낸드 (정펌)"},
        {&ANDROID,                    "ANDROID",                    "Lineage (안드로이드)"},
        {&LAKKA,                      "LAKKA",                      "Lakka (에뮬레이터)"},
        {&UBUNTU,                     "UBUNTU",                     "Ubuntu (리눅스)"},
        {&REBOOT,                     "REBOOT",                     "Hekate (메인 메뉴)"},
        {&SHUTDOWN,                   "SHUTDOWN",                   "시스템 종료"},
        {&BOOT_ENTRY,                 "BOOT_ENTRY",                 "로 재부팅, 해당 OS 설치시 사용 가능합니다"},
        /* ASAP Packages */
        {&UPDATE_ASAP_1,              "UPDATE_ASAP_1",              "테스터 버전을 설치합니다."},
        {&UPDATE_ASAP_2,              "UPDATE_ASAP_2",              "Atmosphère 와 Hekate만 업데이트됩니다!"},
        {&UPDATE_ASAP_3,              "UPDATE_ASAP_3",              "반드시 시스템 모듈을 모두 끄세요."},
        {&UPDATE_ASAP_4,              "UPDATE_ASAP_4",              "재부팅 시, ATLAS로 자동 적용됩니다."},
        // Informations
        {&CFWBOOT_TITLE,              "CFWBOOT_TITLE",              "Extra Setting+"},
        {&HEKATE_TITLE,               "HEKATE_TITLE",               "Hekate"},
        {&L4T_TITLE,                  "L4T_TITLE",                  "L4T"},
        {&VOLUME_INFO,                "VOLUME_INFO",                "독 모드 볼륨 조절 지원"},
        {&LAUNCHER_INFO,              "LAUNCHER_INFO",              "재부팅 · UMS 도구"},
        {&CFWBOOT_INFO,               "CFWBOOT_INFO",               "에서 선택한 낸드로만 커펌 부팅"},
        {&HEKATE_INFO,                "HEKATE_INFO",                "홈 메뉴, UMS (이동식 디스크)로 재부팅"},
        // Configs
        {&MAINMENU_INFO,              "MAINMENU_INFO",              "시스템 확인 및 울트라핸드의 설정을 변경"},
        {&INFO,                       "INFO",                       "정보"},
        {&ABOUT_INFO,                 "ABOUT_INFO",                 "커스텀 패키지 테슬라 메뉴"},
        {&OLD_DEVICE,                 "OLD_DEVICE",                 "구형"},
        {&NEW_DEVICE,                 "NEW_DEVICE",                 "개선판"},
        {&DF_THEME,                   "DF_THEME",                   "Black"},
        {&UPDATE_ULTRA_INFO,          "UPDATE_ULTRA_INFO",          "다음 구성 요소를 전부 업데이트합니다"},
        {&UPDATE_TEST_WARN,           "UPDATE_TEST_WARN",           "테스터 팩에선 오류가 발생할 수 있습니다"},
        {&HIGHLIGHT_LINE,             "HIGHLIGHT_LINE",             "¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯  　　　　　　"},
        {&UPDATE_TEST_INFO,           "UPDATE_TEST_INFO",           "ASAP을 테스터 버전으로 교체합니다"},
        {&UPDATE_TEST_NOTICE,         "UPDATE_TEST_NOTICE",         "다운로드 이후 자동으로 재부팅합니다"},
        // ETC.
        {&OVERRIDE_SELECTION,         "OVERRIDE_SELECTION",         "설정 안 함"},
        {&AUTO_SELECTION,             "AUTO_SELECTION",             "자동"},
        // Package-System Clock+
        {&ENABLED,                    "ENABLED",                    "활성화"},
        {&SCPLUS_INFO,                "SCPLUS_INFO",                "Sys-ClkOC Toolkitloader.kip 관리 도구"},
        {&SC_STATUS,                  "SC_STATUS",                  "앱의 사용 여부 선택"},
        {&LDR_TOOL,                   "LDR_TOOL",                   "Loader.kip 편집 도구"},
        {&LDR_INFO,                   "LDR_INFO",                   "선택시 자동 재부팅"},
        {&TOOL_WARNING_1,             "TOOL_WARNING_1",             "낸드에 치명적인 손상을 초래할 수 있습니다"},
        {&TOOL_WARNING_2,             "TOOL_WARNING_2",             "시스낸드에서의 편집/사용을 피해주십시오"},
        {&CPU_VOLTAGE,                "CPU_VOLTAGE",                "CPU 전압 허용 범위"},
        {&CPU_INFO,                   "CPU_INFO",                   "단위: mV"},
        {&CPU_CHART,                  "CPU_CHART",                  "　 구분"},
        {&USED,                       "USED",                       "사용중"},
        // Package-Extra Setting+
        {&ES_DISABLED,                "ES_DISABLED",                "비활성화"},
        {&CD_LAUNCHER,                "CD_LAUNCHER",                "런처 생성/제거"},
        {&INSTALL_PATH,               "INSTALL_PATH",               "설치 경로"},
        {&INSTALL_VER,                "INSTALL_VER",                "설치 버전"},
        {&RBOOT_OPTION,               "RBOOT_OPTION",               "재부팅 옵션"},
        {&CD_LAUNCHER_INFO,           "CD_LAUNCHER_INFO",           "L4T 런처의 세부 설정을 조정합니다"},
        {&AND_LCR_INFO,               "AND_LCR_INFO",               "Android 기본 값을 생성/제거"},
        {&UBU_LCR_INFO,               "UBU_LCR_INFO",               "Ubuntu 기본 값을 생성/제거"},
        {&SET_OS_NAND,                "SET_OS_NAND",                "OS가 설치된 낸드를 설정"},
        {&REBOOT_BOOTLOADER,          "REBOOT_BOOTLOADER",          "재부팅 부트로더를 선택"},
        {&USB_HS_INFO,                "USB_HS_INFO",                "USB 3.0 지원 유무 설정"},
        {&DDR_INFO,                   "DDR_INFO",                   "삼성 SD 카드인 경우에만 활성화"},
        {&UNSURE_INFO,                "UNSURE_INFO",                "잘 모르는 경우 설정하지 마세요"},
        {&SET_UBU_VER,                "SET_UBU_VER",                "설치한 Ubuntu 버전을 반드시 지정"},
        {&EXTRA_SETTING,              "EXTRA_SETTING",              "커스텀 펌웨어 관련 설정을 진행합니다"},
        {&CFW_CFG,                    "CFW_CFG",                    "시스템 세부 설정 편집"},
        {&CFW_CFG_INFO,               "CFW_CFG_INFO",               "커스텀 펌웨어의 여러 기능을 설정합니다"},
        {&DEFAULT_NAND,               "DEFAULT_NAND",               "기본 낸드"},
        {&DEFAULT_NAND_INFO,          "DEFAULT_NAND_INFO",          "Atmosphère 부팅 낸드 설정"},
        {&NO_EMUNAND,                 "NO_EMUNAND",                 "에뮤낸드 없음"},
        {&ADD_ENTRY,                  "ADD_ENTRY",                  "부팅 런처"},
        {&ADD_ENTRY_INFO,             "ADD_ENTRY_INFO",             "Hekate - Moon 런처 항목 설정"},
        {&MOON_LAUNCHER,              "MOON_LAUNCHER",              "Moon 런처"},
        {&MOON_ENTRY,                 "MOON_ENTRY",                 "부팅 엔트리"},
        {&MOON_LAUNCHER_INFO,         "MOON_LAUNCHER_INFO",         "각 엔트리의 아이콘은 자동으로 설정됩니다"},
        {&MOON_LAUNCHER_WARN_1,       "MOON_LAUNCHER_WARN_1",       "L4T를 부팅하려면 OS의 설치가 필요합니다"},
        {&MOON_LAUNCHER_WARN_2,       "MOON_LAUNCHER_WARN_2",       "Ubuntu의 경우, 버전을 맞춰 생성하세요"},
        {&MOON_CFW,                   "MOON_CFW",                   "기본 낸드로 설정된 커펌 (자동 생성)"},
        {&MOON_STOCK,                 "MOON_STOCK",                 "커펌 모듈 OFF 정펌 (자동 생성)"},
        {&MOON_ANDROID,               "MOON_ANDROID",               "안드로이드 (Lineage OS) 부팅 엔트리"},
        {&MOON_LAKKA,                 "MOON_LAKKA",                 "Libretro 기반 Lakka 부팅 엔트리"},
        {&MOON_UBUNTU,                "MOON_UBUNTU",                "Switchroot 포팅 LINUX 부팅 엔트리"},
        {&SYSTEM_CFG,                 "SYSTEM_CFG",                 "시스템 설정"},
        {&SYSTEM_CFG_INFO,            "SYSTEM_CFG_INFO",            "커펌 시스템 값 설정"},
        {&HEKATE_BOOT,                "HEKATE_BOOT",                "부팅"},
        {&HEKATE_BOOT_INFO,           "HEKATE_BOOT_INFO",           "시작 관련 설정을 변경합니다"},
        {&HEKATE_BACKLIGHT_INFO,      "HEKATE_BACKLIGHT_INFO",      "홈 화면의 백라이트 밝기를 조절합니다"},
        {&HEKATE_BACKLIGHT,           "HEKATE_BACKLIGHT",           "화면 밝기"},
        {&AUTO_HOS_OFF_SEC,           "AUTO_HOS_OFF_SEC",           "시스템 종료 후"},
        {&AUTO_HOS_OFF_VAL,           "AUTO_HOS_OFF_VAL",           "자동으로 깨어나지 않도록합니다"},
        {&HEKATE_AUTO_HOS_OFF,        "HEKATE_AUTO_HOS_OFF",        "HOS 종료"},
        {&BOOT_WAIT_SEC,              "BOOT_WAIT_SEC",              "1초 이상"},
        {&BOOT_WAIT_VAL,              "BOOT_WAIT_VAL",              " 볼륨 입력으로 Hekate로 이동 가능"},
        {&HEKATE_BOOT_WAIT,           "HEKATE_BOOT_WAIT",           "부팅 화면 대기 시간"},
        {&BOOT_WAIT_SECOND,           "BOOT_WAIT_SECOND",           "초"},
        {&AUTOBOOT_CFW_SEC,           "AUTOBOOT_CFW_SEC",           " 버튼"},
        {&AUTOBOOT_CFW_VAL,           "AUTOBOOT_CFW_VAL",           "재기동 시 Atmosphère로 재시작합니다"},
        {&HEKATE_AUTOBOOT,            "HEKATE_AUTOBOOT",            "자동 부팅"},
        {&SYSTEM_PAGE,                "SYSTEM_PAGE",                "시스템"},
        {&DMNT_CHEAT,                 "DMNT_CHEAT",                 "dmnt 치트"},
        {&DMNT_TOGGLE,                "DMNT_TOGGLE",                "dmnt 토글 저장"},
        {&NO_GC_INFO,                 "NO_GC_INFO",                 "게임 카드 설치를 비활성화합니다"},
        {&NO_GC_WARN,                 "NO_GC_WARN",                 "슬립 해제시 카트리지를 인식하면 오류 발생"},
        {&NO_GC,                      "NO_GC",                      "카트리지 차단"},
        {&REC_INFO,                   "REC_INFO",                   " 버튼 녹화 설정을 변경합니다"},
        {&REC_WARN,                   "REC_WARN",                   "값에 따라 녹화 시간이 변동될 수 있습니다"},
        {&SYSMMC_ONLY,                "SYSMMC_ONLY",                "시스낸드 전용"},
        {&ENABLED_EMUMMC,             "ENABLED_EMUMMC",             "에뮤낸드 = 기본 적용"},
        {&EXOSPHERE_INFO,             "EXOSPHERE_INFO",             "본체의 시리얼 넘버를 변조합니다"},
        {&EXOSPHERE,                  "EXOSPHERE",                  "시리얼 변조"},
        {&DNS_INFO,                   "DNS_INFO",                   "닌텐도 서버와 연결을 차단합니다"},
        {&DNS_MITM,                   "DNS_MITM",                   "서버 차단"},
        {&APPLY_RESET_SEC,            "APPLY_RESET_SEC",            "설정 값 적용리셋"},
        {&APPLY_RESET_VAL,            "APPLY_RESET_VAL",            "자동으로 재부팅 됩니다"},
        {&APPLY_CFG,                  "APPLY_CFG",                  "설정 적용"},
        {&RESET_CFG,                  "RESET_CFG",                  "기본 값으로 되돌리기"},
        {&HB_MENU,                    "HB_MENU",                    "홈브류 설정"},
        {&HB_MENU_INFO,               "HB_MENU_INFO",               "Sphaira 실행 버튼"},
        {&FULL_MEMORY,                "FULL_MEMORY",                "풀 메모리 모드"},
        {&FULL_MEMORY_INFO,           "FULL_MEMORY_INFO",           "메모리 제한 없음"},
        {&FULL_MEMORY_REC,            "FULL_MEMORY_REC",            "권장"},
        {&FULL_MEMORY_FORWARDER_VAL,  "FULL_MEMORY_FORWARDER_VAL",  "sphaira 열기  sphaira 아이콘으로 이동"},
        {&FULL_MEMORY_FORWARDER_CTN,  "FULL_MEMORY_FORWARDER_CTN",  "  버튼 입력  바로가기 설치  설치"},
        {&FULL_MEMORY_KEY,            "FULL_MEMORY_KEY",            "(타이틀) +  홀드 진입을 활성화합니다"},
        {&APPLET_MEMORY,              "APPLET_MEMORY",              "애플릿 모드"},
        {&APPLET_MEMORY_INFO,         "APPLET_MEMORY_INFO",         "메모리 제한됨"},
        {&APPLET_MEMORY_VAL,          "APPLET_MEMORY_VAL",          " (앨범),  (유저) 진입 설정을 변경합니다"},
        {&APPLET_MEMORY_KEY,          "APPLET_MEMORY_KEY",          "기본:  (앨범) +  홀드"},
        {&APPLET_HB_MENU_ICON,        "APPLET_HB_MENU_ICON",        "진입 아이콘"},
        {&APPLET_HB_MENU_KEY,         "APPLET_HB_MENU_KEY",         "진입 커맨드"},
        {&S_LANG_PATCH,               "S_LANG_PATCH",               "한글 패치"},
        {&S_TRANSLATE,                "S_TRANSLATE",                "번역 업데이트"},
        {&S_LANG,                     "S_LANG",                     "ko"},
        {&APPLET_ALBUM,               "APPLET_ALBUM",               "앨범"},
        {&APPLET_USER,                "APPLET_USER",                "유저"},
        {&APPLET_KEY_HOLD,            "APPLET_KEY_HOLD",            "홀드"},
        {&APPLET_KEY_CLICK,           "APPLET_KEY_CLICK",           "원클릭"},
        {&SD_CLEANUP,                 "SD_CLEANUP",                 "SD 카드 정리"},
        {&SD_CLEANUP_INFO,            "SD_CLEANUP_INFO",            "Ultrahand 초기화, 정크 삭제"},
        {&NORMAL_DEVICE,              "NORMAL_DEVICE",              "일반 기기"},
        {&PATCH_DEVICE,               "PATCH_DEVICE",               "8GB 기기"},
        {&RAM_PATCH_WARN,             "RAM_PATCH_WARN",             "패치시 일반 기기는 작동하지 않게됩니다"},
        {&RAM_PATCH,                  "RAM_PATCH",                  "8GB 패치"},
        // Package-Quick Guide+
        {&QUICK_GUIDE_INFO,           "QUICK_GUIDE_INFO",           "간이 설명서오류 코드"},
        {&QUICK_GUIDE,                "QUICK_GUIDE",                "가이드"},
        {&KEYMAP_INFO,                "KEYMAP_INFO",                "전반적인 터치 조작 가능"},
        {&KEY_MOVE,                   "KEY_MOVE",                   "이동"},
        {&KEY_MENU,                   "KEY_MENU",                   "메뉴"},
        {&KEY_SELECT,                 "KEY_SELECT",                 "선택"},
        {&KEYGAP_1,                   "KEYGAP_1",                   "    　　"},
        {&KEYGAP_2,                   "KEYGAP_2",                   "    "},
        {&KEYGAP_3,                   "KEYGAP_3",                   "    　　"},
        {&PACK_INFO,                  "PACK_INFO",                  "패키지 설명서"},
        {&USEFUL,                     "USEFUL",                     "유용한 기능 (앱 실행 상태)"},
        {&HIDE_SPHAIRA,               "HIDE_SPHAIRA",               "'앱 숨기기:' = '  숨기기  켬'"},
        {&FORWARDER_SPHAIRA,          "FORWARDER_SPHAIRA",          "'바로가기 설치:' = '홈브류 +   바로가기 설치'"},
        {&APP_INSTALL,                "APP_INSTALL",                "앱 설치"},
        {&PC_INSTALL,                 "PC_INSTALL",                 "  기타  FTP·MTP 설치  파일 넣기"},
        {&FTP_INSTALL,                "FTP_INSTALL",                "서버 연결  Install 폴더에 파일 끌어넣기"},
        {&MTP_INSTALL,                "MTP_INSTALL",                "PC 연결  Install 폴더에 파일 끌어넣기"},
        {&USB_INSTALL,                "USB_INSTALL",                "'2.외장하드:' = '파일 탐색기  마운트  파일 선택'"},
        {&HDD_INSTALL_1,              "HDD_INSTALL_1",              "하드 연결  파일 탐색기    고급 "},
        {&HDD_INSTALL_2,              "HDD_INSTALL_2",              "마운트  하드 선택  파일 선택  예"},
        {&ERROR_INFO,                 "ERROR_INFO",                 "오류 코드"},
        {&ALBUM_INFO,                 "ALBUM_INFO",                 "앨범 오류"},
        {&ALBUM_ERROR_1,              "ALBUM_ERROR_1",              "sphairahbmenu 혹은 dmnt의 구성 불량"},
        {&ALBUM_ERROR_2,              "ALBUM_ERROR_2",              "애플릿 모드 사용중인 경우, 메모리 부족 충돌"},
        {&CPUDEAD_INFO,               "CPUDEAD_INFO",               "CPU 파손"},
        {&CPU_ERROR,                  "CPU_ERROR",                  "CPU에 복구 불가능한 손상이 발생"},
        {&SYSTEM_ERROR,               "SYSTEM_ERROR",               "시스템 오류"},
        {&MODULE_ERROR,               "MODULE_ERROR",               "모듈 오류"},
        {&MODULE_INFO_1,              "MODULE_INFO_1",              "스위치 모듈 오류시 안내문 참고"},
        {&MODULE_INFO_2,              "MODULE_INFO_2",              "앱이 아닌 스위치 모듈 오류가 발생한 경우"},
        {&MODULE_INFO_3,              "MODULE_INFO_3",              "시스템 파일 손상, 낸드 리빌딩 필요"},
        {&MODULE_INFO_4,              "MODULE_INFO_4",              "에뮤낸드 파일 손상, 재생성으로 해결"},
        {&SYSNAND_INFO,               "SYSNAND_INFO",               "시스낸드"},
        {&EMUNAND_INFO,               "EMUNAND_INFO",               "에뮤낸드"},
        {&NORMAL_ERROR,               "NORMAL_ERROR",               "자주 발생하는 오류"},
        {&APP_MODULE_ERROR,           "APP_MODULE_ERROR",           "앱 모듈 오류"},
        {&SWITCH_MODULE_ERROR,        "SWITCH_MODULE_ERROR",        "스위치 모듈 오류"},
        {&PREV_PAGE,                  "PREV_PAGE",                  "이전 페이지"},
        {&NEXT_PAGE,                  "NEXT_PAGE",                  "다음 페이지"},
        {&VER_ERR,                    "VER_ERR",                    "버전 오류"},
        {&SD_ERR,                     "SD_ERR",                     "SD 카드 오류"},
        {&STORAGE_ERR,                "STORAGE_ERR",                "저장소 오류"},
        {&MISC_ERR,                   "MISC_ERR",                   "기타 오류"},
        {&NSS_ERR,                    "NSS_ERR",                    "Switch Online Service 오류"},
        {&SERVER_ERR,                 "SERVER_ERR",                 "서버 오류"},
        {&NETWORK_ERR,                "NETWORK_ERR",                "네트워크 오류"},
        {&FORWARDER_ERR,              "FORWARDER_ERR",              "글로벌 오류"},
        {&FORWARDER_INFO,             "FORWARDER_INFO",             "NSP 포워더 (바로가기)"},
        {&APP_ERR,                    "APP_ERR",                    "유저 환경에 따라 발생하는 오류"},
        {&APP_MODULE,                 "APP_MODULE",                 "홈브류오버레이용 시스템 모듈"},
        {&SCROLL,                     "SCROLL",                     "스크롤"},
        {&SWITCH_MODULE,              "SWITCH_MODULE",              "스위치 시스템 모듈"},
        // Overlays-Custom infomation
        {&OVLSYSMODULE,               "OVLSYSMODULE",               "모듈 관리자"},
        {&EDIZON,                     "EDIZON",                     "치트 매니저"},
        {&EMUIIBO,                    "EMUIIBO",                    "가상 아미보"},
        {&FIZEAU,                     "FIZEAU",                     "색감 매니저"},
        {&NXFANCONTROL,               "NXFANCONTROL",               "쿨링 시스템"},
        {&FPSLOCKER,                  "FPSLOCKER",                  "고정 프레임"},
        {&LDNMITM,                    "LDNMITM",                    "LAN 플레이"},
        {&QUICKNTP,                   "QUICKNTP",                   "시간 동기화"},
        {&REVERSENX,                  "REVERSENX",                  "모드 전환기"},
        {&STATUSMONITOR,              "STATUSMONITOR",              "상태 모니터"},
        {&STUDIOUSPANCAKE,            "STUDIOUSPANCAKE",            "빠른 재부팅"},
        {&SYSCLK,                     "SYSCLK",                     "클럭 조정기"},
        {&SYSDVR,                     "SYSDVR",                     "USB 스트리밍"},
        {&SYSPATCH,                   "SYSPATCH",                   "패치 시스템"},
        #endif

        {&INCOMPATIBLE_WARNING,       "INCOMPATIBLE_WARNING",       "AMS 1.10.0 미호환"},
        {&SYSTEM_RAM,                 "SYSTEM_RAM",                 "시스템 RAM"},
        {&FREE,                       "FREE",                       "여유"},
        {&DEFAULT_CHAR_WIDTH,         "DEFAULT_CHAR_WIDTH",         "0.33"},
        {&UNAVAILABLE_SELECTION,      "UNAVAILABLE_SELECTION",      "설정 없음"},
        {&ON,                         "ON",                         "\uE14B"},
        {&OFF,                        "OFF",                        "\uE14C"},
        {&OK,                         "OK",                         "확인"},
        {&BACK,                       "BACK",                       "뒤로"},
        {&HIDE,                       "HIDE",                       "숨기기"},
        {&CANCEL,                     "CANCEL",                     "취소"},
        {&GAP_1,                      "GAP_1",                      "     "},
        {&GAP_2,                      "GAP_2",                      "  "},

        #if USING_WIDGET_DIRECTIVE
        {&SUNDAY,                     "SUNDAY",                     " 日"},
        {&MONDAY,                     "MONDAY",                     " 月"},
        {&TUESDAY,                    "TUESDAY",                    " 火"},
        {&WEDNESDAY,                  "WEDNESDAY",                  " 水"},
        {&THURSDAY,                   "THURSDAY",                   " 木"},
        {&FRIDAY,                     "FRIDAY",                     " 金"},
        {&SATURDAY,                   "SATURDAY",                   " 土"},
        {&JANUARY,                    "JANUARY",                    "1월"},
        {&FEBRUARY,                   "FEBRUARY",                   "2월"},
        {&MARCH,                      "MARCH",                      "3월"},
        {&APRIL,                      "APRIL",                      "4월"},
        {&MAY,                        "MAY",                        "5월"},
        {&JUNE,                       "JUNE",                       "6월"},
        {&JULY,                       "JULY",                       "7월"},
        {&AUGUST,                     "AUGUST",                     "8월"},
        {&SEPTEMBER,                  "SEPTEMBER",                  "9월"},
        {&OCTOBER,                    "OCTOBER",                    "10월"},
        {&NOVEMBER,                   "NOVEMBER",                   "11월"},
        {&DECEMBER,                   "DECEMBER",                   "12월"},
        {&SUN,                        "SUN",                        " 日"},
        {&MON,                        "MON",                        " 月"},
        {&TUE,                        "TUE",                        " 火"},
        {&WED,                        "WED",                        " 水"},
        {&THU,                        "THU",                        " 木"},
        {&FRI,                        "FRI",                        " 金"},
        {&SAT,                        "SAT",                        " 土"},
        {&JAN,                        "JAN",                        "1월"},
        {&FEB,                        "FEB",                        "2월"},
        {&MAR,                        "MAR",                        "3월"},
        {&APR,                        "APR",                        "4월"},
        {&MAY_ABBR,                   "MAY_ABBR",                   "5월"},
        {&JUN,                        "JUN",                        "6월"},
        {&JUL,                        "JUL",                        "7월"},
        {&AUG,                        "AUG",                        "8월"},
        {&SEP,                        "SEP",                        "9월"},
        {&OCT,                        "OCT",                        "10월"},
        {&NOV,                        "NOV",                        "11월"},
        {&DEC,                        "DEC",                        "12월"},
        #endif
    };

    static constexpr size_t LANG_TABLE_SIZE = sizeof(LANG_TABLE) / sizeof(LANG_TABLE[0]);


    // --- reinitializeLangVars: just a loop now ---

    //#if IS_LAUNCHER_DIRECTIVE
    void reinitializeLangVars() {
        for (size_t i = 0; i < LANG_TABLE_SIZE; ++i)
            *LANG_TABLE[i].var = LANG_TABLE[i].defaultVal;
    }
    //#endif


    // --- parseLanguage: same loop, different source ---

    void parseLanguage(const std::string& langFile) {
        std::unordered_map<std::string, std::string> jsonMap;
        if (!parseJsonToMap(langFile, jsonMap)) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to parse language file: " + langFile);
            #endif
            return;
        }
        for (size_t i = 0; i < LANG_TABLE_SIZE; ++i) {
            auto it = jsonMap.find(LANG_TABLE[i].key);
            *LANG_TABLE[i].var = (it != jsonMap.end()) ? it->second : LANG_TABLE[i].defaultVal;
        }
    }


    // --- localizeTimeStr and applyLangReplacements unchanged ---

    #if USING_WIDGET_DIRECTIVE
    void localizeTimeStr(char* timeStr) {
        static std::unordered_map<std::string, std::string*> mappings = {
            {"Sun", &SUN}, {"Mon", &MON}, {"Tue", &TUE}, {"Wed", &WED},
            {"Thu", &THU}, {"Fri", &FRI}, {"Sat", &SAT},
            {"Sunday", &SUNDAY}, {"Monday", &MONDAY}, {"Tuesday", &TUESDAY},
            {"Wednesday", &WEDNESDAY}, {"Thursday", &THURSDAY},
            {"Friday", &FRIDAY}, {"Saturday", &SATURDAY},
            {"Jan", &JAN}, {"Feb", &FEB}, {"Mar", &MAR}, {"Apr", &APR},
            {"May", &MAY_ABBR}, {"Jun", &JUN}, {"Jul", &JUL}, {"Aug", &AUG},
            {"Sep", &SEP}, {"Oct", &OCT}, {"Nov", &NOV}, {"Dec", &DEC},
            {"January", &JANUARY}, {"February", &FEBRUARY}, {"March", &MARCH},
            {"April", &APRIL}, {"June", &JUNE}, {"July", &JULY},
            {"August", &AUGUST}, {"September", &SEPTEMBER}, {"October", &OCTOBER},
            {"November", &NOVEMBER}, {"December", &DECEMBER}
        };

        std::string timeStrCopy = timeStr;
        size_t pos;
        for (const auto& mapping : mappings) {
            pos = timeStrCopy.find(mapping.first);
            while (pos != std::string::npos) {
                timeStrCopy.replace(pos, mapping.first.length(), *(mapping.second));
                pos = timeStrCopy.find(mapping.first, pos + mapping.second->length());
            }
        }
        strcpy(timeStr, timeStrCopy.c_str());
    }
    #endif

    void applyLangReplacements(std::string& text, bool isValue) {
        if (isValue) {
            if (text.length() == 2 && text[0] == 'O') {
                if (text[1] == 'n') { text = ON; return; }
                if (text[1] == 'f' && text[2] == 'f') { text = OFF; return; }
            }
        }
        #if IS_LAUNCHER_DIRECTIVE
        else {
            switch (text.length()) {
                case 5:  if (text == "LAKKA")     { text = LAKKA;     } break;
                case 6:  if (text == "Reboot") { text = REBOOT; } else if (text == "UBUNTU") { text = UBUNTU; } break;
                case 7:  if (text == "CFWBOOT") { text = CFWBOOT; } else if (text == "OFWBOOT") { text = OFWBOOT; } else if (text == "ANDROID") { text = ANDROID; } break;
                case 8:  if (text == "Shutdown")  { text = SHUTDOWN;  } break;
                case 9:  if (text == "Reboot To") { text = REBOOT_TO; } break;
                case 10: if (text == "Boot Entry"){ text = BOOT_ENTRY;} break;
            }
        }
        #endif
    }
    
    
    
    // Predefined hexMap
    //const std::array<int, 256> hexMap = [] {
    //    std::array<int, 256> map = {0};
    //    map['0'] = 0; map['1'] = 1; map['2'] = 2; map['3'] = 3; map['4'] = 4;
    //    map['5'] = 5; map['6'] = 6; map['7'] = 7; map['8'] = 8; map['9'] = 9;
    //    map['A'] = 10; map['B'] = 11; map['C'] = 12; map['D'] = 13; map['E'] = 14; map['F'] = 15;
    //    map['a'] = 10; map['b'] = 11; map['c'] = 12; map['d'] = 13; map['e'] = 14; map['f'] = 15;
    //    return map;
    //}();
    
    
    // Prepare a map of default settings
    std::map<const std::string, std::string> defaultThemeSettingsMap = {
        {"default_overlay_color", whiteColor},
        {"default_package_color", whiteColor},
        {"default_script_color", whiteColor},
        {"clock_color", whiteColor},
        {"temperature_color", whiteColor},
        {"battery_color", "3CDD88"},
        {"battery_charging_color", "00FF00"},
        {"battery_low_color", "F63345"},
        {"widget_backdrop_alpha", "13"},
        {"widget_backdrop_color", blackColor},
        {"bg_alpha", "13"},
        {"bg_color", blackColor},
        {"separator_alpha", "15"},
        {"separator_color", "404040"},
        {"text_separator_color", "404040"},
        {"text_color", whiteColor},
        {"notification_text_color", whiteColor},
        {"header_text_color", whiteColor},
        {"header_separator_color", whiteColor},
        {"star_color", "FFAA17"},
        {"selection_star_color", "FFAA17"},
        {"bottom_button_color", whiteColor},
        {"bottom_text_color", whiteColor},
        {"bottom_separator_color", whiteColor},
        {"top_separator_color", "404040"},
        {"table_bg_color", "3F3F3F"},
        {"table_bg_alpha", "13"},
        {"table_section_text_color", "5DC5FB"},
        //{"table_info_text_color", "00FFDD"},
        {"table_info_text_color", whiteColor},
        {"warning_text_color", "F63345"},
        {"healthy_ram_text_color", "3CDD88"},
        {"neutral_ram_text_color", "FFAA17"},
        {"bad_ram_text_color", "F63345"},
        {"trackbar_slider_color", "606060"},
        {"trackbar_slider_border_color", "505050"},
        {"trackbar_slider_malleable_color", "A0A0A0"},
        {"trackbar_full_color", "00FFDD"},
        {"trackbar_empty_color", "404040"},
        {"overlay_text_color", whiteColor},
        {"ult_overlay_text_color", whiteColor},
        {"package_text_color", whiteColor},
        {"ult_package_text_color", whiteColor},
        {"banner_version_text_color", greyColor},
        {"overlay_version_text_color", greyColor},
        {"ult_overlay_version_text_color", "00FFDD"},
        {"package_version_text_color", greyColor},
        {"ult_package_version_text_color", "00FFDD"},
        {"on_text_color", "00FFDD"},
        {"off_text_color", greyColor},
        {"invalid_text_color", "F63345"},
        {"inprogress_text_color", "3CDD88"},
        {"selection_text_color", whiteColor},
        {"selection_value_text_color", "00FFFF"},
        {"selection_bg_color", blackColor},
        {"selection_bg_alpha", "13"},
        {"scrollbar_color", "555555"},
        {"scrollbar_wall_color", greyColor},
        {"highlight_color_1", "2288CC"},
        {"highlight_color_2", "88FFFF"},
        {"highlight_color_3", "3CDD88"},
        {"highlight_color_4", "2CD2B1"},
        {"click_text_color", whiteColor},
        {"click_alpha", "13"},
        {"click_color", "2CD2B1"},
        {"progress_alpha", "7"},
        {"progress_color", "2597F7"},
        {"invert_bg_click_color", FALSE_STR},
        //{"disable_selection_bg", FALSE_STR},
        //{"disable_selection_value_color", FALSE_STR},
        //{"disable_colorful_logo", FALSE_STR},
        {"logo_color_1", "EAEAEA"},
        {"logo_color_2", whiteColor},
        {"dynamic_logo_color_1", "C9F1FF"},
        {"dynamic_logo_color_2", "4BCDF9"},
        // ASAP: Packages coler.
        {"accent_text_color", "00FFDD"},
        {"notice_text_color", "00FFFF"},
        // fpslocker
        {"fps_accent_color", "FF3333"},
        {"fps_faild_color", "FF9999"},
        // status-monitor-overlay
        {"stm_bg_alpha", "10"},
        {"stm_bg_color", "111111"},
        {"stm_mimicbg_color", "000000"},
        {"stm_accent_color", "00FFDD"},
        {"stm_section_color", "22DDFF"},
        {"stm_text_color", whiteColor}
    };
    
    bool isNumericCharacter(char c) {
        return std::isdigit(c);
    }
    
    bool isValidHexColor(const std::string& hexColor) {
        // Check if the string is a valid hexadecimal color of the format "#RRGGBB"
        if (hexColor.size() != 6) {
            return false; // Must be exactly 6 characters long
        }
        
        for (char c : hexColor) {
            if (!isxdigit(c)) {
                return false; // Must contain only hexadecimal digits (0-9, A-F, a-f)
            }
        }
        
        return true;
    }
    
    
    std::atomic<bool> refreshWallpaperNow(false);
    std::atomic<bool> refreshWallpaper(false);
    std::vector<u8> wallpaperData; 
    std::atomic<bool> inPlot(false);
    
    std::mutex wallpaperMutex;
    std::condition_variable cv;
    
    bool loadRGBA8888toRGBA4444(const std::string& filePath, u8* dst, size_t srcSize) {
        FILE* f = fopen(filePath.c_str(), "rb");
        if (!f) return false;
    
        const uint8x8_t mask = vdup_n_u8(0xF0);
        constexpr size_t chunkBytes = 128 * 1024;
        uint8_t chunkBuffer[chunkBytes];
        size_t totalRead = 0;
    
        setvbuf(f, nullptr, _IOFBF, chunkBytes);
    
        while (totalRead < srcSize) {
            const size_t toRead = std::min(srcSize - totalRead, chunkBytes);
            const size_t bytesRead = fread(chunkBuffer, 1, toRead, f);
            if (bytesRead == 0) { fclose(f); return false; }
    
            const uint8_t* src = chunkBuffer;
            size_t i = 0;
            for (; i + 16 <= bytesRead; i += 16) {
                uint8x16_t data = vld1q_u8(src + i);
                uint8x8x2_t sep = vuzp_u8(vget_low_u8(data), vget_high_u8(data));
                vst1_u8(dst, vorr_u8(vand_u8(sep.val[0], mask), vshr_n_u8(sep.val[1], 4)));
                dst += 8;
            }
            for (; i + 1 < bytesRead; i += 2)
                *dst++ = (src[i] & 0xF0) | (src[i+1] >> 4);
    
            totalRead += bytesRead;
        }
    
        fclose(f);
        return true;
    }

    
    void loadWallpaperFile(const std::string& filePath, s32 width, s32 height) {
        const size_t srcSize = width * height * 4;
        wallpaperData.resize(srcSize / 2);
        if (!isFileOrDirectory(filePath) ||
            !loadRGBA8888toRGBA4444(filePath, wallpaperData.data(), srcSize))
            wallpaperData.clear();
    }
    

    void loadWallpaperFileWhenSafe() {
        if (expandedMemory && !inPlot.load(std::memory_order_acquire) && !refreshWallpaper.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lock(wallpaperMutex);
            cv.wait(lock, [] { return !inPlot.load(std::memory_order_acquire) && !refreshWallpaper.load(std::memory_order_acquire); });
            if (wallpaperData.empty() && isFileOrDirectory(WALLPAPER_PATH)) {
                loadWallpaperFile(WALLPAPER_PATH);
            }
        }
    }


    void reloadWallpaper() {
        // Signal that wallpaper is being refreshed
        refreshWallpaper.store(true, std::memory_order_release);
        
        // Lock the mutex for condition waiting
        std::unique_lock<std::mutex> lock(wallpaperMutex);
        
        // Wait for inPlot to be false before reloading the wallpaper
        cv.wait(lock, [] { return !inPlot.load(std::memory_order_acquire); });
        
        // Clear the current wallpaper data
        wallpaperData.clear();
        
        // Reload the wallpaper file
        if (isFileOrDirectory(WALLPAPER_PATH)) {
            loadWallpaperFile(WALLPAPER_PATH);
        }
        
        // Signal that wallpaper has finished refreshing
        refreshWallpaper.store(false, std::memory_order_release);
        
        // Notify any waiting threads
        cv.notify_all();
    }

    
    
    // Global variables for FPS calculation
    //double lastTimeCount = 0.0;
    //int frameCount = 0;
    //float fps = 0.0f;
    //double elapsedTime = 0.0;
    
    std::atomic<bool> themeIsInitialized(false); // for loading the theme once in OverlayFrame / HeaderOverlayFrame
    
    // Variables for touch commands
    std::atomic<bool> touchingBack(false);
    std::atomic<bool> touchingSelect(false);
    std::atomic<bool> touchingNextPage(false);
    std::atomic<bool> touchingMenu(false);
    std::atomic<bool> shortTouchAndRelease(false);
    std::atomic<bool> longTouchAndRelease(false);
    std::atomic<bool> simulatedBack(false);
    //bool simulatedBackComplete = true;
    std::atomic<bool> simulatedSelect(false);
    //bool simulatedSelectComplete = true;
    std::atomic<bool> simulatedNextPage(false);
    //std::atomic<bool> simulatedNextPageComplete(true);
    std::atomic<bool> simulatedMenu(false);
    //bool simulatedMenuComplete = true;
    std::atomic<bool> stillTouching(false);
    std::atomic<bool> interruptedTouch(false);
    std::atomic<bool> touchInBounds(false);
    
    
#if USING_WIDGET_DIRECTIVE
    // Battery implementation
    bool powerInitialized = false;
    bool powerCacheInitialized;
    uint32_t powerCacheCharge;
    //float powerConsumption;
    bool powerCacheIsCharging;
    PsmSession powerSession;
    
    // Define variables to store previous battery charge and time
    //uint32_t prevBatteryCharge = 0;
    //s64 timeOut = 0;
    
    
    std::atomic<uint32_t> batteryCharge{0};
    std::atomic<bool> isCharging{false};
    //bool validPower;
    
    
    
    bool powerGetDetails(uint32_t *_batteryCharge, bool *_isCharging) {
        static uint64_t last_call_ns = 0;
        
        // Ensure power system is initialized
        if (!powerInitialized) {
            return false;
        }
        
        // Get the current time in nanoseconds
        const uint64_t now_ns = armTicksToNs(armGetSystemTick());
        
        // 3 seconds in nanoseconds
        static constexpr uint64_t min_delay_ns = 3000000000ULL;
        
        // Check if enough time has elapsed or if cache is not initialized
        const bool useCache = (now_ns - last_call_ns <= min_delay_ns) && powerCacheInitialized;
        if (!useCache) {
            PsmChargerType charger = PsmChargerType_Unconnected;
            Result rc = psmGetBatteryChargePercentage(_batteryCharge);
            bool hwReadsSucceeded = R_SUCCEEDED(rc);
            
            if (hwReadsSucceeded) {
                rc = psmGetChargerType(&charger);
                hwReadsSucceeded &= R_SUCCEEDED(rc);
                *_isCharging = (charger != PsmChargerType_Unconnected);
                
                if (hwReadsSucceeded) {
                    // Update cache
                    powerCacheCharge = *_batteryCharge;
                    powerCacheIsCharging = *_isCharging;
                    powerCacheInitialized = true;
                    last_call_ns = now_ns; // Update last call time after successful hardware read
                    return true;
                }
            }
            
            // Use cached values if the hardware read fails
            if (powerCacheInitialized) {
                *_batteryCharge = powerCacheCharge;
                *_isCharging = powerCacheIsCharging;
                return hwReadsSucceeded; // Return false if hardware read failed but cache is valid
            }
            
            // Return false if cache is not initialized and hardware read failed
            return false;
        }
        
        // Use cached values if not enough time has passed
        *_batteryCharge = powerCacheCharge;
        *_isCharging = powerCacheIsCharging;
        return true; // Return true as cache is used
    }
    
    
    void powerInit(void) {
        uint32_t charge = 0;
        bool charging = false;
        
        powerCacheInitialized = false;
        powerCacheCharge = 0;
        powerCacheIsCharging = false;
        
        if (!powerInitialized) {
            Result rc = psmInitialize();
            if (R_SUCCEEDED(rc)) {
                rc = psmBindStateChangeEvent(&powerSession, 1, 1, 1);
                
                if (R_FAILED(rc))
                    psmExit();
                
                if (R_SUCCEEDED(rc)) {
                    powerInitialized = true;
                    ult::powerGetDetails(&charge, &charging);
                    ult::batteryCharge.store(charge, std::memory_order_release);
                    ult::isCharging.store(charging, std::memory_order_release);
                    //prevBatteryCharge = charge; // if needed
                }
            }
        }
    }
    
    void powerExit(void) {
        if (powerInitialized) {
            psmUnbindStateChangeEvent(&powerSession);
            psmExit();
            powerInitialized = false;
            powerCacheInitialized = false;
        }
    }
#endif
    
    
    // Temperature Implementation
    std::atomic<float> PCB_temperature{0.0f};
    std::atomic<float> SOC_temperature{0.0f};
    
    /*
    I2cReadRegHandler was taken from Switch-OC-Suite source code made by KazushiMe
    Original repository link (Deleted, last checked 15.04.2023): https://github.com/KazushiMe/Switch-OC-Suite
    */
    
    Result I2cReadRegHandler(u8 reg, I2cDevice dev, u16 *out) {
        struct readReg {
            u8 send;
            u8 sendLength;
            u8 sendData;
            u8 receive;
            u8 receiveLength;
        };
        
        I2cSession _session;
        
        Result res = i2cOpenSession(&_session, dev);
        if (res)
            return res;
        
        u16 val;
        
        struct readReg readRegister = {
            .send = 0 | (I2cTransactionOption_Start << 6),
            .sendLength = sizeof(reg),
            .sendData = reg,
            .receive = 1 | (I2cTransactionOption_All << 6),
            .receiveLength = sizeof(val),
        };
        
        res = i2csessionExecuteCommandList(&_session, &val, sizeof(val), &readRegister, sizeof(readRegister));
        if (res) {
            i2csessionClose(&_session);
            return res;
        }
        
        *out = val;
        i2csessionClose(&_session);
        return 0;
    }
    
    
    // Common helper function to read temperature (integer and fractional parts)
    Result ReadTemperature(float *temperature, u8 integerReg, u8 fractionalReg, bool integerOnly) {
        u16 rawValue;
        u8 val;
        s32 integerPart = 0;
        float fractionalPart = 0.0f;  // Change this to a float to retain fractional precision
        
        // Read the integer part of the temperature
        Result res = I2cReadRegHandler(integerReg, I2cDevice_Tmp451, &rawValue);
        if (R_FAILED(res)) {
            return res;  // Error during I2C read
        }
        
        val = (u8)rawValue;  // Cast the value to an 8-bit unsigned integer
        integerPart = val;    // Integer part of temperature in Celsius
        
        if (integerOnly)
        {
            *temperature = static_cast<float>(integerPart);  // Ensure it's treated as a float
            return 0;  // Return only integer part if requested
        }
        
        // Read the fractional part of the temperature
        res = I2cReadRegHandler(fractionalReg, I2cDevice_Tmp451, &rawValue);
        if (R_FAILED(res)) {
            return res;  // Error during I2C read
        }
        
        val = (u8)rawValue;  // Cast the value to an 8-bit unsigned integer
        fractionalPart = static_cast<float>(val >> 4) * 0.0625f;  // Convert upper 4 bits into fractional part
        
        // Combine integer and fractional parts
        *temperature = static_cast<float>(integerPart) + fractionalPart;
        
        return 0;
    }
    
    // Function to get the SOC temperature
    Result ReadSocTemperature(float *temperature, bool integerOnly) {
        return ReadTemperature(temperature, TMP451_SOC_TEMP_REG, TMP451_SOC_TMP_DEC_REG, integerOnly);
    }
    
    // Function to get the PCB temperature
    Result ReadPcbTemperature(float *temperature, bool integerOnly) {
        return ReadTemperature(temperature, TMP451_PCB_TEMP_REG, TMP451_PCB_TMP_DEC_REG, integerOnly);
    }
    
    
    // Time implementation
    CONSTEXPR_STRING std::string DEFAULT_DT_FORMAT = "%T %a";
    std::string datetimeFormat = DEFAULT_DT_FORMAT;
    
    
    // Widget settings
    //std::string hideClock, hideBattery, hidePCBTemp, hideSOCTemp;
    bool hideClock, hideBattery, hidePCBTemp, hideSOCTemp, dynamicWidgetColors;
    bool hideWidgetBackdrop, centerWidgetAlignment, extendedWidgetBackdrop;

    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeWidgetVars() {
        // Load INI data once instead of 8 separate file reads
        auto ultrahandSection = getKeyValuePairsFromSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME);
        
        // Helper lambda to safely get boolean values with proper defaults
        auto getBoolValue = [&](const std::string& key, bool defaultValue = false) -> bool {
            if (ultrahandSection.count(key) > 0) {
                return (ultrahandSection.at(key) != FALSE_STR);
            }
            return defaultValue;
        };
        
        // Set all values from the loaded section with correct defaults (matching initialization)
        hideClock = getBoolValue("hide_clock", false);                           // FALSE_STR default
        hideBattery = getBoolValue("hide_battery", true);                        // TRUE_STR default
        hideSOCTemp = getBoolValue("hide_soc_temp", true);                       // TRUE_STR default  
        hidePCBTemp = getBoolValue("hide_pcb_temp", true);                       // TRUE_STR default
        dynamicWidgetColors = getBoolValue("dynamic_widget_colors", true);       // TRUE_STR default
        hideWidgetBackdrop = getBoolValue("hide_widget_backdrop", false);        // FALSE_STR default
        centerWidgetAlignment = getBoolValue("center_widget_alignment", true);   // TRUE_STR default
        extendedWidgetBackdrop = getBoolValue("extended_widget_backdrop", false); // FALSE_STR default
    }
    #endif
    
    bool cleanVersionLabels, hideOverlayVersions, hidePackageVersions, useLibultrahandTitles, useLibultrahandVersions, usePackageTitles, usePackageVersions;
    


    
    // Helper function to convert MB to bytes
    u64 mbToBytes(u32 mb) {
        return static_cast<u64>(mb) * 0x100000;
    }
    
    // Helper function to convert bytes to MB
    u32 bytesToMB(u64 bytes) {
        return static_cast<u32>(bytes / 0x100000);
    }
    

    // Helper function to get version-appropriate default heap size
    static OverlayHeapSize getDefaultHeapSize() {
        if (hosversionAtLeast(21, 0, 0)) {
            return OverlayHeapSize::Size_4MB;  // HOS 21.0.0+
        } else if (hosversionAtLeast(20, 0, 0)) {
            return OverlayHeapSize::Size_6MB;  // HOS 20.0.0+
        } else {
            return OverlayHeapSize::Size_8MB;  // Older versions
        }
    }
    
    // Implementation
    OverlayHeapSize getCurrentHeapSize() {
        // Fast path: return cached value if already loaded
        if (heapSizeCache.initialized) {
            return heapSizeCache.cachedSize;
        }
        
        // Slow path: read from file (only happens once)
        FILE* f = fopen(ult::OVL_HEAP_CONFIG_PATH.c_str(), "rb");
        if (!f) {
            // No config file - use version-specific default
            heapSizeCache.cachedSize = getDefaultHeapSize();
            heapSizeCache.initialized = true;
            return heapSizeCache.cachedSize;
        }
        
        u64 size;
        if (fread(&size, sizeof(size), 1, f) == 1) {
            constexpr u64 twoMB = 0x200000;
            // Only accept multiples of 2MB, excluding 2MB itself
            if (size != twoMB && size % twoMB == 0) {
                heapSizeCache.cachedSize = static_cast<OverlayHeapSize>(size);
                fclose(f);
                heapSizeCache.initialized = true;
                return heapSizeCache.cachedSize;
            }
        }
        
        // Invalid or no data in config - use version-specific default
        fclose(f);
        heapSizeCache.cachedSize = getDefaultHeapSize();
        heapSizeCache.initialized = true;
        return heapSizeCache.cachedSize;
    }
    
    // Update the global default too
    OverlayHeapSize currentHeapSize = getDefaultHeapSize();
    
    bool setOverlayHeapSize(OverlayHeapSize heapSize) {
        ult::createDirectory(ult::NX_OVLLOADER_PATH);
        
        FILE* f = fopen(ult::OVL_HEAP_CONFIG_PATH.c_str(), "wb");
        if (!f) return false;
        
        const u64 size = static_cast<u64>(heapSize);
        const bool success = (fwrite(&size, sizeof(size), 1, f) == 1);
        fclose(f);
        
        // Update cache on successful write
        if (success) {
            heapSizeCache.cachedSize = heapSize;
            heapSizeCache.initialized = true;

            // Create reloading flag to indicate this was an intentional restart
            ult::createDirectory(ult::FLAGS_PATH);
            f = fopen(ult::RELOADING_FLAG_FILEPATH.c_str(), "wb");
            if (f) {
                fclose(f);  // Empty file, just needs to exist
            }
        }
        
        return success;
    }
    
    
    // Implementation
    bool requestOverlayExit() {
        ult::createDirectory(ult::NX_OVLLOADER_PATH);
        
        FILE* f = fopen(ult::OVL_EXIT_FLAG_PATH.c_str(), "wb");
        if (!f) return false;
        
        // Write a single byte (flag file just needs to exist)
        u8 flag = 1;
        bool success = (fwrite(&flag, 1, 1, f) == 1);
        fclose(f);
        
        deleteFileOrDirectory(NOTIFICATIONS_FLAG_FILEPATH);
        
        return success;
    }


    const std::string loaderInfo = envGetLoaderInfo();
    std::string loaderTitle = extractTitle(loaderInfo);

    bool expandedMemory = false;
    bool furtherExpandedMemory = false;
    bool limitedMemory = false;
    
    std::string versionLabel;
    
    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeVersionLabels() {
        // Load INI data once instead of 6 separate file reads
        auto ultrahandSection = getKeyValuePairsFromSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME);
        
        // Helper lambda to safely get boolean values with proper defaults
        auto getBoolValue = [&](const std::string& key, bool defaultValue = false) -> bool {
            if (ultrahandSection.count(key) > 0) {
                return (ultrahandSection.at(key) != FALSE_STR);
            }
            return defaultValue;
        };
        
        // Set all values from the loaded section with correct defaults (matching initialization)
        cleanVersionLabels = getBoolValue("clean_version_labels", false);        // FALSE_STR default
        hideOverlayVersions = getBoolValue("hide_overlay_versions", false);      // FALSE_STR default  
        hidePackageVersions = getBoolValue("hide_package_versions", false);      // FALSE_STR default
        //libultrahandTitles = getBoolValue("libultrahand_titles", false);               // FALSE_STR default
        //useLibultrahandVersions = getBoolValue("libultrahand_versions", true);            // TRUE_STR default
        //matchPackages = getBoolValue("match_packages", true);            // TRUE_STR default
        
        //#ifdef APP_VERSION
        //versionLabel = cleanVersionLabel(APP_VERSION) + "  (" + loaderTitle + " " + cleanVersionLabel(loaderInfo) + ")";
        //#endif
        //versionLabel = (cleanVersionLabels) ? std::string(APP_VERSION) : (std::string(APP_VERSION) + "   (" + extractTitle(loaderInfo) + " v" + cleanVersionLabel(loaderInfo) + ")");
    }
    #endif
    
    
    // Number of renderer threads to use
    const unsigned numThreads = 4;//expandedMemory ? 4 : 0;
    std::vector<std::thread> renderThreads(numThreads);

    const s32 bmpChunkSize = ((720 + numThreads - 1) / numThreads);
    std::atomic<s32> currentRow;
    
    //std::atomic<unsigned int> barrierCounter{0};
    //std::mutex barrierMutex;
    //std::condition_variable barrierCV;
    //
    //void barrierWait() {
    //    std::unique_lock<std::mutex> lock(barrierMutex);
    //    if (++barrierCounter == numThreads) {
    //        barrierCounter = 0; // Reset for the next round
    //        barrierCV.notify_all();
    //    } else {
    //        barrierCV.wait(lock, [] { return barrierCounter == 0; });
    //    }
    //}
}
