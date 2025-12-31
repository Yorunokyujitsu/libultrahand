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
 *  Copyright (c) 2023-2025 ppkantorski
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
        #if !USING_FSTREAM_DIRECTIVE
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
        #else
            std::ifstream file(filePath);
            if (!file.is_open()) {
                #if USING_LOGGING_DIRECTIVE
                logMessage("Failed to open JSON file: " + filePath);
                #endif
                return false;
            }
            content.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            file.close();
        #endif
        
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
        
        Service srv;
        if (R_FAILED(smGetService(&srv, "dmnt:cht")))
            return NULL_STR;
        
        if (R_FAILED(serviceDispatch(&srv, 65003))) {
            serviceClose(&srv);
            return NULL_STR;
        }
        
        struct {
            u64 process_id;
            u64 title_id;
            struct { u64 base; u64 size; } main_nso_extents;
            struct { u64 base; u64 size; } heap_extents;
            struct { u64 base; u64 size; } alias_extents;
            struct { u64 base; u64 size; } address_space_extents;
            u8 main_nso_build_id[0x20];
        } metadata;
        
        Result rc = serviceDispatchOut(&srv, 65002, metadata);
        serviceClose(&srv);
        
        if (R_FAILED(rc))
            return NULL_STR;
        
        u64 buildid;
        std::memcpy(&buildid, metadata.main_nso_build_id, sizeof(u64));
        
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
    
    bool firstBoot = true; // for detecting first boot
    
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

    #if IS_LAUNCHER_DIRECTIVE
    std::string ENGLISH = "영어";
    std::string SPANISH = "스페인어";
    std::string FRENCH = "프랑스어";
    std::string GERMAN = "독일어";
    std::string JAPANESE = "일본어";
    std::string KOREAN = "한국어";
    std::string ITALIAN = "이탈리아어";
    std::string DUTCH = "네덜란드어";
    std::string PORTUGUESE = "포루투갈어";
    std::string RUSSIAN = "러시아어";
    std::string UKRAINIAN = "우크라이나어";
    std::string POLISH = "폴란드어";
    std::string SIMPLIFIED_CHINESE = "중국어 (간체)";
    std::string TRADITIONAL_CHINESE = "중국어 (번체)";
    std::string OVERLAYS = "오버레이"; //defined in libTesla now
    std::string OVERLAYS_ABBR = "오버레이";
    std::string OVERLAY = "오버레이";
    std::string HIDDEN_OVERLAYS = "숨겨진 오버레이";
    std::string PACKAGES = "Package+"; //defined in libTesla now
    std::string PACKAGE = "패키지";
    std::string HIDDEN_PACKAGES = "숨겨진 패키지";
    std::string HIDDEN = "숨겨진 항목";
    std::string HIDE_OVERLAY = "오버레이 숨기기";
    std::string HIDE_PACKAGE = "패키지 숨기기";
    std::string LAUNCH_ARGUMENTS = "인수 실행";
    std::string FORCE_AMS110_SUPPORT = "AMS110 강제 지원";
    std::string QUICK_LAUNCH = "빠른 실행";
    std::string BOOT_COMMANDS = "Boot 커맨드";
    std::string EXIT_COMMANDS = "Exit 커맨드";
    std::string ERROR_LOGGING = "오류 로깅";
    std::string COMMANDS = "커맨드";
    std::string SETTINGS = "설정";
    std::string FAVORITE = "즐겨찾기";
    std::string MAIN_SETTINGS = "메인 설정";
    std::string UI_SETTINGS = "UI 변경";

    std::string WIDGET = "위젯";
    std::string WIDGET_ITEMS = "위젯 아이템";
    std::string WIDGET_SETTINGS = "위젯 설정";
    std::string CLOCK = "시각";
    std::string BATTERY = "배터리";
    std::string SOC_TEMPERATURE = "SoC 온도";
    std::string PCB_TEMPERATURE = "PCB 온도";
    std::string BACKDROP = "배경";
    std::string DYNAMIC_COLORS = "동적 색상";
    std::string CENTER_ALIGNMENT = "위젯 가운데 정렬";
    std::string EXTENDED_BACKDROP = "위젯 배경 확장";
    std::string MISCELLANEOUS = "기타";
    //std::string MENU_ITEMS = "메뉴 아이템";
    std::string MENU_SETTINGS = "메뉴 설정";
    std::string USER_GUIDE = "사용 설명서";
    std::string SHOW_HIDDEN = "숨김 항목 표시";
    std::string SHOW_DELETE = "삭제 옵션 표시";
    std::string SHOW_UNSUPPORTED = "미지원 항목 표시";
    std::string PAGE_SWAP = "페이지 교체";
    std::string RIGHT_SIDE_MODE = "우측 배치";
    std::string OVERLAY_VERSIONS = "오버레이 버전";
    std::string PACKAGE_VERSIONS = "패키지 버전";
    std::string CLEAN_VERSIONS = "정리된 버전";
    //std::string VERSION_LABELS = "버전 표시";
    std::string KEY_COMBO = "키 조합";
    std::string MODE = "모드";
    std::string LAUNCH_MODES = "모드";
    std::string LANGUAGE = "언어";
    std::string OVERLAY_INFO = "오버레이 정보";
    std::string SOFTWARE_UPDATE = "업데이트";
    std::string UPDATE_ULTRAHAND = "업데이트";
    std::string UPDATE_LANGUAGES = "언어팩 다운로드";
    std::string SYSTEM = "시스템 정보";
    std::string DEVICE_INFO = "기기 상세";
    std::string FIRMWARE = "펌웨어";
    std::string BOOTLOADER = "부트로더";
    std::string HARDWARE = "하드웨어";
    std::string MEMORY = "메모리";
    std::string VENDOR = "제조사";
    std::string MODEL = "P/N";
    std::string STORAGE = "저장소";
    //std::string NOTICE = "안내";
    //std::string UTILIZES = "2MB 활용";


    std::string OVERLAY_MEMORY = "오버레이 메모리";
    std::string NOT_ENOUGH_MEMORY = "메모리가 부족합니다.";
    std::string WALLPAPER_SUPPORT_DISABLED = "배경화면 비활성화";
    std::string SOUND_SUPPORT_DISABLED = "사운드 비활성화";
    std::string WALLPAPER_SUPPORT_ENABLED = "배경화면 활성화";
    std::string SOUND_SUPPORT_ENABLED = "사운드 활성화";
    std::string EXIT_OVERLAY_SYSTEM = "오버레이 종료";

    //std::string MEMORY_EXPANSION = "여유 ";
    //std::string REBOOT_REQUIRED = "적용을 위해, 재부팅이 필요합니다";
    std::string LOCAL_IP = "로컬 IP";
    std::string WALLPAPER = "배경";
    std::string THEME = "테마";
    std::string DEFAULT = "기본";
    std::string ROOT_PACKAGE = "기본 패키지";
    std::string SORT_PRIORITY = "우선순위 정렬";
    std::string OPTIONS = "옵션";
    std::string FAILED_TO_OPEN = "파일 열기 실패";
    std::string LAUNCH_COMBOS = "실행 버튼";
    std::string NOTIFICATIONS = "알림";
    std::string SOUND_EFFECTS = "사운드 이펙트";
    std::string HAPTIC_FEEDBACK = "햅틱 피드백";
    std::string OPAQUE_SCREENSHOTS = "스크린샷 배경";

    std::string PACKAGE_INFO = "패키지 정보";
    std::string _TITLE = "이름";
    std::string _VERSION= "버전";
    std::string _CREATOR = "개발자";
    std::string _ABOUT = "설명";
    std::string _CREDITS = "기여자";

    std::string USERGUIDE_OFFSET = "177";
    std::string SETTINGS_MENU = "설정 메뉴";
    std::string SCRIPT_OVERLAY = "오버레이 스크립트";
    std::string STAR_FAVORITE = "즐겨찾기";
    std::string APP_SETTINGS = "앱 설정";
    std::string ON_MAIN_MENU = "메인 메뉴";
    std::string ON_A_COMMAND = "커맨드 위에서";
    std::string ON_OVERLAY_PACKAGE = "오버레이/패키지 위에서";
    std::string FEATURES = "이펙트";
    std::string SWIPE_TO_OPEN = "밀어서 열기";

    std::string THEME_SETTINGS = "테마 설정";
    std::string DYNAMIC_LOGO = "동적 로고";
    std::string SELECTION_BACKGROUND = "선택 배경";
    std::string SELECTION_TEXT = "선택 텍스트";
    std::string SELECTION_VALUE = "선택 값";
    std::string LIBULTRAHAND_TITLES = "libultrahand 제목";
    std::string LIBULTRAHAND_VERSIONS = "libultrahand 버전";
    std::string PACKAGE_TITLES = "패키지 제목";

    std::string ULTRAHAND_HAS_STARTED = "Ultrahand 시작됨";
    std::string NEW_UPDATE_IS_AVAILABLE = "새로운 버전이 있습니다!";
    //std::string REBOOT_IS_REQUIRED = "재부팅이 필요합니다";
    //std::string HOLD_A_TO_DELETE = "\uE0E0 홀드하여 삭제";
    std::string DELETE_PACKAGE = "패키지 삭제";
    std::string DELETE_OVERLAY = "오버레이 삭제";
    std::string SELECTION_IS_EMPTY = "비어 있음";
    std::string FORCED_SUPPORT_WARNING = "강제 지원은 위험할 수 있습니다";

    std::string TASK_IS_COMPLETE = "작업 완료!";
    std::string TASK_HAS_FAILED = "작업 실패";

    //std::string PACKAGE_VERSIONS = "패키지 버전";

    //std::string PROGRESS_ANIMATION = "애니메이션";

    /* ASAP Packages */
    std::string UPDATE_ASAP_1 = "테스터 버전을 설치합니다.";
    std::string UPDATE_ASAP_2 = "Atmosphère 와 Hekate만 업데이트됩니다!";
    std::string UPDATE_ASAP_3 = "반드시 시스템 모듈을 모두 끄세요.";
    std::string UPDATE_ASAP_4 = "재부팅 시, ATLAS로 자동 적용됩니다.";
    // Sections
    std::string REBOOT_TO = "다시 시작 · 시스템 종료";
    std::string CFWBOOT = "에뮤/시스낸드 (커펌)";
    std::string OFWBOOT = "시스낸드 (정펌)";
    std::string ANDROID = "Lineage (안드로이드)";
    std::string LAKKA = "Lakka (에뮬레이터)";
    std::string UBUNTU = "Ubuntu (리눅스)";
    std::string REBOOT = "Hekate (메인 메뉴)";
    std::string SHUTDOWN = "시스템 종료";
    // Informations
    std::string CFWBOOT_TITLE = "Extra Setting+";
    std::string HEKATE_TITLE = "Hekate";
    std::string L4T_TITLE = "L4T";
    std::string VOLUME_INFO = "독 모드 볼륨 조절 지원";
    std::string LAUNCHER_INFO = "재부팅 · UMS 도구";
    std::string CFWBOOT_INFO = "에서 선택한 낸드로만 커펌 부팅";
    std::string HEKATE_INFO = "홈 메뉴, UMS (이동식 디스크)로 재부팅";
    std::string BOOT_ENTRY = "로 재부팅, 해당 OS 설치시 사용 가능합니다";
    // Configs
    std::string MAINMENU_INFO = "시스템 확인 및 울트라핸드의 설정을 변경";
    std::string INFO = "정보";
    std::string ABOUT_INFO = "커스텀 패키지 테슬라 메뉴";
    std::string OLD_DEVICE = "구형";
    std::string NEW_DEVICE = "개선판";
    std::string DF_THEME = "Black";
    std::string UPDATE_ULTRA_INFO = "다음 구성 요소를 전부 업데이트합니다";
    std::string UPDATE_TEST_WARN = "테스터 팩에선 오류가 발생할 수 있습니다";
    std::string HIGHLIGHT_LINE = "¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯  　　　　　　";
    std::string UPDATE_TEST_INFO = "ASAP을 테스터 버전으로 교체합니다";
    std::string UPDATE_TEST_NOTICE = "다운로드 이후 자동으로 재부팅합니다";
    // ETC.
    std::string OVERRIDE_SELECTION = "설정 안 함";
    std::string AUTO_SELECTION = "자동";
    // Package-System Clock+
    std::string ENABLED = "활성화";
    std::string SCPLUS_INFO = "Sys-ClkOC Toolkitloader.kip 관리 도구";
    std::string SC_STATUS = " 앱의 사용 여부 선택";
    std::string LDR_TOOL = "Loader.kip 편집 도구";
    std::string LDR_INFO = " 선택시 자동 재부팅";
    std::string TOOL_WARNING_1 = "낸드에 치명적인 손상을 초래할 수 있습니다";
    std::string TOOL_WARNING_2 = "시스낸드에서의 편집/사용을 피해주십시오";
    std::string CPU_VOLTAGE = "CPU 전압 허용 범위";
    std::string CPU_INFO = "단위: mV";
    std::string CPU_CHART = "　 구분";
    std::string USED = "사용중";
    // Package-Extra Setting+
    std::string EXTRA_SETTING = "커스텀 펌웨어 관련 설정을 진행합니다";
    std::string CFW_CFG = "시스템 세부 설정 편집";
    std::string CFW_CFG_INFO = "커스텀 펌웨어의 여러 기능을 설정합니다";
    std::string DEFAULT_NAND = "기본 낸드";
    std::string DEFAULT_NAND_INFO = "Atmosphère 부팅 낸드 설정";
    std::string NO_EMUNAND = "에뮤낸드 없음";
    std::string ADD_ENTRY = "부팅 런처";
    std::string ADD_ENTRY_INFO = "Hekate - Moon 런처 항목 설정";
    std::string MOON_LAUNCHER = "Moon 런처";
    std::string MOON_ENTRY = "부팅 엔트리";
    std::string MOON_LAUNCHER_INFO = "각 엔트리의 아이콘은 자동으로 설정됩니다";
    std::string MOON_LAUNCHER_WARN_1 = "L4T를 부팅하려면 OS의 설치가 필요합니다";
    std::string MOON_LAUNCHER_WARN_2 = "Ubuntu의 경우, 버전을 맞춰 생성하세요";
    std::string MOON_CFW = "기본 낸드로 설정된 커펌 (자동 생성)";
    std::string MOON_STOCK = "커펌 모듈 OFF 정펌 (자동 생성)";
    std::string MOON_ANDROID = "안드로이드 (Lineage OS) 부팅 엔트리";
    std::string MOON_LAKKA = "Libretro 기반 Lakka 부팅 엔트리";
    std::string MOON_UBUNTU = "Switchroot 포팅 LINUX 부팅 엔트리";
    std::string SYSTEM_CFG = "시스템 설정";
    std::string SYSTEM_CFG_INFO = "커펌 시스템 값 설정";
    std::string HEKATE_BOOT = "부팅";
    std::string HEKATE_BOOT_INFO = "시작 관련 설정을 변경합니다";
    std::string HEKATE_BACKLIGHT_INFO = "홈 화면의 백라이트 밝기를 조절합니다";
    std::string HEKATE_BACKLIGHT = "화면 밝기";
    std::string AUTO_HOS_OFF_SEC = "시스템 종료 후";
    std::string AUTO_HOS_OFF_VAL = "자동으로 깨어나지 않도록합니다";
    std::string HEKATE_AUTO_HOS_OFF = "HOS 종료";
    std::string BOOT_WAIT_SEC = "1초 이상";
    std::string BOOT_WAIT_VAL = " 볼륨 입력으로 Hekate로 이동 가능";
    std::string HEKATE_BOOT_WAIT = "부팅 화면 대기 시간";
    std::string BOOT_WAIT_SECOND = "초";
    std::string AUTOBOOT_CFW_SEC = " 버튼";
    std::string AUTOBOOT_CFW_VAL = "재기동 시 Atmosphère로 재시작합니다";
    std::string HEKATE_AUTOBOOT = "자동 부팅";
    std::string SYSTEM_PAGE = "시스템";
    std::string DMNT_CHEAT = "dmnt 치트";
    std::string DMNT_TOGGLE = "dmnt 토글 저장";
    std::string NO_GC_INFO = "게임 카드 설치를 비활성화합니다";
    std::string NO_GC_WARN = "슬립 해제시 카트리지를 인식하면 오류 발생";
    std::string NO_GC = "카트리지 차단";
    std::string REC_INFO = " 버튼 녹화 설정을 변경합니다";
    std::string REC_WARN = "값에 따라 녹화 시간이 변동될 수 있습니다";
    std::string SYSMMC_ONLY = "시스낸드 전용";
    std::string ENABLED_EMUMMC = "에뮤낸드 = 기본 적용";
    std::string EXOSPHERE_INFO = "본체의 시리얼 넘버를 변조합니다";
    std::string EXOSPHERE = "시리얼 변조";
    std::string DNS_INFO = "닌텐도 서버와 연결을 차단합니다";
    std::string DNS_MITM = "서버 차단";
    std::string APPLY_RESET_SEC = "설정 값 적용/리셋";
    std::string APPLY_RESET_VAL = "자동으로 재부팅 됩니다";
    std::string APPLY_CFG = "설정 적용";
    std::string RESET_CFG = "기본 값으로 되돌리기";
    std::string HB_MENU = "홈브류 설정";
    std::string HB_MENU_INFO = "Sphaira 실행 버튼";
    std::string FULL_MEMORY = "풀 메모리 모드";
    std::string FULL_MEMORY_INFO = "메모리 제한 없음";
    std::string FULL_MEMORY_REC = "권장";
    std::string FULL_MEMORY_FORWARDER_VAL = "sphaira 열기  sphaira 아이콘으로 이동";
    std::string FULL_MEMORY_FORWARDER_CTN = "  버튼 입력  바로가기 설치  설치";
    std::string FULL_MEMORY_KEY = "(타이틀) +  홀드 진입을 활성화합니다";
    std::string APPLET_MEMORY = "애플릿 모드";
    std::string APPLET_MEMORY_INFO = "메모리 제한됨";
    std::string APPLET_MEMORY_VAL = " (앨범),  (유저) 진입 설정을 변경합니다";
    std::string APPLET_MEMORY_KEY = "기본:  (앨범) +  홀드";
    std::string APPLET_HB_MENU_ICON = "진입 아이콘";
    std::string APPLET_HB_MENU_KEY = "진입 커맨드";
    std::string S_LANG_PATCH = "한글 패치";
    std::string S_TRANSLATE = "번역 업데이트";
    std::string S_LANG = "ko";
    std::string APPLET_ALBUM = "앨범";
    std::string APPLET_USER = "유저";
    std::string APPLET_KEY_HOLD = "홀드";
    std::string APPLET_KEY_CLICK = "원클릭";
    std::string SD_CLEANUP = "SD 카드 정리";
    std::string SD_CLEANUP_INFO = "Ultrahand 초기화, 정크 삭제";
    std::string NORMAL_DEVICE = "일반 기기";
    std::string PATCH_DEVICE = "8GB 기기";
    std::string RAM_PATCH_WARN = "패치시 일반 기기는 작동하지 않게됩니다";
    std::string RAM_PATCH = "8GB 패치";
    // Package-Quick Guide+
    std::string QUICK_GUIDE_INFO = "간이 설명서오류 코드";
    std::string QUICK_GUIDE = "가이드";
    std::string KEYMAP_INFO = "전반적인 터치 조작 가능";
    std::string KEY_MOVE = "이동";
    std::string KEY_MENU = "메뉴";
    std::string KEY_SELECT = "선택";
    std::string KEYGAP_1 = "    　　";
    std::string KEYGAP_2 = "    ";
    std::string KEYGAP_3 = "    　　";
    std::string PACK_INFO = "패키지 설명서";
    std::string USEFUL = "유용한 기능 (앱 실행 상태)";
    std::string HIDE_SPHAIRA = "'앱 숨기기:' = '  숨기기  켬'";
    std::string FORWARDER_SPHAIRA = "'바로가기 설치:' = '홈브류 +   바로가기 설치'";
    std::string APP_INSTALL = "앱 설치";
    std::string PC_INSTALL = "  기타  FTP·MTP 설치  파일 넣기";
    std::string FTP_INSTALL = "서버 연결  Install 폴더에 파일 끌어넣기";
    std::string MTP_INSTALL = "PC 연결  Install 폴더에 파일 끌어넣기";
    std::string USB_INSTALL = "'2.외장하드:' = '파일 탐색기  마운트  파일 선택'";
    std::string HDD_INSTALL_1 = "하드 연결  파일 탐색기    고급 ";
    std::string HDD_INSTALL_2 = "마운트  하드 선택  파일 선택  예";
    std::string ERROR_INFO = "오류 코드";
    std::string ALBUM_INFO = "앨범 오류";
    std::string ALBUM_ERROR_1 = "sphairahbmenu 혹은 dmnt의 구성 불량";
    std::string ALBUM_ERROR_2 = "애플릿 모드 사용중인 경우, 메모리 부족 충돌";
    std::string CPUDEAD_INFO = "CPU 파손";
    std::string CPU_ERROR = "CPU에 복구 불가능한 손상이 발생";
    std::string SYSTEM_ERROR = "시스템 오류";
    std::string MODULE_ERROR = "모듈 오류";
    std::string MODULE_INFO_1 = "스위치 모듈 오류시 안내문 참고";
    std::string MODULE_INFO_2 = "앱이 아닌 스위치 모듈 오류가 발생한 경우";
    std::string MODULE_INFO_3 = "시스템 파일 손상, 낸드 리빌딩 필요";
    std::string MODULE_INFO_4 = "에뮤낸드 파일 손상, 재생성으로 해결";
    std::string SYSNAND_INFO = "시스낸드";
    std::string EMUNAND_INFO = "에뮤낸드";
    std::string NORMAL_ERROR = "자주 발생하는 오류";
    std::string APP_MODULE_ERROR = "앱 모듈 오류";
    std::string SWITCH_MODULE_ERROR = "스위치 모듈 오류";
    std::string PREV_PAGE = "이전 페이지";
    std::string NEXT_PAGE = "다음 페이지";
    std::string VER_ERR = "버전 오류";
    std::string SD_ERR = "SD 카드 오류";
    std::string STORAGE_ERR = "저장소 오류";
    std::string MISC_ERR = "기타 오류";
    std::string NSS_ERR = "Switch Online Service 오류";
    std::string SERVER_ERR = "서버 오류";
    std::string NETWORK_ERR = "네트워크 오류";
    std::string FORWARDER_ERR = "글로벌 오류";
    std::string FORWARDER_INFO = "NSP 포워더 (바로가기)";
    std::string APP_ERR = "유저 환경에 따라 발생하는 오류";
    std::string APP_MODULE = "홈브류오버레이용 시스템 모듈";
    std::string SCROLL = "스크롤";
    std::string SWITCH_MODULE = "스위치 시스템 모듈";
    // Overlays-Custom infomation
    std::string OVLSYSMODULE = "모듈 관리자";
    std::string EDIZON = "치트 매니저";
    std::string EMUIIBO = "가상 아미보";
    std::string FIZEAU = "색감 매니저";
    std::string NXFANCONTROL = "쿨링 시스템";
    std::string FPSLOCKER = "고정 프레임";
    std::string LDNMITM = "LAN 플레이";
    std::string QUICKNTP = "시간 동기화";
    std::string REVERSENX = "모드 전환기";
    std::string STATUSMONITOR = "상태 모니터";
    std::string STUDIOUSPANCAKE = "빠른 재부팅";
    std::string SYSCLK = "클럭 조정기";
    std::string SYSDVR = "USB 스트리밍";
    std::string SYSPATCH = "패치 시스템";
    #endif

    std::string INCOMPATIBLE_WARNING = "AMS 1.10 미호환";
    std::string SYSTEM_RAM = "시스템 RAM";
    std::string FREE = "여유";

    std::string DEFAULT_CHAR_WIDTH = "0.33";
    std::string UNAVAILABLE_SELECTION = "설정 없음";


    std::string ON = "\uE14B";
    std::string OFF = "\uE14C";

    std::string OK = "확인";
    std::string BACK = "뒤로";
    std::string HIDE = "숨기기";
    std::string CANCEL = "취소";

    std::string GAP_1 = "     ";
    std::string GAP_2 = "  ";

    std::atomic<float> halfGap = 0.0f;
    

    //std::string EMPTY = "비어 있음";
    
    #if USING_WIDGET_DIRECTIVE
    std::string SUNDAY = " 日";
    std::string MONDAY = " 月";
    std::string TUESDAY = " 火";
    std::string WEDNESDAY = " 水";
    std::string THURSDAY = " 木";
    std::string FRIDAY = " 金";
    std::string SATURDAY = " 土";
    
    std::string JANUARY = "1월";
    std::string FEBRUARY = "2월";
    std::string MARCH = "3월";
    std::string APRIL = "4월";
    std::string MAY = "5월";
    std::string JUNE = "6월";
    std::string JULY = "7월";
    std::string AUGUST = "8월";
    std::string SEPTEMBER = "9월";
    std::string OCTOBER = "10월";
    std::string NOVEMBER = "11월";
    std::string DECEMBER = "12월";
    
    std::string SUN = " 日";
    std::string MON = " 月";
    std::string TUE = " 火";
    std::string WED = " 水";
    std::string THU = " 木";
    std::string FRI = " 金";
    std::string SAT = " 土";
    
    std::string JAN = "1월";
    std::string FEB = "2월";
    std::string MAR = "3월";
    std::string APR = "4월";
    std::string MAY_ABBR = "5월";
    std::string JUN = "6월";
    std::string JUL = "7월";
    std::string AUG = "8월";
    std::string SEP = "9월";
    std::string OCT = "10월";
    std::string NOV = "11월";
    std::string DEC = "12월";
    #endif

    
    #if IS_LAUNCHER_DIRECTIVE
    // Constant string definitions (English)
    void reinitializeLangVars() {
        ENGLISH = "영어";
        SPANISH = "스페인어";
        FRENCH = "프랑스어";
        GERMAN = "독일어";
        JAPANESE = "일본어";
        KOREAN = "한국어";
        ITALIAN = "이탈리아어";
        DUTCH = "네덜란드어";
        PORTUGUESE = "포루투갈어";
        RUSSIAN = "러시아어";
        UKRAINIAN = "우크라이나어";
        POLISH = "폴란드어";
        SIMPLIFIED_CHINESE = "중국어 (간체)";
        TRADITIONAL_CHINESE = "중국어 (번체)";
        DEFAULT_CHAR_WIDTH = "0.33";
        UNAVAILABLE_SELECTION = "설정 없음";
        OVERLAYS = "오버레이"; //defined in libTesla now
        OVERLAYS_ABBR = "오버레이";
        OVERLAY = "오버레이";
        HIDDEN_OVERLAYS = "숨겨진 오버레이";
        PACKAGES = "Package+"; //defined in libTesla now
        PACKAGE = "패키지";
        HIDDEN_PACKAGES = "숨겨진 패키지";
        HIDDEN = "숨겨진 항목";
        HIDE_OVERLAY = "오버레이 숨기기";
        HIDE_PACKAGE = "패키지 숨기기";
        LAUNCH_ARGUMENTS = "인수 실행";
        FORCE_AMS110_SUPPORT = "AMS110 강제 지원";
        QUICK_LAUNCH = "빠른 실행";
        BOOT_COMMANDS = "Boot 커맨드";
        EXIT_COMMANDS = "Exit 커맨드";
        ERROR_LOGGING = "오류 로깅";
        COMMANDS = "커맨드";
        SETTINGS = "설정";
        FAVORITE = "즐겨찾기";
        MAIN_SETTINGS = "메인 설정";
        UI_SETTINGS = "UI 변경";
        WIDGET = "위젯";
        WIDGET_ITEMS = "위젯 아이템";
        WIDGET_SETTINGS = "위젯 설정";
        CLOCK = "시각";
        BATTERY = "배터리";
        SOC_TEMPERATURE = "SoC 온도";
        PCB_TEMPERATURE = "PCB 온도";
        BACKDROP = "배경";
        DYNAMIC_COLORS = "동적 색상";
        CENTER_ALIGNMENT = "위젯 가운데 정렬";
        EXTENDED_BACKDROP = "위젯 배경 확장";
        MISCELLANEOUS = "기타";
        //MENU_ITEMS = "메뉴 아이템";
        MENU_SETTINGS = "메뉴 설정";
        USER_GUIDE = "사용 설명서";
        SHOW_HIDDEN = "숨김 항목 표시";
        SHOW_DELETE = "삭제 옵션 표시";
        SHOW_UNSUPPORTED = "미지원 항목 표시";
        PAGE_SWAP = "페이지 교체";
        RIGHT_SIDE_MODE = "우측 배치";
        OVERLAY_VERSIONS = "오버레이 버전";
        PACKAGE_VERSIONS = "패키지 버전";
        CLEAN_VERSIONS = "정리된 버전";
        //VERSION_LABELS = "버전 표시";
        KEY_COMBO = "키 조합";
        MODE = "모드";
        LAUNCH_MODES = "모드";
        LANGUAGE = "언어";
        OVERLAY_INFO = "오버레이 정보";
        SOFTWARE_UPDATE = "업데이트";
        UPDATE_ULTRAHAND = "업데이트";
        UPDATE_LANGUAGES = "언어팩 다운로드";
        SYSTEM = "시스템 정보";
        DEVICE_INFO = "기기 상세";
        FIRMWARE = "펌웨어";
        BOOTLOADER = "부트로더";
        HARDWARE = "하드웨어";
        MEMORY = "메모리";
        VENDOR = "제조사";
        MODEL = "P/N";
        STORAGE = "저장소";
        //NOTICE = "안내";
        //UTILIZES = "2MB 활용";
        SYSTEM_RAM = "시스템 RAM";
        FREE = "여유";
        
        OVERLAY_MEMORY = "오버레이 메모리";
        NOT_ENOUGH_MEMORY = "메모리가 부족합니다.";
        WALLPAPER_SUPPORT_DISABLED = "배경화면 비활성화";
        SOUND_SUPPORT_DISABLED = "사운드 비활성화";
        WALLPAPER_SUPPORT_ENABLED = "배경화면 활성화";
        SOUND_SUPPORT_ENABLED = "사운드 활성화";
        EXIT_OVERLAY_SYSTEM = "오버레이 종료";

        //MEMORY_EXPANSION = "확장";
        //REBOOT_REQUIRED = "적용을 위해, 재부팅이 필요합니다";
        LOCAL_IP = "로컬 IP";
        WALLPAPER = "배경";
        THEME = "테마";
        DEFAULT = "기본";
        ROOT_PACKAGE = "기본 패키지";
        SORT_PRIORITY = "우선순위 정렬";
        OPTIONS = "옵션";
        FAILED_TO_OPEN = "파일 열기 실패";

        LAUNCH_COMBOS = "실행 버튼";
        NOTIFICATIONS = "알림";
        SOUND_EFFECTS = "사운드 이펙트";
        HAPTIC_FEEDBACK = "햅틱 피드백";
        OPAQUE_SCREENSHOTS = "스크린샷 배경";
        ON = "\uE14B";
        OFF = "\uE14C";
        PACKAGE_INFO = "패키지 정보";
        _TITLE = "이름";
        _VERSION= "정보";
        _CREATOR = "개발자";
        _ABOUT = "설명";
        _CREDITS = "기여자";
        OK = "확인";
        BACK = "뒤로";
        HIDE = "숨기기";
        CANCEL = "취소";

        /* ASAP Packages */
        UPDATE_ASAP_1 = "테스터 버전을 설치합니다.";
        UPDATE_ASAP_2 = "Atmosphère 와 Hekate만 업데이트됩니다!";
        UPDATE_ASAP_3 = "반드시 시스템 모듈을 모두 끄세요.";
        UPDATE_ASAP_4 = "재부팅 시, ATLAS로 자동 적용됩니다.";
        // Sections
        REBOOT_TO = "다시 시작 · 시스템 종료";
        CFWBOOT = "에뮤/시스낸드 (커펌)";
        OFWBOOT = "시스낸드 (정펌)";
        ANDROID = "Lineage (안드로이드)";
        LAKKA = "Lakka (에뮬레이터)";
        UBUNTU = "Ubuntu (리눅스)";
        REBOOT = "Hekate (메인 메뉴)";
        SHUTDOWN = "시스템 종료";
        // Informations
        CFWBOOT_TITLE = "Extra Setting+";
        HEKATE_TITLE = "Hekate";
        L4T_TITLE = "L4T";
        VOLUME_INFO = "독 모드 볼륨 조절 지원";
        LAUNCHER_INFO = "재부팅 · UMS 도구";
        CFWBOOT_INFO = "에서 선택한 낸드로만 커펌 부팅";
        HEKATE_INFO = "홈 메뉴, UMS (이동식 디스크)로 재부팅";
        BOOT_ENTRY = "로 재부팅, 해당 OS 설치시 사용 가능합니다";
        // Configs
        MAINMENU_INFO = "시스템 확인 및 울트라핸드의 설정을 변경";
        INFO = "정보";
        ABOUT_INFO = "커스텀 패키지 테슬라 메뉴";
        OLD_DEVICE = "구형";
        NEW_DEVICE = "개선판";
        DF_THEME = "Black";
        UPDATE_ULTRA_INFO = "다음 구성 요소를 전부 업데이트합니다";
        UPDATE_TEST_WARN = "테스터 팩에선 오류가 발생할 수 있습니다";
        HIGHLIGHT_LINE = "¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯  　　　　　　";
        UPDATE_TEST_INFO = "ASAP을 테스터 버전으로 교체합니다";
        UPDATE_TEST_NOTICE = "다운로드 이후 자동으로 재부팅합니다";
        // ETC.
        OVERRIDE_SELECTION = "설정 안 함";
        AUTO_SELECTION = "자동";
        // Package-System Clock+
        ENABLED = "활성화";
        SCPLUS_INFO = "Sys-ClkOC Toolkitloader.kip 관리 도구";
        SC_STATUS = "앱의 사용 여부 선택";
        LDR_TOOL = "Loader.kip 편집 도구";
        LDR_INFO = "선택시 자동 재부팅";
        TOOL_WARNING_1 = "낸드에 치명적인 손상을 초래할 수 있습니다";
        TOOL_WARNING_2 = "시스낸드에서의 편집/사용을 피해주십시오";
        CPU_VOLTAGE = "CPU 전압 허용 범위";
        CPU_INFO = "단위: mV";
        CPU_CHART = "　 구분";
        USED = "사용중";
        // Package-Extra Setting+
        EXTRA_SETTING = "커스텀 펌웨어 관련 설정을 진행합니다";
        CFW_CFG = "시스템 세부 설정 편집";
        CFW_CFG_INFO = "커스텀 펌웨어의 여러 기능을 설정합니다";
        DEFAULT_NAND = "기본 낸드";
        DEFAULT_NAND_INFO = "Atmosphère 부팅 낸드 설정";
        NO_EMUNAND = "에뮤낸드 없음";
        ADD_ENTRY = "부팅 런처";
        ADD_ENTRY_INFO = "Hekate - Moon 런처 항목 설정";
        MOON_LAUNCHER = "Moon 런처";
        MOON_ENTRY = "부팅 엔트리";
        MOON_LAUNCHER_INFO = "각 엔트리의 아이콘은 자동으로 설정됩니다";
        MOON_LAUNCHER_WARN_1 = "L4T를 부팅하려면 OS의 설치가 필요합니다";
        MOON_LAUNCHER_WARN_2 = "Ubuntu의 경우, 버전을 맞춰 생성하세요";
        MOON_CFW = "기본 낸드로 설정된 커펌 (자동 생성)";
        MOON_STOCK = "커펌 모듈 OFF 정펌 (자동 생성)";
        MOON_ANDROID = "안드로이드 (Lineage OS) 부팅 엔트리";
        MOON_LAKKA = "Libretro 기반 Lakka 부팅 엔트리";
        MOON_UBUNTU = "Switchroot 포팅 LINUX 부팅 엔트리";
        SYSTEM_CFG = "시스템 설정";
        SYSTEM_CFG_INFO = "커펌 시스템 값 설정";
        HEKATE_BOOT = "부팅";
        HEKATE_BOOT_INFO = "시작 관련 설정을 변경합니다";
        HEKATE_BACKLIGHT_INFO = "홈 화면의 백라이트 밝기를 조절합니다";
        HEKATE_BACKLIGHT = "화면 밝기";
        AUTO_HOS_OFF_SEC = "시스템 종료 후";
        AUTO_HOS_OFF_VAL = "자동으로 깨어나지 않도록합니다";
        HEKATE_AUTO_HOS_OFF = "HOS 종료";
        BOOT_WAIT_SEC = "1초 이상";
        BOOT_WAIT_VAL = " 볼륨 입력으로 Hekate로 이동 가능";
        HEKATE_BOOT_WAIT = "부팅 화면 대기 시간";
        BOOT_WAIT_SECOND = "초";
        AUTOBOOT_CFW_SEC = " 버튼";
        AUTOBOOT_CFW_VAL = "재기동 시 Atmosphère로 재시작합니다";
        HEKATE_AUTOBOOT = "자동 부팅";
        SYSTEM_PAGE = "시스템";
        DMNT_CHEAT = "dmnt 치트";
        DMNT_TOGGLE = "dmnt 토글 저장";
        NO_GC_INFO = "게임 카드 설치를 비활성화합니다";
        NO_GC_WARN = "슬립 해제시 카트리지를 인식하면 오류 발생";
        NO_GC = "카트리지 차단";
        REC_INFO = " 버튼 녹화 설정을 변경합니다";
        REC_WARN = "값에 따라 녹화 시간이 변동될 수 있습니다";
        SYSMMC_ONLY = "시스낸드 전용";
        ENABLED_EMUMMC = "에뮤낸드 = 기본 적용";
        EXOSPHERE_INFO = "본체의 시리얼 넘버를 변조합니다";
        EXOSPHERE = "시리얼 변조";
        DNS_INFO = "닌텐도 서버와 연결을 차단합니다";
        DNS_MITM = "서버 차단";
        APPLY_RESET_SEC = "설정 값 적용리셋";
        APPLY_RESET_VAL = "자동으로 재부팅 됩니다";
        APPLY_CFG = "설정 적용";
        RESET_CFG = "기본 값으로 되돌리기";
        HB_MENU = "홈브류 설정";
        HB_MENU_INFO = "Sphaira 실행 버튼";
        FULL_MEMORY = "풀 메모리 모드";
        FULL_MEMORY_INFO = "메모리 제한 없음";
        FULL_MEMORY_REC = "권장";
        FULL_MEMORY_FORWARDER_VAL = "sphaira 열기  sphaira 아이콘으로 이동";
        FULL_MEMORY_FORWARDER_CTN = "  버튼 입력  바로가기 설치  설치";
        FULL_MEMORY_KEY = "(타이틀) +  홀드 진입을 활성화합니다";
        APPLET_MEMORY = "애플릿 모드";
        APPLET_MEMORY_INFO = "메모리 제한됨";
        APPLET_MEMORY_VAL = " (앨범),  (유저) 진입 설정을 변경합니다";
        APPLET_MEMORY_KEY = "기본:  (앨범) +  홀드";
        APPLET_HB_MENU_ICON = "진입 아이콘";
        APPLET_HB_MENU_KEY = "진입 커맨드";
        S_LANG_PATCH = "한글 패치";
        S_TRANSLATE = "번역 업데이트";
        S_LANG = "ko";
        APPLET_ALBUM = "앨범";
        APPLET_USER = "유저";
        APPLET_KEY_HOLD = "홀드";
        APPLET_KEY_CLICK = "원클릭";
        SD_CLEANUP = "SD 카드 정리";
        SD_CLEANUP_INFO = "Ultrahand 초기화, 정크 삭제";
        NORMAL_DEVICE = "일반 기기";
        PATCH_DEVICE = "8GB 기기";
        RAM_PATCH_WARN = "패치시 일반 기기는 작동하지 않게됩니다";
        RAM_PATCH = "8GB 패치";
        // Package-Quick Guide+
        QUICK_GUIDE_INFO = "간이 설명서오류 코드";
        QUICK_GUIDE = "가이드";
        KEYMAP_INFO = "전반적인 터치 조작 가능";
        KEY_MOVE = "이동";
        KEY_MENU = "메뉴";
        KEY_SELECT = "선택";
        KEYGAP_1 = "    　　";
        KEYGAP_2 = "    ";
        KEYGAP_3 = "    　　";
        PACK_INFO = "패키지 설명서";
        USEFUL = "유용한 기능 (앱 실행 상태)";
        HIDE_SPHAIRA = "'앱 숨기기:' = '  숨기기  켬'";
        FORWARDER_SPHAIRA = "'바로가기 설치:' = '홈브류 +   바로가기 설치'";
        APP_INSTALL = "앱 설치";
        PC_INSTALL = "  기타  FTP·MTP 설치  파일 넣기";
        FTP_INSTALL = "서버 연결  Install 폴더에 파일 끌어넣기";
        MTP_INSTALL = "PC 연결  Install 폴더에 파일 끌어넣기";
        USB_INSTALL = "'2.외장하드:' = '파일 탐색기  마운트  파일 선택'";
        HDD_INSTALL_1 = "하드 연결  파일 탐색기    고급 ";
        HDD_INSTALL_2 = "마운트  하드 선택  파일 선택  예";
        ERROR_INFO = "오류 코드";
        ALBUM_INFO = "앨범 오류";
        ALBUM_ERROR_1 = "sphairahbmenu 혹은 dmnt의 구성 불량";
        ALBUM_ERROR_2 = "애플릿 모드 사용중인 경우, 메모리 부족 충돌";
        CPUDEAD_INFO = "CPU 파손";
        CPU_ERROR = "CPU에 복구 불가능한 손상이 발생";
        SYSTEM_ERROR = "시스템 오류";
        MODULE_ERROR = "모듈 오류";
        MODULE_INFO_1 = "스위치 모듈 오류시 안내문 참고";
        MODULE_INFO_2 = "앱이 아닌 스위치 모듈 오류가 발생한 경우";
        MODULE_INFO_3 = "시스템 파일 손상, 낸드 리빌딩 필요";
        MODULE_INFO_4 = "에뮤낸드 파일 손상, 재생성으로 해결";
        SYSNAND_INFO = "시스낸드";
        EMUNAND_INFO = "에뮤낸드";
        NORMAL_ERROR = "자주 발생하는 오류";
        APP_MODULE_ERROR = "앱 모듈 오류";
        SWITCH_MODULE_ERROR = "스위치 모듈 오류";
        PREV_PAGE = "이전 페이지";
        NEXT_PAGE = "다음 페이지";
        VER_ERR = "버전 오류";
        SD_ERR = "SD 카드 오류";
        STORAGE_ERR = "저장소 오류";
        MISC_ERR = "기타 오류";
        NSS_ERR = "Switch Online Service 오류";
        SERVER_ERR = "서버 오류";
        NETWORK_ERR = "네트워크 오류";
        FORWARDER_ERR = "글로벌 오류";
        FORWARDER_INFO = "NSP 포워더 (바로가기)";
        APP_ERR = "유저 환경에 따라 발생하는 오류";
        APP_MODULE = "홈브류오버레이용 시스템 모듈";
        SCROLL = "스크롤";
        SWITCH_MODULE = "스위치 시스템 모듈";
        // Overlays-Custom infomation
        OVLSYSMODULE = "모듈 관리자";
        EDIZON = "치트 매니저";
        EMUIIBO = "가상 아미보";
        FIZEAU = "색감 매니저";
        NXFANCONTROL = "쿨링 시스템";
        FPSLOCKER = "고정 프레임";
        LDNMITM = "LAN 플레이";
        QUICKNTP = "시간 동기화";
        REVERSENX = "모드 전환기";
        STATUSMONITOR = "상태 모니터";
        STUDIOUSPANCAKE = "빠른 재부팅";
        SYSCLK = "클럭 조정기";
        SYSDVR = "USB 스트리밍";
        SYSPATCH = "패치 시스템";
        GAP_1 = "     ";
        GAP_2 = "  ";

        USERGUIDE_OFFSET = "177";
        SETTINGS_MENU = "설정 메뉴";
        SCRIPT_OVERLAY = "오버레이 스크립트";
        STAR_FAVORITE = "즐겨찾기";
        APP_SETTINGS = "앱 설정";
        ON_MAIN_MENU = "메인 메뉴";
        ON_A_COMMAND = "커맨드 위에서";
        ON_OVERLAY_PACKAGE = "오버레이/패키지 위에서";
        FEATURES = "이펙트";
        SWIPE_TO_OPEN = "밀어서 열기";
        //PROGRESS_ANIMATION = "애니메이션";

        THEME_SETTINGS = "테마 설정";
        DYNAMIC_LOGO = "동적 로고";
        SELECTION_BACKGROUND = "선택 배경";
        SELECTION_TEXT = "선택 텍스트";
        SELECTION_VALUE = "선택 값";
        LIBULTRAHAND_TITLES = "libultrahand 제목";
        LIBULTRAHAND_VERSIONS = "libultrahand 버전";
        PACKAGE_TITLES = "패키지 제목";
        //PACKAGE_VERSIONS = "패키지 버전";

        ULTRAHAND_HAS_STARTED = "Ultrahand 시작됨";
        NEW_UPDATE_IS_AVAILABLE = "새로운 버전이 있습니다!";
        //REBOOT_IS_REQUIRED = "재부팅이 필요합니다";
        //HOLD_A_TO_DELETE = " 홀드하여 삭제";
        DELETE_PACKAGE = "패키지 삭제";
        DELETE_OVERLAY = "오버레이 삭제";
        SELECTION_IS_EMPTY = "비어 있음";
        FORCED_SUPPORT_WARNING = "강제 지원은 위험할 수 있습니다";
        INCOMPATIBLE_WARNING = "AMS 1.10 미호환";
        TASK_IS_COMPLETE = "작업 완료!";
        TASK_HAS_FAILED = "작업 실패";

        //EMPTY = "비어 있음";
    
        SUNDAY = " 日";
        MONDAY = " 月";
        TUESDAY = " 火";
        WEDNESDAY = " 水";
        THURSDAY = " 木";
        FRIDAY = " 金";
        SATURDAY = " 土";
        
        JANUARY = "1월";
        FEBRUARY = "2월";
        MARCH = "3월";
        APRIL = "4월";
        MAY = "5월";
        JUNE = "6월";
        JULY = "7월";
        AUGUST = "8월";
        SEPTEMBER = "9월";
        OCTOBER = "10월";
        NOVEMBER = "11월";
        DECEMBER = "12월";
        
        SUN = " 日";
        MON = " 月";
        TUE = " 火";
        WED = " 水";
        THU = " 木";
        FRI = " 金";
        SAT = " 土";
        
        JAN = "1월";
        FEB = "2월";
        MAR = "3월";
        APR = "4월";
        MAY_ABBR = "5월";
        JUN = "6월";
        JUL = "7월";
        AUG = "8월";
        SEP = "9월";
        OCT = "10월";
        NOV = "11월";
        DEC = "12월";
    }
    #endif
    
    
    
    // Function to update a constant if the new value from JSON is not empty
    void updateIfNotEmpty(std::string& constant, const std::string& newValue) {
        if (!newValue.empty()) {
            constant = newValue;
        }
    }

    void parseLanguage(const std::string& langFile) {
        // Map to store parsed JSON data
        std::unordered_map<std::string, std::string> jsonMap;
        if (!parseJsonToMap(langFile, jsonMap)) {
            #if USING_LOGGING_DIRECTIVE
            logMessage("Failed to parse language file: " + langFile);
            #endif
            return;
        }

        
        std::unordered_map<std::string, std::string*> configMap = {
            #if IS_LAUNCHER_DIRECTIVE
            {"ENGLISH", &ENGLISH},
            {"SPANISH", &SPANISH},
            {"FRENCH", &FRENCH},
            {"GERMAN", &GERMAN},
            {"JAPANESE", &JAPANESE},
            {"KOREAN", &KOREAN},
            {"ITALIAN", &ITALIAN},
            {"DUTCH", &DUTCH},
            {"PORTUGUESE", &PORTUGUESE},
            {"RUSSIAN", &RUSSIAN},
            {"UKRAINIAN", &UKRAINIAN},
            {"POLISH", &POLISH},
            {"SIMPLIFIED_CHINESE", &SIMPLIFIED_CHINESE},
            {"TRADITIONAL_CHINESE", &TRADITIONAL_CHINESE},
            {"OVERLAYS", &OVERLAYS},
            {"OVERLAYS_ABBR", &OVERLAYS_ABBR},
            {"OVERLAY", &OVERLAY},
            {"HIDDEN_OVERLAYS", &HIDDEN_OVERLAYS},
            {"PACKAGES", &PACKAGES},
            {"PACKAGE", &PACKAGE},
            {"HIDDEN_PACKAGES", &HIDDEN_PACKAGES},
            {"HIDDEN", &HIDDEN},
            {"HIDE_PACKAGE", &HIDE_PACKAGE},
            {"HIDE_OVERLAY", &HIDE_OVERLAY},
            {"LAUNCH_ARGUMENTS", &LAUNCH_ARGUMENTS},
            {"FORCE_AMS110_SUPPORT", &FORCE_AMS110_SUPPORT},
            {"QUICK_LAUNCH", &QUICK_LAUNCH},
            {"BOOT_COMMANDS", &BOOT_COMMANDS},
            {"EXIT_COMMANDS", &EXIT_COMMANDS},
            {"ERROR_LOGGING", &ERROR_LOGGING},
            {"COMMANDS", &COMMANDS},
            {"SETTINGS", &SETTINGS},
            {"FAVORITE", &FAVORITE},
            {"MAIN_SETTINGS", &MAIN_SETTINGS},
            {"UI_SETTINGS", &UI_SETTINGS},

            {"WIDGET", &WIDGET},
            {"WIDGET_ITEMS", &WIDGET_ITEMS},
            {"WIDGET_SETTINGS", &WIDGET_SETTINGS},
            {"CLOCK", &CLOCK},
            {"BATTERY", &BATTERY},
            {"SOC_TEMPERATURE", &SOC_TEMPERATURE},
            {"PCB_TEMPERATURE", &PCB_TEMPERATURE},
            {"BACKDROP", &BACKDROP},
            {"DYNAMIC_COLORS", &DYNAMIC_COLORS},
            {"CENTER_ALIGNMENT", &CENTER_ALIGNMENT},
            {"EXTENDED_BACKDROP", &EXTENDED_BACKDROP},
            {"MISCELLANEOUS", &MISCELLANEOUS},
            //{"MENU_ITEMS", &MENU_ITEMS},
            {"MENU_SETTINGS", &MENU_SETTINGS},
            {"USER_GUIDE", &USER_GUIDE},
            {"SHOW_HIDDEN", &SHOW_HIDDEN},
            {"SHOW_DELETE", &SHOW_DELETE},
            {"SHOW_UNSUPPORTED", &SHOW_UNSUPPORTED},
            {"PAGE_SWAP", &PAGE_SWAP},
            {"RIGHT_SIDE_MODE", &RIGHT_SIDE_MODE},
            {"OVERLAY_VERSIONS", &OVERLAY_VERSIONS},
            {"PACKAGE_VERSIONS", &PACKAGE_VERSIONS},
            {"CLEAN_VERSIONS", &CLEAN_VERSIONS},
            //{"VERSION_LABELS", &VERSION_LABELS},
            {"KEY_COMBO", &KEY_COMBO},
            {"MODE", &MODE},
            {"LAUNCH_MODES", &LAUNCH_MODES},
            {"LANGUAGE", &LANGUAGE},
            {"OVERLAY_INFO", &OVERLAY_INFO},
            {"SOFTWARE_UPDATE", &SOFTWARE_UPDATE},
            {"UPDATE_ULTRAHAND", &UPDATE_ULTRAHAND},
            {"UPDATE_LANGUAGES", &UPDATE_LANGUAGES},
            {"SYSTEM", &SYSTEM},
            {"DEVICE_INFO", &DEVICE_INFO},
            {"FIRMWARE", &FIRMWARE},
            {"BOOTLOADER", &BOOTLOADER},
            {"HARDWARE", &HARDWARE},
            {"MEMORY", &MEMORY},
            {"VENDOR", &VENDOR},
            {"MODEL", &MODEL},
            {"STORAGE", &STORAGE},
            //{"NOTICE", &NOTICE},
            //{"UTILIZES", &UTILIZES},

            {"OVERLAY_MEMORY", &OVERLAY_MEMORY},
            {"NOT_ENOUGH_MEMORY", &NOT_ENOUGH_MEMORY},
            {"WALLPAPER_SUPPORT_DISABLED", &WALLPAPER_SUPPORT_DISABLED},
            {"SOUND_SUPPORT_DISABLED", &SOUND_SUPPORT_DISABLED},
            {"WALLPAPER_SUPPORT_ENABLED", &WALLPAPER_SUPPORT_ENABLED},
            {"SOUND_SUPPORT_ENABLED", &SOUND_SUPPORT_ENABLED},
            {"EXIT_OVERLAY_SYSTEM", &EXIT_OVERLAY_SYSTEM},

            //{"MEMORY_EXPANSION", &MEMORY_EXPANSION},
            //{"REBOOT_REQUIRED", &REBOOT_REQUIRED},
            {"LOCAL_IP", &LOCAL_IP},
            {"WALLPAPER", &WALLPAPER},
            {"THEME", &THEME},
            {"DEFAULT", &DEFAULT},
            {"ROOT_PACKAGE", &ROOT_PACKAGE},
            {"SORT_PRIORITY", &SORT_PRIORITY},
            {"OPTIONS", &OPTIONS},
            {"FAILED_TO_OPEN", &FAILED_TO_OPEN},

            {"LAUNCH_COMBOS", &LAUNCH_COMBOS},
            {"NOTIFICATIONS", &NOTIFICATIONS},
            {"SOUND_EFFECTS", &SOUND_EFFECTS},
            {"HAPTIC_FEEDBACK", &HAPTIC_FEEDBACK},
            {"OPAQUE_SCREENSHOTS", &OPAQUE_SCREENSHOTS},

            {"PACKAGE_INFO", &PACKAGE_INFO},
            {"TITLE", &_TITLE},
            {"VERSION", &_VERSION},
            {"CREATOR", &_CREATOR},
            {"ABOUT", &_ABOUT},
            {"CREDITS", &_CREDITS},

            {"USERGUIDE_OFFSET", &USERGUIDE_OFFSET},
            {"SETTINGS_MENU", &SETTINGS_MENU},
            {"SCRIPT_OVERLAY", &SCRIPT_OVERLAY},
            {"STAR_FAVORITE", &STAR_FAVORITE},
            {"APP_SETTINGS", &APP_SETTINGS},
            {"ON_MAIN_MENU", &ON_MAIN_MENU},
            {"ON_A_COMMAND", &ON_A_COMMAND},
            {"ON_OVERLAY_PACKAGE", &ON_OVERLAY_PACKAGE},
            {"FEATURES", &FEATURES},
            {"SWIPE_TO_OPEN", &SWIPE_TO_OPEN},

            {"THEME_SETTINGS", &THEME_SETTINGS},
            {"DYNAMIC_LOGO", &DYNAMIC_LOGO},
            {"SELECTION_BACKGROUND", &SELECTION_BACKGROUND},
            {"SELECTION_TEXT", &SELECTION_TEXT},
            {"SELECTION_VALUE", &SELECTION_VALUE},
            {"LIBULTRAHAND_TITLES", &LIBULTRAHAND_TITLES},
            {"LIBULTRAHAND_VERSIONS", &LIBULTRAHAND_VERSIONS},
            {"PACKAGE_TITLES", &PACKAGE_TITLES},

            {"ULTRAHAND_HAS_STARTED", &ULTRAHAND_HAS_STARTED},
            {"NEW_UPDATE_IS_AVAILABLE", &NEW_UPDATE_IS_AVAILABLE},
            //{"REBOOT_IS_REQUIRED", &REBOOT_IS_REQUIRED},
            //{"HOLD_A_TO_DELETE", &HOLD_A_TO_DELETE},
            {"DELETE_PACKAGE", &DELETE_PACKAGE},
            {"DELETE_OVERLAY", &DELETE_OVERLAY},
            {"SELECTION_IS_EMPTY", &SELECTION_IS_EMPTY},
            {"FORCED_SUPPORT_WARNING", &FORCED_SUPPORT_WARNING},
            {"INCOMPATIBLE_WARNING", &INCOMPATIBLE_WARNING},
            {"TASK_IS_COMPLETE", &TASK_IS_COMPLETE},
            {"TASK_HAS_FAILED", &TASK_HAS_FAILED},

            //{"PACKAGE_VERSIONS", &PACKAGE_VERSIONS},
            //{"PROGRESS_ANIMATION", &PROGRESS_ANIMATION},

            /* ASAP Packages */
            {"UPDATE_ASAP_1", &UPDATE_ASAP_1},
            {"UPDATE_ASAP_2", &UPDATE_ASAP_2},
            {"UPDATE_ASAP_3", &UPDATE_ASAP_3},
            {"UPDATE_ASAP_4", &UPDATE_ASAP_4},
            // Sections
            {"REBOOT_TO", &REBOOT_TO},
            {"CFWBOOT", &CFWBOOT},
            {"OFWBOOT", &OFWBOOT},
            {"ANDROID", &ANDROID},
            {"LAKKA", &LAKKA},
            {"UBUNTU", &UBUNTU},
            {"REBOOT", &REBOOT},
            {"SHUTDOWN", &SHUTDOWN},
            // Informations
            {"CFWBOOT_TITLE", &CFWBOOT_TITLE},
            {"HEKATE_TITLE", &HEKATE_TITLE},
            {"L4T_TITLE", &L4T_TITLE},
            {"VOLUME_INFO", &VOLUME_INFO},
            {"LAUNCHER_INFO", &LAUNCHER_INFO},
            {"CFWBOOT_INFO", &CFWBOOT_INFO},
            {"HEKATE_INFO", &HEKATE_INFO},
            {"BOOT_ENTRY", &BOOT_ENTRY},
            // Configs
            {"MAINMENU_INFO", &MAINMENU_INFO},
            {"INFO", &INFO},
            {"ABOUT_INFO", &ABOUT_INFO},
            {"OLD_DEVICE", &OLD_DEVICE},
            {"NEW_DEVICE", &NEW_DEVICE},
            {"DF_THEME", &DF_THEME},
            {"UPDATE_ULTRA_INFO", &UPDATE_ULTRA_INFO},
            {"UPDATE_TEST_WARN", &UPDATE_TEST_WARN},
            {"HIGHLIGHT_LINE", &HIGHLIGHT_LINE},
            {"UPDATE_TEST_INFO", &UPDATE_TEST_INFO},
            {"UPDATE_TEST_NOTICE", &UPDATE_TEST_NOTICE},
            // ETC.
            {"OVERRIDE_SELECTION", &OVERRIDE_SELECTION},
            {"AUTO_SELECTION", &AUTO_SELECTION},
            // Package-System Clock+
            {"ENABLED", &ENABLED},
            {"SCPLUS_INFO", &SCPLUS_INFO},
            {"SC_STATUS", &SC_STATUS},
            {"LDR_TOOL", &LDR_TOOL},
            {"LDR_INFO", &LDR_INFO},
            {"TOOL_WARNING_1", &TOOL_WARNING_1},
            {"TOOL_WARNING_2", &TOOL_WARNING_2},
            {"CPU_VOLTAGE", &CPU_VOLTAGE},
            {"CPU_INFO", &CPU_INFO},
            {"CPU_CHART", &CPU_CHART},
            {"USED", &USED},
            // Package-Extra Setting+
            {"EXTRA_SETTING", &EXTRA_SETTING},
            {"CFW_CFG", &CFW_CFG},
            {"CFW_CFG_INFO", &CFW_CFG_INFO},
            {"DEFAULT_NAND", &DEFAULT_NAND},
            {"DEFAULT_NAND_INFO", &DEFAULT_NAND_INFO},
            {"NO_EMUNAND", &NO_EMUNAND},
            {"ADD_ENTRY", &ADD_ENTRY},
            {"ADD_ENTRY_INFO", &ADD_ENTRY_INFO},
            {"MOON_LAUNCHER", &MOON_LAUNCHER},
            {"MOON_ENTRY", &MOON_ENTRY},
            {"MOON_LAUNCHER_INFO", &MOON_LAUNCHER_INFO},
            {"MOON_LAUNCHER_WARN_1", &MOON_LAUNCHER_WARN_1},
            {"MOON_LAUNCHER_WARN_2", &MOON_LAUNCHER_WARN_2},
            {"MOON_CFW", &MOON_CFW},
            {"MOON_STOCK", &MOON_STOCK},
            {"MOON_ANDROID", &MOON_ANDROID},
            {"MOON_LAKKA", &MOON_LAKKA},
            {"MOON_UBUNTU", &MOON_UBUNTU},
            {"SYSTEM_CFG", &SYSTEM_CFG},
            {"SYSTEM_CFG_INFO", &SYSTEM_CFG_INFO},
            {"HEKATE_BOOT", &HEKATE_BOOT},
            {"HEKATE_BOOT_INFO", &HEKATE_BOOT_INFO},
            {"HEKATE_BACKLIGHT_INFO", &HEKATE_BACKLIGHT_INFO},
            {"HEKATE_BACKLIGHT", &HEKATE_BACKLIGHT},
            {"AUTO_HOS_OFF_SEC", &AUTO_HOS_OFF_SEC},
            {"AUTO_HOS_OFF_VAL", &AUTO_HOS_OFF_VAL},
            {"HEKATE_AUTO_HOS_OFF", &HEKATE_AUTO_HOS_OFF},
            {"BOOT_WAIT_SEC", &BOOT_WAIT_SEC},
            {"BOOT_WAIT_VAL", &BOOT_WAIT_VAL},
            {"HEKATE_BOOT_WAIT", &HEKATE_BOOT_WAIT},
            {"BOOT_WAIT_SECOND", &BOOT_WAIT_SECOND},
            {"AUTOBOOT_CFW_SEC", &AUTOBOOT_CFW_SEC},
            {"AUTOBOOT_CFW_VAL", &AUTOBOOT_CFW_VAL},
            {"HEKATE_AUTOBOOT", &HEKATE_AUTOBOOT},
            {"SYSTEM_PAGE", &SYSTEM_PAGE},
            {"DMNT_CHEAT", &DMNT_CHEAT},
            {"DMNT_TOGGLE", &DMNT_TOGGLE},
            {"NO_GC_INFO", &NO_GC_INFO},
            {"NO_GC_WARN", &NO_GC_WARN},
            {"NO_GC", &NO_GC},
            {"REC_INFO", &REC_INFO},
            {"REC_WARN", &REC_WARN},
            {"SYSMMC_ONLY", &SYSMMC_ONLY},
            {"ENABLED_EMUMMC", &ENABLED_EMUMMC},
            {"EXOSPHERE_INFO", &EXOSPHERE_INFO},
            {"EXOSPHERE", &EXOSPHERE},
            {"DNS_INFO", &DNS_INFO},
            {"DNS_MITM", &DNS_MITM},
            {"APPLY_RESET_SEC", &APPLY_RESET_SEC},
            {"APPLY_RESET_VAL", &APPLY_RESET_VAL},
            {"APPLY_CFG", &APPLY_CFG},
            {"RESET_CFG", &RESET_CFG},
            {"HB_MENU", &HB_MENU},
            {"HB_MENU_INFO", &HB_MENU_INFO},
            {"FULL_MEMORY", &FULL_MEMORY},
            {"FULL_MEMORY_INFO", &FULL_MEMORY_INFO},
            {"FULL_MEMORY_REC", &FULL_MEMORY_REC},
            {"FULL_MEMORY_FORWARDER_VAL", &FULL_MEMORY_FORWARDER_VAL},
            {"FULL_MEMORY_FORWARDER_CTN", &FULL_MEMORY_FORWARDER_CTN},
            {"FULL_MEMORY_KEY", &FULL_MEMORY_KEY},
            {"APPLET_MEMORY", &APPLET_MEMORY},
            {"APPLET_MEMORY_INFO", &APPLET_MEMORY_INFO},
            {"APPLET_MEMORY_VAL", &APPLET_MEMORY_VAL},
            {"APPLET_MEMORY_KEY", &APPLET_MEMORY_KEY},
            {"APPLET_HB_MENU_ICON", &APPLET_HB_MENU_ICON},
            {"APPLET_HB_MENU_KEY", &APPLET_HB_MENU_KEY},
            {"S_LANG_PATCH", &S_LANG_PATCH},
            {"S_TRANSLATE", &S_TRANSLATE},
            {"S_LANG", &S_LANG},
            {"APPLET_ALBUM", &APPLET_ALBUM},
            {"APPLET_USER", &APPLET_USER},
            {"APPLET_KEY_HOLD", &APPLET_KEY_HOLD},
            {"APPLET_KEY_CLICK", &APPLET_KEY_CLICK},
            {"SD_CLEANUP", &SD_CLEANUP},
            {"SD_CLEANUP_INFO", &SD_CLEANUP_INFO},
            {"NORMAL_DEVICE", &NORMAL_DEVICE},
            {"PATCH_DEVICE", &PATCH_DEVICE},
            {"RAM_PATCH_WARN", &RAM_PATCH_WARN},
            {"RAM_PATCH", &RAM_PATCH},
            // Package-Quick Guide+
            {"QUICK_GUIDE_INFO", &QUICK_GUIDE_INFO},
            {"QUICK_GUIDE", &QUICK_GUIDE},
            {"KEYMAP_INFO", &KEYMAP_INFO},
            {"KEY_MOVE", &KEY_MOVE},
            {"KEY_MENU", &KEY_MENU},
            {"KEY_SELECT", &KEY_SELECT},
            {"KEYGAP_1", &KEYGAP_1},
            {"KEYGAP_2", &KEYGAP_2},
            {"KEYGAP_3", &KEYGAP_3},
            {"PACK_INFO", &PACK_INFO},
            {"USEFUL", &USEFUL},
            {"HIDE_SPHAIRA", &HIDE_SPHAIRA},
            {"FORWARDER_SPHAIRA", &FORWARDER_SPHAIRA},
            {"APP_INSTALL", &APP_INSTALL},
            {"PC_INSTALL", &PC_INSTALL},
            {"FTP_INSTALL", &FTP_INSTALL},
            {"MTP_INSTALL", &MTP_INSTALL},
            {"USB_INSTALL", &USB_INSTALL},
            {"HDD_INSTALL_1", &HDD_INSTALL_1},
            {"HDD_INSTALL_2", &HDD_INSTALL_2},
            {"ERROR_INFO", &ERROR_INFO},
            {"ALBUM_INFO", &ALBUM_INFO},
            {"ALBUM_ERROR_1", &ALBUM_ERROR_1},
            {"ALBUM_ERROR_2", &ALBUM_ERROR_2},
            {"CPUDEAD_INFO", &CPUDEAD_INFO},
            {"CPU_ERROR", &CPU_ERROR},
            {"SYSTEM_ERROR", &SYSTEM_ERROR},
            {"MODULE_ERROR", &MODULE_ERROR},
            {"MODULE_INFO_1", &MODULE_INFO_1},
            {"MODULE_INFO_2", &MODULE_INFO_2},
            {"MODULE_INFO_3", &MODULE_INFO_3},
            {"MODULE_INFO_4", &MODULE_INFO_4},
            {"SYSNAND_INFO", &SYSNAND_INFO},
            {"EMUNAND_INFO", &EMUNAND_INFO},
            {"NORMAL_ERROR", &NORMAL_ERROR},
            {"APP_MODULE_ERROR", &APP_MODULE_ERROR},
            {"SWITCH_MODULE_ERROR", &SWITCH_MODULE_ERROR},
            {"PREV_PAGE", &PREV_PAGE},
            {"NEXT_PAGE", &NEXT_PAGE},
            {"VER_ERR", &VER_ERR},
            {"SD_ERR", &SD_ERR},
            {"STORAGE_ERR", &STORAGE_ERR},
            {"MISC_ERR", &MISC_ERR},
            {"NSS_ERR", &NSS_ERR},
            {"SERVER_ERR", &SERVER_ERR},
            {"NETWORK_ERR", &NETWORK_ERR},
            {"FORWARDER_ERR", &FORWARDER_ERR},
            {"FORWARDER_INFO", &FORWARDER_INFO},
            {"APP_ERR", &APP_ERR},
            {"APP_MODULE", &APP_MODULE},
            {"SCROLL", &SCROLL},
            {"SWITCH_MODULE", &SWITCH_MODULE},
            // Overlays-Custom infomation
            {"OVLSYSMODULE", &OVLSYSMODULE},
            {"EDIZON", &EDIZON},
            {"EMUIIBO", &EMUIIBO},
            {"FIZEAU", &FIZEAU},
            {"NXFANCONTROL", &NXFANCONTROL},
            {"FPSLOCKER", &FPSLOCKER},
            {"LDNMITM", &LDNMITM},
            {"QUICKNTP", &QUICKNTP},
            {"REVERSENX", &REVERSENX},
            {"STATUSMONITOR", &STATUSMONITOR},
            {"STUDIOUSPANCAKE", &STUDIOUSPANCAKE},
            {"SYSCLK", &SYSCLK},
            {"SYSDVR", &SYSDVR},
            {"SYSPATCH", &SYSPATCH},
            #endif

            {"SYSTEM_RAM", &SYSTEM_RAM},
            {"FREE", &FREE},
            
            {"DEFAULT_CHAR_WIDTH", &DEFAULT_CHAR_WIDTH},
            {"UNAVAILABLE_SELECTION", &UNAVAILABLE_SELECTION},

            {"ON", &ON},
            {"OFF", &OFF},

            {"OK", &OK},
            {"BACK", &BACK},
            {"HIDE", &HIDE},
            {"CANCEL", &CANCEL},

            {"GAP_1", &GAP_1},
            {"GAP_2", &GAP_2},

            //{"EMPTY", &EMPTY},

            #if USING_WIDGET_DIRECTIVE
            {"SUNDAY", &SUNDAY},
            {"MONDAY", &MONDAY},
            {"TUESDAY", &TUESDAY},
            {"WEDNESDAY", &WEDNESDAY},
            {"THURSDAY", &THURSDAY},
            {"FRIDAY", &FRIDAY},
            {"SATURDAY", &SATURDAY},
            {"JANUARY", &JANUARY},
            {"FEBRUARY", &FEBRUARY},
            {"MARCH", &MARCH},
            {"APRIL", &APRIL},
            {"MAY", &MAY},
            {"JUNE", &JUNE},
            {"JULY", &JULY},
            {"AUGUST", &AUGUST},
            {"SEPTEMBER", &SEPTEMBER},
            {"OCTOBER", &OCTOBER},
            {"NOVEMBER", &NOVEMBER},
            {"DECEMBER", &DECEMBER},
            {"SUN", &SUN},
            {"MON", &MON},
            {"TUE", &TUE},
            {"WED", &WED},
            {"THU", &THU},
            {"FRI", &FRI},
            {"SAT", &SAT},
            {"JAN", &JAN},
            {"FEB", &FEB},
            {"MAR", &MAR},
            {"APR", &APR},
            {"MAY_ABBR", &MAY_ABBR},
            {"JUN", &JUN},
            {"JUL", &JUL},
            {"AUG", &AUG},
            {"SEP", &SEP},
            {"OCT", &OCT},
            {"NOV", &NOV},
            {"DEC", &DEC}
            #endif
        };
        
        // Iterate over the map to update global variables
        for (auto& kv : configMap) {
            auto it = jsonMap.find(kv.first);
            if (it != jsonMap.end()) {
                updateIfNotEmpty(*kv.second, it->second);
            }
        }
    }
    
    
    // Helper function to apply replacements
    //void applyTimeStrReplacements(std::string& str, const std::unordered_map<std::string, std::string>& mappings) {
    //    size_t pos;
    //    for (const auto& mapping : mappings) {
    //        pos = str.find(mapping.first);
    //        while (pos != std::string::npos) {
    //            str.replace(pos, mapping.first.length(), mapping.second);
    //            pos = str.find(mapping.first, pos + mapping.second.length());
    //        }
    //    }
    //}
    
    #if USING_WIDGET_DIRECTIVE
    void localizeTimeStr(char* timeStr) {
        // Define static unordered_map for day and month mappings
        static std::unordered_map<std::string, std::string*> mappings = {
            {"Sun", &SUN},
            {"Mon", &MON},
            {"Tue", &TUE},
            {"Wed", &WED},
            {"Thu", &THU},
            {"Fri", &FRI},
            {"Sat", &SAT},
            {"Sunday", &SUNDAY},
            {"Monday", &MONDAY},
            {"Tuesday", &TUESDAY},
            {"Wednesday", &WEDNESDAY},
            {"Thursday", &THURSDAY},
            {"Friday", &FRIDAY},
            {"Saturday", &SATURDAY},
            {"Jan", &JAN},
            {"Feb", &FEB},
            {"Mar", &MAR},
            {"Apr", &APR},
            {"May", &MAY_ABBR},
            {"Jun", &JUN},
            {"Jul", &JUL},
            {"Aug", &AUG},
            {"Sep", &SEP},
            {"Oct", &OCT},
            {"Nov", &NOV},
            {"Dec", &DEC},
            {"January", &JANUARY},
            {"February", &FEBRUARY},
            {"March", &MARCH},
            {"April", &APRIL},
            {"May", &MAY},
            {"June", &JUNE},
            {"July", &JULY},
            {"August", &AUGUST},
            {"September", &SEPTEMBER},
            {"October", &OCTOBER},
            {"November", &NOVEMBER},
            {"December", &DECEMBER}
        };
        
        std::string timeStrCopy = timeStr; // Convert the char array to a string for processing
        
        // Apply day and month replacements
        size_t pos;
        for (const auto& mapping : mappings) {
            pos = timeStrCopy.find(mapping.first);
            while (pos != std::string::npos) {
                timeStrCopy.replace(pos, mapping.first.length(), *(mapping.second));
                pos = timeStrCopy.find(mapping.first, pos + mapping.second->length());
            }
        }
        
        // Copy the modified string back to the character array
        strcpy(timeStr, timeStrCopy.c_str());
    }
    #endif

    // Unified function to apply replacements
    void applyLangReplacements(std::string& text, bool isValue) {
        if (isValue) {
            // Direct comparison for value replacements
            if (text.length() == 2) {
                if (text[0] == 'O') {
                    if (text[1] == 'n') {
                        text = ON;
                        return;
                    } else if (text[1] == 'f' && text == "Off") {
                        text = OFF;
                        return;
                    }
                }
            }
        }
        #if IS_LAUNCHER_DIRECTIVE
        else {
            // Direct comparison for launcher replacements
            switch (text.length()) {
                case 5:
                    if (text == "LAKKA") {
                        text = LAKKA;
                    }
                    break;
                case 6:
                    if (text == "Reboot") {
                        text = REBOOT;
                    }
                    else if (text == "UBUNTU") {
                        text = UBUNTU;
                    }
                    break;
                case 7:
                    if (text == "CFWBOOT") {
                        text = CFWBOOT;
                    }
                    else if (text == "OFWBOOT") {
                        text = OFWBOOT;
                    }
                    else if (text == "ANDROID") {
                        text = ANDROID;
                    }
                    break;
                case 8:
                    if (text == "Shutdown") {
                        text = SHUTDOWN;
                    }
                    break;
                case 9:
                    if (text == "Reboot To") {
                        text = REBOOT_TO;
                    }
                    break;
                case 10:
                    if (text == "Boot Entry") {
                        text = BOOT_ENTRY;
                    }
                    break;
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
    
    
    void loadWallpaperFile(const std::string& filePath, s32 width, s32 height) {
        const size_t originalDataSize = width * height * 4;
        const size_t compressedDataSize = originalDataSize / 2;
        
        wallpaperData.resize(compressedDataSize);
        
        if (!isFileOrDirectory(filePath)) {
            wallpaperData.clear();
            return;
        }
        
        FILE* file = fopen(filePath.c_str(), "rb");
        if (!file) {
            wallpaperData.clear();
            return;
        }

        setvbuf(file, nullptr, _IOFBF, 256 * 1024);
        
        constexpr size_t chunkBytes = 128 * 1024;
        uint8_t chunkBuffer[chunkBytes];
        
        size_t totalRead = 0;
        uint8_t* dst = wallpaperData.data();
        const uint8x8_t mask = vdup_n_u8(0xF0);
        
        while (totalRead < originalDataSize) {
            const size_t remaining = originalDataSize - totalRead;
            const size_t toRead = remaining < chunkBytes ? remaining : chunkBytes;
            
            const size_t bytesRead = fread(chunkBuffer, 1, toRead, file);
            if (bytesRead == 0) {
                fclose(file);
                wallpaperData.clear();
                return;
            }
            
            const uint8_t* src = chunkBuffer;
            size_t i = 0;
            
            // NEON: Process 16 bytes -> 8 bytes
            for (; i + 16 <= bytesRead; i += 16) {
                uint8x16_t data = vld1q_u8(src + i);
                uint8x8x2_t sep = vuzp_u8(vget_low_u8(data), vget_high_u8(data));
                vst1_u8(dst, vorr_u8(vand_u8(sep.val[0], mask), vshr_n_u8(sep.val[1], 4)));
                dst += 8;
            }
            
            // Scalar fallback
            for (; i + 1 < bytesRead; i += 2) {
                *dst++ = (src[i] & 0xF0) | (src[i + 1] >> 4);
            }
            
            totalRead += bytesRead;
        }
        
        fclose(file);
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
