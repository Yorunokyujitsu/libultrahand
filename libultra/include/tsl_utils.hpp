/********************************************************************************
 * File: tsl_utils.hpp
 * Author: ppkantorski
 * Description: 
 *   'tsl_utils.hpp' is a central utility header for the Ultrahand Overlay project,
 *   containing a variety of functions and definitions related to system status,
 *   input handling, and application-specific behavior on the Nintendo Switch.
 *   This header provides essential utilities for interacting with the system,
 *   managing key input, and enhancing overlay functionality.
 *
 *   The utilities defined here are designed to operate independently, facilitating
 *   robust system interaction capabilities required for custom overlays.
 *
 *   For the latest updates and contributions, visit the project's GitHub repository:
 *   GitHub Repository: https://github.com/ppkantorski/Ultrahand-Overlay
 *
 *   Note: This notice is integral to the project's documentation and must not be 
 *   altered or removed.
 *
 *  Licensed under both GPLv2 and CC-BY-4.0
 *  Copyright (c) 2024 ppkantorski
 ********************************************************************************/


#pragma once
#ifndef TSL_UTILS_HPP
#define TSL_UTILS_HPP

#if !USING_FSTREAM_DIRECTIVE // For not using fstream (needs implementing)
#include <stdio.h>
#else
#include <fstream>
#endif

#include <ultra.hpp>
#include <switch.h>
#include <arm_neon.h>

#include <stdlib.h>
#include <strings.h>
//#include <math.h>

#include <algorithm>
#include <cstring>
#include <cwctype>
#include <string>
#include <functional>
#include <type_traits>
#include <mutex>
#include <memory>
//#include <chrono>
#include <list>
#include <stack>
#include <map>
#include <barrier>

#ifndef APPROXIMATE_cos
// Approximation for cos(x) using Taylor series around 0
#define APPROXIMATE_cos(x)       (1 - (x) * (x) / 2 + (x) * (x) * (x) * (x) / 24)  // valid for small x
#endif


#ifndef APPROXIMATE_ifloor
#define APPROXIMATE_ifloor(x)   ((int)((x) >= 0 ? (x) : (x) - 1))  // truncate toward negative infinity
#define APPROXIMATE_iceil(x)    ((int)((x) == (int)(x) ? (x) : ((x) > 0 ? (int)(x) + 1 : (int)(x))))  // truncate toward positive infinity
#endif

#ifndef APPROXIMATE_sqrt
// Fast approximation for sqrt using Newton's method
#define APPROXIMATE_sqrt(x)     ((x) <= 0 ? 0 : (x) / 2.0 * (3.0 - ((x) * (x) * 0.5)))  // Approximation for x close to 1
#define APPROXIMATE_pow(x, y)   ((y) == 0 ? 1 : ((y) == 1 ? (x) : APPROXIMATE_sqrt(x))) // limited to approximate sqrt if y=0.5
#endif

#ifndef APPROXIMATE_fmod
#define APPROXIMATE_fmod(x, y)    ((x) - ((int)((x) / (y)) * (y)))  // equivalent to x - floor(x/y) * y
#endif

#ifndef APPROXIMATE_cos
// Approximation for cos(x) using Taylor series around 0
#define APPROXIMATE_cos(x)       (1 - (x) * (x) / 2 + (x) * (x) * (x) * (x) / 24)  // valid for small x
#endif

#ifndef APPROXIMATE_acos
#define APPROXIMATE_acos(x)      (1.5708 - (x) - (x)*(x)*(x) / 6)  // limited approximation for acos in range [-1, 1]
#endif

#ifndef APPROXIMATE_fabs
#define APPROXIMATE_fabs(x)      ((x) < 0 ? -(x) : (x))
#endif

struct OverlayCombo {
    std::string path;   // full overlay path
    std::string launchArg; // empty = use per-overlay launch_args key, otherwise a “mode” arg
};

struct SwapDepth {
    u32 value;
    explicit SwapDepth(u32 v) : value(v) {}
};

namespace ult {
    extern bool correctFrameSize; // for detecting the correct Overlay display size

    extern u16 DefaultFramebufferWidth;            ///< Width of the framebuffer
    extern u16 DefaultFramebufferHeight;           ///< Height of the framebuffer

    extern std::unordered_map<std::string, std::string> translationCache;

    extern std::unordered_map<u64, OverlayCombo> g_entryCombos;
    extern std::atomic<bool> launchingOverlay;
    extern std::atomic<bool> settingsInitialized;
    extern std::atomic<bool> currentForeground;
    //extern std::mutex simulatedNextPageMutex;

    //void loadOverlayKeyCombos();
    //std::string getOverlayForKeyCombo(u64 keys);

    bool readFileContent(const std::string& filePath, std::string& content);
    void parseJsonContent(const std::string& content, std::unordered_map<std::string, std::string>& result);
    bool parseJsonToMap(const std::string& filePath, std::unordered_map<std::string, std::string>& result);

    bool loadTranslationsFromJSON(const std::string& filePath);

    extern u16 activeHeaderHeight;

    bool consoleIsDocked();
    
    std::string getTitleIdAsString();
    
    extern std::string lastTitleID;
    extern std::atomic<bool> resetForegroundCheck;


    //extern bool isLauncher;
    extern std::atomic<bool> internalTouchReleased;
    extern u32 layerEdge;
    extern bool useRightAlignment;
    extern bool useSwipeToOpen;
    extern bool useLaunchCombos;
    extern bool useNotifications;
    extern bool usePageSwap;
    extern std::atomic<bool> noClickableItems;

    extern bool useDynamicLogo;
    extern bool useSelectionBG;
    extern bool useSelectionText;
    extern bool useSelectionValue;


    
    // Define the duration boundaries (for smooth scrolling)
    //extern const std::chrono::milliseconds initialInterval;  // Example initial interval
    //extern const std::chrono::milliseconds shortInterval;    // Short interval after long hold
    //extern const std::chrono::milliseconds transitionPoint; // Point at which the shortest interval is reached
    


    //constexpr std::chrono::milliseconds initialInterval = std::chrono::milliseconds(67);  // Example initial interval
    //constexpr std::chrono::milliseconds shortInterval = std::chrono::milliseconds(10);    // Short interval after long hold
    //constexpr std::chrono::milliseconds transitionPoint = std::chrono::milliseconds(2000); // Point at which the shortest interval is reached

    // Function to interpolate between two durations
    //std::chrono::milliseconds interpolateDuration(std::chrono::milliseconds start, std::chrono::milliseconds end, float t);
    
#if IS_LAUNCHER_DIRECTIVE
    extern std::atomic<bool> overlayLaunchRequested;
    extern std::string requestedOverlayPath;
    extern std::string requestedOverlayArgs;
    extern std::mutex overlayLaunchMutex;
#endif
    
    
    //#include <filesystem> // Comment out filesystem
    
    // CUSTOM SECTION START
    //extern float backWidth, selectWidth, nextPageWidth;
    extern std::atomic<float> backWidth;
    extern std::atomic<float> selectWidth;
    extern std::atomic<float> nextPageWidth;
    extern std::atomic<bool> inMainMenu;
    extern std::atomic<bool> inOverlaysPage;
    extern std::atomic<bool> inPackagesPage;
    
    extern bool firstBoot; // for detecting first boot
    
    //static std::unordered_map<std::string, std::string> hexSumCache;
    
    // Define an atomic bool for interpreter completion
    extern std::atomic<bool> threadFailure;
    extern std::atomic<bool> runningInterpreter;
    extern std::atomic<bool> shakingProgress;
    
    extern std::atomic<bool> isHidden;
    extern std::atomic<bool> externalAbortCommands;
    
    //bool progressAnimation = false;
    extern bool disableTransparency;
    //bool useCustomWallpaper = false;
    extern bool useMemoryExpansion;
    extern bool useOpaqueScreenshots;
    
    extern std::atomic<bool> onTrackBar;
    extern std::atomic<bool> allowSlide;
    extern std::atomic<bool> unlockedSlide;
    
    void atomicToggle(std::atomic<bool>& b);

    /**
     * @brief Shutdown modes for the Ultrahand-Overlay project.
     *
     * These macros define the shutdown modes used in the Ultrahand-Overlay project:
     * - `SpsmShutdownMode_Normal`: Normal shutdown mode.
     * - `SpsmShutdownMode_Reboot`: Reboot mode.
     */
    #define SpsmShutdownMode_Normal 0
    #define SpsmShutdownMode_Reboot 1
    
    /**
     * @brief Key mapping macros for button keys.
     *
     * These macros define button keys for the Ultrahand-Overlay project to simplify key mappings.
     * For example, `KEY_A` represents the `HidNpadButton_A` key.
     */
    #define KEY_A HidNpadButton_A
    #define KEY_B HidNpadButton_B
    #define KEY_X HidNpadButton_X
    #define KEY_Y HidNpadButton_Y
    #define KEY_L HidNpadButton_L
    #define KEY_R HidNpadButton_R
    #define KEY_ZL HidNpadButton_ZL
    #define KEY_ZR HidNpadButton_ZR
    #define KEY_PLUS HidNpadButton_Plus
    #define KEY_MINUS HidNpadButton_Minus
    #define KEY_DUP HidNpadButton_Up
    #define KEY_DDOWN HidNpadButton_Down
    #define KEY_DLEFT HidNpadButton_Left
    #define KEY_DRIGHT HidNpadButton_Right
    #define KEY_SL HidNpadButton_AnySL
    #define KEY_SR HidNpadButton_AnySR
    #define KEY_LSTICK HidNpadButton_StickL
    #define KEY_RSTICK HidNpadButton_StickR
    #define KEY_UP HidNpadButton_AnyUp
    #define KEY_DOWN HidNpadButton_AnyDown
    #define KEY_LEFT HidNpadButton_AnyLeft
    #define KEY_RIGHT HidNpadButton_AnyRight
    
    #define SCRIPT_KEY HidNpadButton_Minus
    #define SYSTEM_SETTINGS_KEY HidNpadButton_Plus
    #define SETTINGS_KEY HidNpadButton_Y
    #define STAR_KEY HidNpadButton_X

    
    // Define a mask with all possible key flags
    #define ALL_KEYS_MASK (KEY_A | KEY_B | KEY_X | KEY_Y | KEY_DUP | KEY_DDOWN | KEY_DLEFT | KEY_DRIGHT | KEY_L | KEY_R | KEY_ZL | KEY_ZR | KEY_SL | KEY_SR | KEY_LSTICK | KEY_RSTICK | KEY_PLUS | KEY_MINUS)
    
    
    extern bool updateMenuCombos;
    
    /**
     * @brief Ultrahand-Overlay Input Macros
     *
     * This block of code defines macros for handling input in the Ultrahand-Overlay project.
     * These macros simplify the mapping of input events to corresponding button keys and
     * provide aliases for touch and joystick positions.
     *
     * The macros included in this block are:
     *
     * - `touchPosition`: An alias for a constant `HidTouchState` pointer.
     * - `touchInput`: An alias for `&touchPos`, representing touch input.
     * - `JoystickPosition`: An alias for `HidAnalogStickState`, representing joystick input.
     *
     * These macros are utilized within the Ultrahand-Overlay project to manage and interpret
     * user input, including touch and joystick events.
     */
    #define touchPosition const HidTouchState
    #define touchInput &touchPos
    #define JoystickPosition HidAnalogStickState
    
    //void convertComboToUnicode(std::string& combo);

    /**
     * @brief Combo key mapping
     */
    struct KeyInfo {
        u64 key;
        const char* name;
        const char* glyph;
    };

    /**
     * @brief Combo key mappings
     *
     * Ordered as they should be displayed
     */
    extern std::array<KeyInfo, 18> KEYS_INFO;

    std::unordered_map<std::string, std::string> createButtonCharMap();
    
    extern std::unordered_map<std::string, std::string> buttonCharMap;
    
    
    void convertComboToUnicode(std::string& combo);
    
    
    // English string definitions
    
    extern const std::string whiteColor;
    extern const std::string blackColor;

    extern std::atomic<bool> languageWasChanged;
    
    inline constexpr double _M_PI = 3.14159265358979323846;  // For double precision
    inline constexpr double RAD_TO_DEG = 180.0f / _M_PI;
    
    #if IS_LAUNCHER_DIRECTIVE
    extern std::string ENGLISH;
    extern std::string SPANISH;
    extern std::string FRENCH;
    extern std::string GERMAN;
    extern std::string JAPANESE;
    extern std::string KOREAN;
    extern std::string ITALIAN;
    extern std::string DUTCH;
    extern std::string PORTUGUESE;
    extern std::string RUSSIAN;
    extern std::string UKRAINIAN;
    extern std::string POLISH;
    extern std::string SIMPLIFIED_CHINESE;
    extern std::string TRADITIONAL_CHINESE;

    extern std::string OVERLAYS; //defined in libTesla now
    extern std::string OVERLAYS_ABBR;
    extern std::string OVERLAY;
    extern std::string HIDDEN_OVERLAYS;
    extern std::string PACKAGES; //defined in libTesla now
    extern std::string PACKAGE;
    extern std::string HIDDEN_PACKAGES;
    extern std::string HIDDEN;
    extern std::string HIDE_OVERLAY;
    extern std::string HIDE_PACKAGE;
    extern std::string LAUNCH_ARGUMENTS;
    extern std::string QUICK_LAUNCH;
    extern std::string BOOT_COMMANDS;
    extern std::string EXIT_COMMANDS;
    extern std::string ERROR_LOGGING;
    extern std::string COMMANDS;
    extern std::string SETTINGS;
    extern std::string FAVORITE;
    extern std::string MAIN_SETTINGS;
    extern std::string UI_SETTINGS;

    extern std::string WIDGET;
    extern std::string WIDGET_ITEMS;
    extern std::string WIDGET_SETTINGS;
    extern std::string CLOCK;
    extern std::string BATTERY;
    extern std::string SOC_TEMPERATURE;
    extern std::string PCB_TEMPERATURE;
    extern std::string BACKDROP;
    extern std::string DYNAMIC_COLORS;
    extern std::string CENTER_ALIGNMENT;
    extern std::string EXTENDED_BACKDROP;
    extern std::string MISCELLANEOUS;
    //extern std::string MENU_ITEMS;
    extern std::string MENU_SETTINGS;
    extern std::string USER_GUIDE;
    extern std::string SHOW_HIDDEN;
    extern std::string SHOW_DELETE;
    extern std::string PAGE_SWAP;
    extern std::string RIGHT_SIDE_MODE;
    extern std::string OVERLAY_VERSIONS;
    extern std::string PACKAGE_VERSIONS;
    extern std::string CLEAN_VERSIONS;
    //extern std::string VERSION_LABELS;
    extern std::string KEY_COMBO;
    extern std::string MODE;
    extern std::string MODES;
    extern std::string LANGUAGE;
    extern std::string OVERLAY_INFO;
    extern std::string SOFTWARE_UPDATE;
    extern std::string UPDATE_ULTRAHAND;
    extern std::string UPDATE_LANGUAGES;
    extern std::string SYSTEM;
    extern std::string DEVICE_INFO;
    extern std::string FIRMWARE;
    extern std::string BOOTLOADER;
    extern std::string HARDWARE;
    extern std::string MEMORY;
    extern std::string VENDOR;
    extern std::string MODEL;
    extern std::string STORAGE;
    extern std::string NOTICE;
    extern std::string UTILIZES;

    extern std::string MEMORY_EXPANSION;
    extern std::string REBOOT_REQUIRED;
    extern std::string LOCAL_IP;
    extern std::string WALLPAPER;
    extern std::string THEME;
    extern std::string DEFAULT;
    extern std::string ROOT_PACKAGE;
    extern std::string SORT_PRIORITY;
    extern std::string OPTIONS;
    extern std::string FAILED_TO_OPEN;

    extern std::string LAUNCH_COMBOS;
    extern std::string NOTIFICATIONS;
    extern std::string OPAQUE_SCREENSHOTS;

    extern std::string PACKAGE_INFO;
    extern std::string _TITLE;
    extern std::string _VERSION;
    extern std::string _CREATOR;
    extern std::string _ABOUT;
    extern std::string _CREDITS;

    extern std::string USERGUIDE_OFFSET;
    extern std::string SETTINGS_MENU;
    extern std::string SCRIPT_OVERLAY;
    extern std::string STAR_FAVORITE;
    extern std::string APP_SETTINGS;
    extern std::string ON_MAIN_MENU;
    extern std::string ON_A_COMMAND;
    extern std::string ON_OVERLAY_PACKAGE;
    extern std::string FEATURES;
    extern std::string SWIPE_TO_OPEN;

    extern std::string THEME_SETTINGS;
    extern std::string DYNAMIC_LOGO;
    extern std::string SELECTION_BACKGROUND;
    extern std::string SELECTION_TEXT;
    extern std::string SELECTION_VALUE;
    extern std::string LIBULTRAHAND_TITLES;
    extern std::string LIBULTRAHAND_VERSIONS;
    extern std::string PACKAGE_TITLES;

    extern std::string ULTRAHAND_HAS_STARTED;
    extern std::string NEW_UPDATE_IS_AVAILABLE;
    extern std::string REBOOT_IS_REQUIRED;
    extern std::string HOLD_A_TO_DELETE;
    extern std::string SELECTION_IS_EMPTY;

    //extern std::string PACKAGE_VERSIONS;
    //extern std::string PROGRESS_ANIMATION;
    /* ASAP Packages */
    // Sections
    extern std::string REBOOT_TO;
    extern std::string CFWBOOT;
    extern std::string OFWBOOT;
    extern std::string ANDROID;
    extern std::string LAKKA;
    extern std::string UBUNTU;
    extern std::string REBOOT;
    extern std::string SHUTDOWN;
    // Informations
    extern std::string CFWBOOT_TITLE;
    extern std::string HEKATE_TITLE;
    extern std::string L4T_TITLE;
    extern std::string VOLUME_INFO;
    extern std::string LAUNCHER_INFO;
    extern std::string CFWBOOT_INFO;
    extern std::string HEKATE_INFO;
    extern std::string BOOT_ENTRY;
    // Configs
    extern std::string MAINMENU_INFO;
    extern std::string INFO;
    extern std::string ABOUT_INFO;
    extern std::string OLD_DEVICE;
    extern std::string NEW_DEVICE;
    extern std::string DF_THEME;
    extern std::string UPDATE_ULTRA_INFO;
    extern std::string UPDATE_TEST_WARN;
    extern std::string HIGHLIGHT_LINE;
    extern std::string UPDATE_TEST_INFO;
    extern std::string UPDATE_TEST_NOTICE;
    // ETC.
    extern std::string OVERRIDE_SELECTION;
    extern std::string AUTO_SELECTION;
    // Package-System Clock+
    extern std::string ENABLED;
    extern std::string SCPLUS_INFO;
    extern std::string SC_STATUS;
    extern std::string LDR_TOOL;
    extern std::string LDR_INFO;
    extern std::string TOOL_WARNING_1;
    extern std::string TOOL_WARNING_2;
    extern std::string CPU_VOLTAGE;
    extern std::string CPU_INFO;
    extern std::string CPU_CHART;
    extern std::string USED;
    // Package-Extra Setting+
    extern std::string EXTRA_SETTING;
    extern std::string CFW_CFG;
    extern std::string CFW_CFG_INFO;
    extern std::string DEFAULT_NAND;
    extern std::string DEFAULT_NAND_INFO;
    extern std::string NO_EMUNAND;
    extern std::string ADD_ENTRY;
    extern std::string ADD_ENTRY_INFO;
    extern std::string MOON_LAUNCHER;
    extern std::string MOON_ENTRY;
    extern std::string MOON_LAUNCHER_INFO;
    extern std::string MOON_LAUNCHER_WARN_1;
    extern std::string MOON_LAUNCHER_WARN_2;
    extern std::string MOON_CFW;
    extern std::string MOON_STOCK;
    extern std::string MOON_ANDROID;
    extern std::string MOON_LAKKA;
    extern std::string MOON_UBUNTU;
    extern std::string SYSTEM_CFG;
    extern std::string SYSTEM_CFG_INFO;
    extern std::string HEKATE_BOOT;
    extern std::string HEKATE_BOOT_INFO;
    extern std::string HEKATE_BACKLIGHT_INFO;
    extern std::string HEKATE_BACKLIGHT;
    extern std::string AUTO_HOS_OFF_SEC;
    extern std::string AUTO_HOS_OFF_VAL;
    extern std::string HEKATE_AUTO_HOS_OFF;
    extern std::string BOOT_WAIT_SEC;
    extern std::string BOOT_WAIT_VAL;
    extern std::string HEKATE_BOOT_WAIT;
    extern std::string BOOT_WAIT_SECOND;
    extern std::string AUTOBOOT_CFW_SEC;
    extern std::string AUTOBOOT_CFW_VAL;
    extern std::string HEKATE_AUTOBOOT;
    extern std::string SYSTEM_PAGE;
    extern std::string DMNT_CHEAT;
    extern std::string DMNT_TOGGLE;
    extern std::string NO_GC_INFO;
    extern std::string NO_GC_WARN;
    extern std::string NO_GC;
    extern std::string REC_INFO;
    extern std::string REC_WARN;
    extern std::string SYSMMC_ONLY;
    extern std::string ENABLED_EMUMMC;
    extern std::string EXOSPHERE_INFO;
    extern std::string EXOSPHERE;
    extern std::string DNS_INFO;
    extern std::string DNS_MITM;
    extern std::string APPLY_RESET_SEC;
    extern std::string APPLY_RESET_VAL;
    extern std::string APPLY_CFG;
    extern std::string RESET_CFG;
    extern std::string HB_MENU;
    extern std::string HB_MENU_INFO;
    extern std::string FULL_MEMORY;
    extern std::string FULL_MEMORY_INFO;
    extern std::string FULL_MEMORY_REC;
    extern std::string FULL_MEMORY_FORWARDER_VAL;
    extern std::string FULL_MEMORY_FORWARDER_CTN;
    extern std::string FULL_MEMORY_KEY;
    extern std::string APPLET_MEMORY;
    extern std::string APPLET_MEMORY_INFO;
    extern std::string APPLET_MEMORY_VAL;
    extern std::string APPLET_MEMORY_KEY;
    extern std::string APPLET_HB_MENU_ICON;
    extern std::string APPLET_HB_MENU_KEY;
    extern std::string APPLET_ALBUM;
    extern std::string APPLET_USER;
    extern std::string APPLET_KEY_HOLD;
    extern std::string APPLET_KEY_CLICK;
    extern std::string SD_CLEANUP;
    extern std::string SD_CLEANUP_INFO;
    extern std::string NORMAL_DEVICE;
    extern std::string PATCH_DEVICE;
    extern std::string RAM_PATCH_WARN;
    extern std::string RAM_PATCH;
    // Package-Quick Guide+
    extern std::string QUICK_GUIDE_INFO;
    extern std::string QUICK_GUIDE;
    extern std::string KEYMAP_INFO;
    extern std::string KEY_MOVE;
    extern std::string KEY_MENU;
    extern std::string KEY_SELECT;
    extern std::string KEYGAP_1;
    extern std::string KEYGAP_2;
    extern std::string KEYGAP_3;
    extern std::string PACK_INFO;
    extern std::string USEFUL;
    extern std::string HIDE_SPHAIRA;
    extern std::string FORWARDER_SPHAIRA;
    extern std::string APP_INSTALL;
    extern std::string PC_INSTALL;
    extern std::string FTP_INSTALL;
    extern std::string MTP_INSTALL;
    extern std::string USB_INSTALL;
    extern std::string HDD_INSTALL_1;
    extern std::string HDD_INSTALL_2;
    extern std::string ERROR_INFO;
    extern std::string ALBUM_INFO;
    extern std::string ALBUM_ERROR_1;
    extern std::string ALBUM_ERROR_2;
    extern std::string CPUDEAD_INFO;
    extern std::string CPU_ERROR;
    extern std::string SYSTEM_ERROR;
    extern std::string MODULE_ERROR;
    extern std::string MODULE_INFO_1;
    extern std::string MODULE_INFO_2;
    extern std::string MODULE_INFO_3;
    extern std::string MODULE_INFO_4;
    extern std::string SYSNAND_INFO;
    extern std::string EMUNAND_INFO;
    extern std::string NORMAL_ERROR;
    extern std::string APP_MODULE_ERROR;
    extern std::string SWITCH_MODULE_ERROR;
    extern std::string PREV_PAGE;
    extern std::string NEXT_PAGE;
    extern std::string VER_ERR;
    extern std::string SD_ERR;
    extern std::string STORAGE_ERR;
    extern std::string MISC_ERR;
    extern std::string NSS_ERR;
    extern std::string SERVER_ERR;
    extern std::string NETWORK_ERR;
    extern std::string FORWARDER_ERR;
    extern std::string FORWARDER_INFO;
    extern std::string APP_ERR;
    extern std::string APP_MODULE;
    extern std::string SCROLL;
    extern std::string SWITCH_MODULE;
    // Overlays-Custom infomation
    extern std::string OVLSYSMODULE;
    extern std::string EDIZON;
    extern std::string EMUIIBO;
    extern std::string FIZEAU;
    extern std::string NXFANCONTROL;
    extern std::string FPSLOCKER;
    extern std::string LDNMITM;
    extern std::string QUICKNTP;
    extern std::string REVERSENX;
    extern std::string STATUSMONITOR;
    extern std::string STUDIOUSPANCAKE;
    extern std::string SYSCLK;
    extern std::string SYSDVR;
    extern std::string SYSPATCH;
    #endif

    extern std::string FREE;

    extern std::string DEFAULT_CHAR_WIDTH;
    extern std::string UNAVAILABLE_SELECTION;

    extern std::string ON;
    extern std::string OFF;

    extern std::string OK;
    extern std::string BACK;
    extern std::string HIDE;
    extern std::string CANCEL;

    extern std::string GAP_1;
    extern std::string GAP_2;
    extern std::atomic<float> halfGap;

    //extern std::string EMPTY;

    #if USING_WIDGET_DIRECTIVE
    extern std::string SUNDAY;
    extern std::string MONDAY;
    extern std::string TUESDAY;
    extern std::string WEDNESDAY;
    extern std::string THURSDAY;
    extern std::string FRIDAY;
    extern std::string SATURDAY;
    
    extern std::string JANUARY;
    extern std::string FEBRUARY;
    extern std::string MARCH;
    extern std::string APRIL;
    extern std::string MAY;
    extern std::string JUNE;
    extern std::string JULY;
    extern std::string AUGUST;
    extern std::string SEPTEMBER;
    extern std::string OCTOBER;
    extern std::string NOVEMBER;
    extern std::string DECEMBER;
    
    extern std::string SUN;
    extern std::string MON;
    extern std::string TUE;
    extern std::string WED;
    extern std::string THU;
    extern std::string FRI;
    extern std::string SAT;
    
    extern std::string JAN;
    extern std::string FEB;
    extern std::string MAR;
    extern std::string APR;
    extern std::string MAY_ABBR;
    extern std::string JUN;
    extern std::string JUL;
    extern std::string AUG;
    extern std::string SEP;
    extern std::string OCT;
    extern std::string NOV;
    extern std::string DEC;
    #endif
    
    #if IS_LAUNCHER_DIRECTIVE
    // Constant string definitions (English)
    void reinitializeLangVars();
    #endif
    
    
    // Define the updateIfNotEmpty function
    void updateIfNotEmpty(std::string& constant, const char* jsonKey, const json_t* jsonData);
    
    void parseLanguage(const std::string& langFile);
    
    #if USING_WIDGET_DIRECTIVE
    void localizeTimeStr(char* timeStr);
    #endif

    // Unified function to apply replacements
    void applyLangReplacements(std::string& text, bool isValue = false);
    
    
    
    //// Map of character widths (pre-calibrated)
    //extern std::unordered_map<wchar_t, float> characterWidths;
    
    //extern float defaultNumericCharWidth;
    
    
    
    // Predefined hexMap
    //extern const std::array<int, 256> hexMap;
    inline constexpr std::array<int, 256> hexMap = [] {
        std::array<int, 256> map = {0};
        map['0'] = 0; map['1'] = 1; map['2'] = 2; map['3'] = 3; map['4'] = 4;
        map['5'] = 5; map['6'] = 6; map['7'] = 7; map['8'] = 8; map['9'] = 9;
        map['A'] = 10; map['B'] = 11; map['C'] = 12; map['D'] = 13; map['E'] = 14; map['F'] = 15;
        map['a'] = 10; map['b'] = 11; map['c'] = 12; map['d'] = 13; map['e'] = 14; map['f'] = 15;
        return map;
    }();
    
    
    // Prepare a map of default settings
    extern std::map<const std::string, std::string> defaultThemeSettingsMap;
    
    bool isNumericCharacter(char c);
    
    bool isValidHexColor(const std::string& hexColor);
    
    
    
    float calculateAmplitude(float x, float peakDurationFactor = 0.25f);
            
    
    
    extern std::atomic<bool> refreshWallpaper;
    extern std::vector<u8> wallpaperData;
    extern std::atomic<bool> inPlot;
    
    extern std::mutex wallpaperMutex;
    extern std::condition_variable cv;
    
    
    
    // Function to load the RGBA file into memory and modify wallpaperData directly
    void loadWallpaperFile(const std::string& filePath, s32 width = 448, s32 height = 720);
    void loadWallpaperFileWhenSafe();

    void reloadWallpaper();
    
    // Global variables for FPS calculation
    //extern double lastTimeCount;
    //extern int frameCount;
    //extern float fps;
    //extern double elapsedTime;
    
    
    extern std::atomic<bool> themeIsInitialized;

    // Variables for touch commands
    extern std::atomic<bool> touchingBack;
    extern std::atomic<bool> touchingSelect;
    extern std::atomic<bool> touchingNextPage;
    extern std::atomic<bool> touchingMenu;
    extern std::atomic<bool> shortTouchAndRelease;
    extern std::atomic<bool> longTouchAndRelease;
    extern std::atomic<bool> simulatedBack;
    //extern bool simulatedBackComplete;
    extern std::atomic<bool> simulatedSelect;
    //extern bool simulatedSelectComplete;
    extern std::atomic<bool> simulatedNextPage;
    //extern std::atomic<bool> simulatedNextPageComplete;
    extern std::atomic<bool> simulatedMenu;
    //extern bool simulatedMenuComplete;
    extern std::atomic<bool> stillTouching;
    extern std::atomic<bool> interruptedTouch;
    extern std::atomic<bool> touchInBounds;
    
    
#if USING_WIDGET_DIRECTIVE
    // Battery implementation
    extern bool powerInitialized;
    extern bool powerCacheInitialized;
    extern uint32_t powerCacheCharge;
    extern bool powerCacheIsCharging;
    extern PsmSession powerSession;
    
    // Define variables to store previous battery charge and time
    //extern uint32_t prevBatteryCharge;
    //extern s64 timeOut;
    
    
    extern std::atomic<uint32_t> batteryCharge;
    extern std::atomic<bool> isCharging;
    
    //constexpr std::chrono::seconds min_delay = std::chrono::seconds(3); // Minimum delay between checks
    
    bool powerGetDetails(uint32_t *_batteryCharge, bool *_isCharging);
    
    void powerInit(void);
    
    void powerExit(void);
#endif
    
    // Temperature Implementation
    extern std::atomic<float> PCB_temperature;
    extern std::atomic<float> SOC_temperature;
    
    /*
    I2cReadRegHandler was taken from Switch-OC-Suite source code made by KazushiMe
    Original repository link (Deleted, last checked 15.04.2023): https://github.com/KazushiMe/Switch-OC-Suite
    */
    
    Result I2cReadRegHandler(u8 reg, I2cDevice dev, u16 *out);
    
    
    #define TMP451_SOC_TEMP_REG 0x01  // Register for SOC temperature integer part
    #define TMP451_SOC_TMP_DEC_REG 0x10  // Register for SOC temperature decimal part
    #define TMP451_PCB_TEMP_REG 0x00  // Register for PCB temperature integer part
    #define TMP451_PCB_TMP_DEC_REG 0x15  // Register for PCB temperature decimal part
    
    // Common helper function to read temperature (integer and fractional parts)
    Result ReadTemperature(float *temperature, u8 integerReg, u8 fractionalReg, bool integerOnly);
    
    // Function to get the SOC temperature
    Result ReadSocTemperature(float *temperature, bool integerOnly = true);
    
    // Function to get the PCB temperature
    Result ReadPcbTemperature(float *temperature, bool integerOnly = true);
    
    
    
    // Time implementation
    
    extern const std::string DEFAULT_DT_FORMAT;
    extern std::string datetimeFormat;
    
    
    // Widget settings
    //static std::string hideClock, hideBattery, hidePCBTemp, hideSOCTemp;
    extern bool hideClock, hideBattery, hidePCBTemp, hideSOCTemp, dynamicWidgetColors;
    extern bool hideWidgetBackdrop, centerWidgetAlignment, extendedWidgetBackdrop;

    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeWidgetVars();
    #endif
    
    extern bool cleanVersionLabels, hideOverlayVersions, hidePackageVersions, useLibultrahandTitles, useLibultrahandVersions, usePackageTitles, usePackageVersions;
    
    extern const std::string loaderInfo;
    extern const std::string loaderTitle;
    extern const bool expandedMemory;
    
    extern std::string versionLabel;
    
    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeVersionLabels();
    #endif
    
    
    // Number of renderer threads to use
    extern const unsigned numThreads;
    extern std::vector<std::thread> renderThreads;
    extern const s32 bmpChunkSize;
    extern std::atomic<s32> currentRow;
    
    
    
    static std::barrier inPlotBarrier(numThreads, [](){
        inPlot.store(false, std::memory_order_release);
    });


    //extern std::atomic<unsigned int> barrierCounter;
    //extern std::mutex barrierMutex;
    //extern std::condition_variable barrierCV;
    //
    //extern void barrierWait();
    

    void initializeThemeVars();
    
    void initializeUltrahandSettings();


}

#endif
