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
 *  Copyright (c) 2024 ppkantorski
 ********************************************************************************/

#include <tsl_utils.hpp>

namespace ult {
    
    std::unordered_map<std::string, std::string> translationCache;
    

    // Helper function to read file content into a string
    bool readFileContent(const std::string& filePath, std::string& content) {
        #if NO_FSTREAM_DIRECTIVE
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
            result[key] = value;
    
            pos = valueEnd + 1; // Move to the next key-value pair
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
        Result rc;
        ApmPerformanceMode perfMode = ApmPerformanceMode_Invalid;
    
        // Initialize the APM service
        rc = apmInitialize();
        if (R_FAILED(rc)) {
            return false;  // Fail early if initialization fails
        }
    
        // Get the current performance mode
        rc = apmGetPerformanceMode(&perfMode);
        apmExit();  // Clean up the APM service
    
        if (R_FAILED(rc)) {
            return false;  // Fail early if performance mode check fails
        }
    
        // Check if the performance mode indicates docked state
        if (perfMode == ApmPerformanceMode_Boost) {
            return true;  // System is docked (boost mode active)
        }
    
        return false;  // Not docked (normal mode or handheld)
    }
    
    std::string getTitleIdAsString() {
        Result rc;
        u64 pid = 0;
        u64 tid = 0;
    
        // The Process Management service is initialized before (as per your setup)
        // Get the current application process ID
        rc = pmdmntGetApplicationProcessId(&pid);
        if (R_FAILED(rc)) {
            return NULL_STR;
        }
    
        rc = pminfoInitialize();
        if (R_FAILED(rc)) {
            return NULL_STR;
        }
    
        // Use pminfoGetProgramId to retrieve the Title ID (Program ID)
        rc = pminfoGetProgramId(&tid, pid);
        if (R_FAILED(rc)) {
            pminfoExit();
            return NULL_STR;
        }
        pminfoExit();
    
        // Convert the Title ID to a string and return it
        char titleIdStr[17];  // 16 characters for the Title ID + null terminator
        snprintf(titleIdStr, sizeof(titleIdStr), "%016lX", tid);
        return std::string(titleIdStr);
    }
    
    //bool isLauncher = false;



    bool internalTouchReleased = true;
    u32 layerEdge = 0;
    bool useRightAlignment = false;
    bool useSwipeToOpen = false;
    bool noClickableItems = false;
    
    
    // Define the duration boundaries (for smooth scrolling)
    const std::chrono::milliseconds initialInterval = std::chrono::milliseconds(67);  // Example initial interval
    const std::chrono::milliseconds shortInterval = std::chrono::milliseconds(10);    // Short interval after long hold
    const std::chrono::milliseconds transitionPoint = std::chrono::milliseconds(2000); // Point at which the shortest interval is reached
    
    // Function to interpolate between two durations
    std::chrono::milliseconds interpolateDuration(std::chrono::milliseconds start, std::chrono::milliseconds end, float t) {
        using namespace std::chrono;
        auto interpolated = start.count() + static_cast<long long>((end.count() - start.count()) * t);
        return milliseconds(interpolated);
    }
    
    
    
    //#include <filesystem> // Comment out filesystem
    
    // CUSTOM SECTION START
    float backWidth, selectWidth, nextPageWidth;
    bool inMainMenu = false;
    bool inOverlaysPage = false;
    bool inPackagesPage = false;
    
    bool firstBoot = true; // for detecting first boot
    
    //std::unordered_map<std::string, std::string> hexSumCache;
    
    // Define an atomic bool for interpreter completion
    std::atomic<bool> threadFailure(false);
    std::atomic<bool> runningInterpreter(false);
    std::atomic<bool> shakingProgress(true);
    
    std::atomic<bool> isHidden(false);
    
    //bool progressAnimation = false;
    bool disableTransparency = false;
    //bool useCustomWallpaper = false;
    bool useMemoryExpansion = false;
    bool useOpaqueScreenshots = false;
    
    bool onTrackBar = false;
    bool allowSlide = false;
    bool unlockedSlide = false;
    
    
    
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
        { HidNpadButton_StickL, "LS", "\uE08A" }, { HidNpadButton_StickR, "RS", "\uE08B" },
        { HidNpadButton_Minus, "MINUS", "\uE0B6" }, { HidNpadButton_Plus, "PLUS", "\uE0B5" }
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
    
        std::string unicodeCombo;
        bool modified = false;
        size_t start = 0;
        size_t length = combo.length();
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
    
    
    const std::string whiteColor = "#FFFFFF";
    const std::string blackColor = "#000000";
    
    #if IS_LAUNCHER_DIRECTIVE
    std::string ENGLISH = "English";
    std::string SPANISH = "Español";
    std::string FRENCH = "Français";
    std::string GERMAN = "Deutsch";
    std::string JAPANESE = "日本語";
    std::string KOREAN = "한국어";
    std::string ITALIAN = "Italiano";
    std::string DUTCH = "Dutch";
    std::string PORTUGUESE = "Português";
    std::string RUSSIAN = "Русский";
    std::string POLISH = "polski";
    std::string SIMPLIFIED_CHINESE = "中文 (簡体)";
    std::string TRADITIONAL_CHINESE = "中文 (繁体)";
    std::string OVERLAYS = "오버레이"; //defined in libTesla now
    std::string OVERLAY = "오버레이";
    std::string HIDDEN_OVERLAYS = "숨겨진 오버레이";
    std::string PACKAGES = "패키지"; //defined in libTesla now
    std::string PACKAGE = "패키지";
    std::string HIDDEN_PACKAGES = "숨겨진 패키지";
    std::string HIDDEN = "숨김";
    std::string HIDE_OVERLAY = "오버레이 숨김";
    std::string HIDE_PACKAGE = "패키지 숨김";
    std::string LAUNCH_ARGUMENTS = "인수 실행";
    std::string BOOT_COMMANDS = "부팅 커맨드";
    std::string EXIT_COMMANDS = "종료 커맨드";
    std::string ERROR_LOGGING = "오류 로깅";
    std::string COMMANDS = "커맨드";
    std::string SETTINGS = "설정";
    std::string MAIN_SETTINGS = "메인 설정";
    std::string UI_SETTINGS = "UI 변경";

    std::string WIDGET = "위젯";
    std::string CLOCK = "시각";
    std::string BATTERY = "배터리";
    std::string SOC_TEMPERATURE = "SoC 온도";
    std::string PCB_TEMPERATURE = "PCB 온도";
    std::string MISCELLANEOUS = "기타";
    std::string MENU_ITEMS = "메뉴 아이템";
    std::string USER_GUIDE = "사용 설명서";
    std::string VERSION_LABELS = "버전 표시";
    std::string KEY_COMBO = "키 조합";
    std::string LANGUAGE = "언어";
    std::string OVERLAY_INFO = "오버레이 정보";
    std::string SOFTWARE_UPDATE = "소프트웨어 업데이트";
    std::string UPDATE_ULTRAHAND = "Ultrahand 업데이트";
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
    std::string NOTICE = "안내";
    std::string UTILIZES = "활용";
    std::string FREE = "여유";
    std::string MEMORY_EXPANSION = "확장";
    std::string REBOOT_REQUIRED = "*적용을 위해, 재부팅 필요";
    std::string LOCAL_IP = "로컬 IP";
    std::string WALLPAPER = "배경";
    std::string THEME = "테마";
    std::string DEFAULT = "기본";
    std::string ROOT_PACKAGE = "기본 패키지";
    std::string SORT_PRIORITY = "우선순위 정렬";
    std::string FAILED_TO_OPEN = "파일 열기 실패";
    std::string CLEAN_VERSIONS = "정리된 버전";
    std::string OVERLAY_VERSIONS = "오버레이 버전";
    std::string PACKAGE_VERSIONS = "패키지 버전";
    std::string OPAQUE_SCREENSHOTS = "스크린샷 배경";

    std::string PACKAGE_INFO = "패키지 정보";
    std::string _TITLE = "이름";
    std::string _VERSION= "버전";
    std::string _CREATOR = "개발자";
    std::string _ABOUT = "설명";
    std::string _CREDITS = "기여자";

    std::string USERGUIDE_OFFSET = "175";
    std::string SETTINGS_MENU = "설정 메뉴";
    std::string SCRIPT_OVERLAY = "오버레이 스크립트";
    std::string STAR_FAVORITE = "색인/즐겨찾기";
    std::string APP_SETTINGS = "앱 설정";
    std::string ON_MAIN_MENU = "메인 메뉴";
    std::string ON_A_COMMAND = "커맨드 위에서";
    std::string ON_OVERLAY_PACKAGE = "오버레이/패키지 위에서";
    std::string EFFECTS = "이펙트";
    std::string SWIPE_TO_OPEN = "밀어서 열기";
    std::string RIGHT_SIDE_MODE = "우측 배치";
    std::string PROGRESS_ANIMATION = "애니메이션";

    std::string REBOOT_TO = "로 재부팅";
    std::string REBOOT = "재부팅";
    std::string SHUTDOWN = "전원 종료";
    std::string BOOT_ENTRY = "런처";

    /* ASAP Packages */
    std::string SEC_INFO = "안내문";
    std::string SEC_WANN = "경고문";
    std::string SEC_IFTX = "안내:";
    std::string SEC_WNTX = "주의:";
    std::string SEC_NULL = "";
    std::string SEC_LINE = "¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯  ㅤㅤㅤㅤㅤㅤ";

    /* Mainmenu */
    std::string SEC_TXT1 = "시스템 확인 및 울트라핸드의 설정을 변경";
    std::string SYS_MMRY = "시스템 메모리";
    std::string SEC_HDR1 = "Ultrahand+";
    std::string SEC_HDR2 = "Tester+";
    std::string CFG_MISC = "추가 설정";
    std::string UPT_MENU = "업데이트";
    std::string UPT_ASAP = "ASAP-Tester";
    std::string UPT_TXT1 = "다음 구성 요소를 전부 업데이트합니다";
    std::string UPT_TXT2 = "ASAP을 테스터 버전으로 교체합니다";
    std::string UPT_TXT3 = "테스터 팩에선 오류가 발생할 수 있습니다";
    std::string UPT_NTC1 = "커스텀 패키지 테슬라 메뉴";
    std::string UPT_NTC2 = "Ultrahand, ovlloader+";
    std::string UPT_NTC3 = "Extra Setting+, System Clock+, OC Toolkit";
    std::string UPT_NTC4 = "MissonControl, SaltyNX, sys-con";
    std::string UPT_NTC5 = "EdiZon, emuiibo, FPSLocker, ReverseNX-RT";
    std::string UPT_NTC6 = "Status-Monitor, sys-clk-oc";
    std::string UPT_NTC7 = "JKSV, Linkalho, sys-clk-manager";
    std::string UPT_NTC8 = "다운로드 이후 자동으로 재부팅합니다";
    std::string UTH_INFO = "Ultrahand Overlay 정보";
    std::string DF_THEME = "BLACK - Basic White";

    /* Current Package - Launcher+ */
    std::string LPLS_CFW = "에뮤/시스낸드 (커펌)";
    std::string LPLS_CFE = "커펌 (eMMC/SD Card)";
    std::string LPLS_OFW = "시스낸드 (정펌)";
    std::string LPLS_OFE = "정펌 (eMMC)";
    std::string HKT_HOME = "메인메뉴";
    std::string L4T_ANDE = "안드로이드";
    std::string L4T_EMUE = "에뮬레이터";
    std::string L4T_UBTE = "리눅스";
    std::string LPLS_IF1 = "닌텐도 스위치";
    std::string LPLS_IF2 = "전원을 종료합니다";
    std::string LPLS_IF3 = "무선 컨트롤러";
    std::string LPLS_IF4 = "연결을 해제합니다";
    std::string LPLS_IF5 = "일부 대응하지 않음";

    /* USER GUIDE */
    std::string EXST_USG = "Extra Setting+";
    std::string SCLK_USG = "System Clock+";
    std::string OCTK_USG = "OC Toolkit";
    std::string LPLS_USG = "Launcher+";
    std::string STAR_USG = "설정│즐겨찾기";
    std::string USG_TXT1 = "업데이트 사후 설정";
    std::string USG_TXT2 = "오버클럭 앱 · 도구 전환";
    std::string USG_TXT3 = "Loader.kip 값 편집 도구";
    std::string USG_TXT4 = "빠른 재부팅 · UMS · 전원";
    std::string USG_TXT5 = "버튼 입력";

    /* ETC */
    std::string OVERRIDE_SELECTION = "설정 안 함";
    std::string AUTO_SELECTION = "자동";
    std::string LANG_FILE = "";
    #endif


    std::string DEFAULT_CHAR_WIDTH = "0.33";
    std::string UNAVAILABLE_SELECTION = "설정 없음";


    std::string ON = "\uE14B";
    std::string OFF = "\uE14C";

    std::string OK = "확인";
    std::string BACK = "뒤로";

    std::string GAP_1 = "     ";
    std::string GAP_2 = "  ";
    

    std::string EMPTY = "비어있음";
    
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
        ENGLISH = "English";
        SPANISH = "Español";
        FRENCH = "Français";
        GERMAN = "Deutsch";
        JAPANESE = "日本語";
        KOREAN = "한국어";
        ITALIAN = "Italiano";
        DUTCH = "Dutch";
        PORTUGUESE = "Português";
        RUSSIAN = "Русский";
        POLISH = "polski";
        SIMPLIFIED_CHINESE = "中文 (簡体)";
        TRADITIONAL_CHINESE = "中文 (繁体)";
        DEFAULT_CHAR_WIDTH = "0.33";
        UNAVAILABLE_SELECTION = "설정 없음";
        OVERLAYS = "오버레이"; //defined in libTesla now
        OVERLAY = "오버레이";
        HIDDEN_OVERLAYS = "숨겨진 오버레이";
        PACKAGES = "패키지"; //defined in libTesla now
        PACKAGE = "패키지";
        HIDDEN_PACKAGES = "숨겨진 패키지";
        HIDDEN = "숨김";
        HIDE_OVERLAY = "오버레이 숨김";
        HIDE_PACKAGE = "패키지 숨김";
        LAUNCH_ARGUMENTS = "인수 실행";
        BOOT_COMMANDS = "부팅 커맨드";
        EXIT_COMMANDS = "종료 커맨드";
        ERROR_LOGGING = "오류 로깅";
        COMMANDS = "커맨드";
        SETTINGS = "설정";
        MAIN_SETTINGS = "메인 설정";
        UI_SETTINGS = "UI 변경";
        WIDGET = "위젯";
        CLOCK = "시각";
        BATTERY = "배터리";
        SOC_TEMPERATURE = "SoC 온도";
        PCB_TEMPERATURE = "PCB 온도";
        MISCELLANEOUS = "기타";
        MENU_ITEMS = "메뉴 아이템";
        USER_GUIDE = "사용 설명서";
        VERSION_LABELS = "버전 표시";
        KEY_COMBO = "키 조합";
        LANGUAGE = "언어";
        OVERLAY_INFO = "오버레이 정보";
        SOFTWARE_UPDATE = "소프트웨어 업데이트";
        UPDATE_ULTRAHAND = "Ultrahand 업데이트";
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
        NOTICE = "안내";
        UTILIZES = "활용";
        FREE = "여유";
        MEMORY_EXPANSION = "확장";
        REBOOT_REQUIRED = "*적용을 위해, 재부팅 필요";
        LOCAL_IP = "로컬 IP";
        WALLPAPER = "배경";
        THEME = "테마";
        DEFAULT = "기본";
        ROOT_PACKAGE = "기본 패키지";
        SORT_PRIORITY = "우선순위 정렬";
        FAILED_TO_OPEN = "파일 열기 실패";
        CLEAN_VERSIONS = "정리된 버전";
        OVERLAY_VERSIONS = "오버레이 버전";
        PACKAGE_VERSIONS = "패키지 버전";
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
        REBOOT_TO = "로 재부팅";
        REBOOT = "재부팅";
        SHUTDOWN = "전원 종료";
        BOOT_ENTRY = "런처";
        GAP_1 = "     ";
        GAP_2 = "  ";

        USERGUIDE_OFFSET = "175";
        SETTINGS_MENU = "설정 메뉴";
        SCRIPT_OVERLAY = "오버레이 스크립트";
        STAR_FAVORITE = "색인/즐겨찾기";
        APP_SETTINGS = "앱 설정";
        ON_MAIN_MENU = "메인 메뉴";
        ON_A_COMMAND = "커맨드 위에서";
        ON_OVERLAY_PACKAGE = "오버레이/패키지 위에서";
        EFFECTS = "이펙트";
        SWIPE_TO_OPEN = "밀어서 열기";
        RIGHT_SIDE_MODE = "우측 배치";
        PROGRESS_ANIMATION = "애니메이션";
        EMPTY = "비어 있음";
    
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

        /* ASAP Packages */
        SEC_INFO = "안내문";
        SEC_WANN = "경고문";
        SEC_IFTX = "안내:";
        SEC_WNTX = "주의:";
        SEC_NULL = "";
        SEC_LINE = "¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯  ㅤㅤㅤㅤㅤㅤ";

        /* Mainmenu */
        SEC_TXT1 = "시스템 확인 및 울트라핸드의 설정을 변경";
        SYS_MMRY = "시스템 메모리";
        SEC_HDR1 = "Ultrahand+";
        SEC_HDR2 = "Tester+";
        CFG_MISC = "추가 설정";
        UPT_MENU = "업데이트";
        UPT_ASAP = "ASAP-Tester";
        UPT_TXT1 = "다음 구성 요소를 전부 업데이트합니다";
        UPT_TXT2 = "ASAP을 테스터 버전으로 교체합니다";
        UPT_TXT3 = "테스터 팩에선 오류가 발생할 수 있습니다";
        UPT_NTC1 = "커스텀 패키지 테슬라 메뉴";
        UPT_NTC2 = "Ultrahand, ovlloader+";
        UPT_NTC3 = "Extra Setting+, System Clock+, OC Toolkit";
        UPT_NTC4 = "MissonControl, SaltyNX, sys-con";
        UPT_NTC5 = "EdiZon, emuiibo, FPSLocker, ReverseNX-RT";
        UPT_NTC6 = "Status-Monitor, sys-clk-oc";
        UPT_NTC7 = "JKSV, Linkalho, sys-clk-manager";
        UPT_NTC8 = "다운로드 이후 자동으로 재부팅합니다";
        UTH_INFO = "Ultrahand Overlay 정보";
        DF_THEME = "BLACK - Basic White";

        /* Current Package - Launcher+ */
        LPLS_CFW = "에뮤/시스낸드 (커펌)";
        LPLS_CFE = "커펌 (eMMC/SD Card)";
        LPLS_OFW = "시스낸드 (정펌)";
        LPLS_OFE = "정펌 (eMMC)";
        HKT_HOME = "메인메뉴";
        L4T_ANDE = "안드로이드";
        L4T_EMUE = "에뮬레이터";
        L4T_UBTE = "리눅스";
        LPLS_IF1 = "닌텐도 스위치";
        LPLS_IF2 = "전원을 종료합니다";
        LPLS_IF3 = "무선 컨트롤러";
        LPLS_IF4 = "연결을 해제합니다";
        LPLS_IF5 = "일부 대응하지 않음";

        /* USER GUIDE */
        EXST_USG = "Extra Setting+";
        SCLK_USG = "System Clock+";
        OCTK_USG = "OC Toolkit";
        LPLS_USG = "Launcher+";
        STAR_USG = "설정│즐겨찾기";
        USG_TXT1 = "업데이트 사후 설정";
        USG_TXT2 = "오버클럭 앱 · 도구 전환";
        USG_TXT3 = "Loader.kip 값 편집 도구";
        USG_TXT4 = "빠른 재부팅 · UMS · 전원";
        USG_TXT5 = "버튼 입력";

        /* ETC */
        OVERRIDE_SELECTION = "설정 안 함";
        AUTO_SELECTION = "자동";
        LANG_FILE = "";
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

        
        static std::unordered_map<std::string, std::string*> configMap = {
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
            {"SIMPLIFIED_CHINESE", &SIMPLIFIED_CHINESE},
            {"TRADITIONAL_CHINESE", &TRADITIONAL_CHINESE},
            {"OVERLAYS", &OVERLAYS},
            {"OVERLAY", &OVERLAY},
            {"HIDDEN_OVERLAYS", &HIDDEN_OVERLAYS},
            {"PACKAGES", &PACKAGES},
            {"PACKAGE", &PACKAGE},
            {"HIDDEN_PACKAGES", &HIDDEN_PACKAGES},
            {"HIDDEN", &HIDDEN},
            {"HIDE_PACKAGE", &HIDE_PACKAGE},
            {"HIDE_OVERLAY", &HIDE_OVERLAY},
            {"LAUNCH_ARGUMENTS", &LAUNCH_ARGUMENTS},
            {"BOOT_COMMANDS", &BOOT_COMMANDS},
            {"EXIT_COMMANDS", &EXIT_COMMANDS},
            {"ERROR_LOGGING", &ERROR_LOGGING},
            {"COMMANDS", &COMMANDS},
            {"SETTINGS", &SETTINGS},
            {"MAIN_SETTINGS", &MAIN_SETTINGS},
            {"UI_SETTINGS", &UI_SETTINGS},

            {"WIDGET", &WIDGET},
            {"CLOCK", &CLOCK},
            {"BATTERY", &BATTERY},
            {"SOC_TEMPERATURE", &SOC_TEMPERATURE},
            {"PCB_TEMPERATURE", &PCB_TEMPERATURE},
            {"MISCELLANEOUS", &MISCELLANEOUS},
            {"MENU_ITEMS", &MENU_ITEMS},
            {"USER_GUIDE", &USER_GUIDE},
            {"VERSION_LABELS", &VERSION_LABELS},
            {"KEY_COMBO", &KEY_COMBO},
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
            {"NOTICE", &NOTICE},
            {"UTILIZES", &UTILIZES},
            {"FREE", &FREE},
            {"MEMORY_EXPANSION", &MEMORY_EXPANSION},
            {"REBOOT_REQUIRED", &REBOOT_REQUIRED},
            {"LOCAL_IP", &LOCAL_IP},
            {"WALLPAPER", &WALLPAPER},
            {"THEME", &THEME},
            {"DEFAULT", &DEFAULT},
            {"ROOT_PACKAGE", &ROOT_PACKAGE},
            {"SORT_PRIORITY", &SORT_PRIORITY},
            {"FAILED_TO_OPEN", &FAILED_TO_OPEN},
            {"CLEAN_VERSIONS", &CLEAN_VERSIONS},
            {"OVERLAY_VERSIONS", &OVERLAY_VERSIONS},
            {"PACKAGE_VERSIONS", &PACKAGE_VERSIONS},
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
            {"EFFECTS", &EFFECTS},
            {"SWIPE_TO_OPEN", &SWIPE_TO_OPEN},
            {"RIGHT_SIDE_MODE", &RIGHT_SIDE_MODE},
            {"PROGRESS_ANIMATION", &PROGRESS_ANIMATION},

            {"REBOOT_TO", &REBOOT_TO},
            {"REBOOT", &REBOOT},
            {"SHUTDOWN", &SHUTDOWN},
            {"BOOT_ENTRY", &BOOT_ENTRY},

            /* ASAP Packages */
            {"SEC_INFO", &SEC_INFO},
            {"SEC_WANN", &SEC_WANN},
            {"SEC_IFTX", &SEC_IFTX},
            {"SEC_WNTX", &SEC_WNTX},
            {"SEC_NULL", &SEC_NULL},
            {"SEC_LINE", &SEC_LINE},

            /* Mainmenu */
            {"SEC_TXT1", &SEC_TXT1},
            {"SYS_MMRY", &SYS_MMRY},
            {"SEC_HDR1", &SEC_HDR1},
            {"SEC_HDR2", &SEC_HDR2},
            {"CFG_MISC", &CFG_MISC},
            {"UPT_MENU", &UPT_MENU},
            {"UPT_ASAP", &UPT_ASAP},
            {"UPT_TXT1", &UPT_TXT1},
            {"UPT_TXT2", &UPT_TXT2},
            {"UPT_TXT3", &UPT_TXT3},
            {"UPT_NTC1", &UPT_NTC1},
            {"UPT_NTC2", &UPT_NTC2},
            {"UPT_NTC3", &UPT_NTC3},
            {"UPT_NTC4", &UPT_NTC4},
            {"UPT_NTC5", &UPT_NTC5},
            {"UPT_NTC6", &UPT_NTC6},
            {"UPT_NTC7", &UPT_NTC7},
            {"UPT_NTC8", &UPT_NTC8},
            {"UTH_INFO", &UTH_INFO},
            {"DF_THEME", &DF_THEME},

            /* Current Package - Launcher+ */
            {"LPLS_CFW", &LPLS_CFW},
            {"LPLS_CFE", &LPLS_CFE},
            {"LPLS_OFW", &LPLS_OFW},
            {"LPLS_OFE", &LPLS_OFE},
            {"HKT_HOME", &HKT_HOME},
            {"L4T_ANDE", &L4T_ANDE},
            {"L4T_EMUE", &L4T_EMUE},
            {"L4T_UBTE", &L4T_UBTE},
            {"LPLS_IF1", &LPLS_IF1},
            {"LPLS_IF2", &LPLS_IF2},
            {"LPLS_IF3", &LPLS_IF3},
            {"LPLS_IF4", &LPLS_IF4},
            {"LPLS_IF5", &LPLS_IF5},

            /* USER GUIDE */
            {"EXST_USG", &EXST_USG},
            {"SCLK_USG", &SCLK_USG},
            {"OCTK_USG", &OCTK_USG},
            {"LPLS_USG", &LPLS_USG},
            {"STAR_USG", &STAR_USG},
            {"USG_TXT1", &USG_TXT1},
            {"USG_TXT2", &USG_TXT2},
            {"USG_TXT3", &USG_TXT3},
            {"USG_TXT4", &USG_TXT4},
            {"USG_TXT5", &USG_TXT5},

            /* ETC */
            {"OVERRIDE_SELECTION", &OVERRIDE_SELECTION},
            {"AUTO_SELECTION", &AUTO_SELECTION},
            {"LANG_FILE", &LANG_FILE},
            #endif


            {"DEFAULT_CHAR_WIDTH", &DEFAULT_CHAR_WIDTH},
            {"UNAVAILABLE_SELECTION", &UNAVAILABLE_SELECTION},

            {"ON", &ON},
            {"OFF", &OFF},

            {"OK", &OK},
            {"BACK", &BACK},

            {"GAP_1", &GAP_1},
            {"GAP_2", &GAP_2},

            {"EMPTY", &EMPTY},

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
        // Static maps for replacements
        #if IS_LAUNCHER_DIRECTIVE
        static const std::unordered_map<std::string, std::string*> launcherReplacements = {
            {"Reboot To", &REBOOT_TO},
            {"Boot Entry", &BOOT_ENTRY},
            {"Reboot", &REBOOT},
            {"Shutdown", &SHUTDOWN}
        };
        #endif
    
        static const std::unordered_map<std::string, std::string*> valueReplacements = {
            {"On", &ON},
            {"Off", &OFF}
        };
    
        // Determine which map to use
        const std::unordered_map<std::string, std::string*>* replacements = nullptr;
    
        if (!isValue) {
            #if IS_LAUNCHER_DIRECTIVE
            replacements = &launcherReplacements;
            #else
            return;
            #endif
        } else {
            replacements = &valueReplacements;
        }
    
        // Perform the direct replacement
        if (replacements) {
            auto it = replacements->find(text);
            if (it != replacements->end()) {
                text = *(it->second);
            }
        }
    }
    
    
    
    // Predefined hexMap
    const std::array<int, 256> hexMap = [] {
        std::array<int, 256> map = {0};
        map['0'] = 0; map['1'] = 1; map['2'] = 2; map['3'] = 3; map['4'] = 4;
        map['5'] = 5; map['6'] = 6; map['7'] = 7; map['8'] = 8; map['9'] = 9;
        map['A'] = 10; map['B'] = 11; map['C'] = 12; map['D'] = 13; map['E'] = 14; map['F'] = 15;
        map['a'] = 10; map['b'] = 11; map['c'] = 12; map['d'] = 13; map['e'] = 14; map['f'] = 15;
        return map;
    }();
    
    
    // Prepare a map of default settings
    std::map<std::string, std::string> defaultThemeSettingsMap = {
        {"default_overlay_color", whiteColor},
        {"default_package_color", "#00FF00"},
        {"clock_color", whiteColor},
        {"bg_alpha", "13"},
        {"bg_color", blackColor},
        {"separator_alpha", "15"},
        {"separator_color", "#404040"},
        {"battery_color", "#3CDD88"},
        {"text_color", whiteColor},
        {"header_text_color", whiteColor},
        {"header_separator_color", whiteColor},
        {"star_color", "#FFAA17"},
        {"selection_star_color", "#FFAA17"},
        {"bottom_button_color", whiteColor},
        {"bottom_text_color", whiteColor},
        {"bottom_separator_color", whiteColor},
        {"table_bg_color", "#3F3F3F"},
        {"table_bg_alpha", "13"},
        {"table_section_text_color", "#5DC5FB"},
        {"table_info_text_color", whiteColor},
        {"warning_text_color", "#F63345"},
        {"trackbar_slider_color", "#606060"},
        {"trackbar_slider_border_color", "#505050"},
        {"trackbar_slider_malleable_color", "#A0A0A0"},
        {"trackbar_full_color", "#00FFDD"},
        {"trackbar_empty_color", "#404040"},
        {"version_text_color", "#AAAAAA"},
        {"on_text_color", "#00FFDD"},
        {"off_text_color", "#AAAAAA"},
        {"invalid_text_color", "#FF0000"},
        {"inprogress_text_color", "#3CDD88"},
        {"selection_text_color", whiteColor},
        {"selection_bg_color", blackColor},
        {"selection_bg_alpha", "13"},
        {"trackbar_color", "#555555"},
        {"highlight_color_1", "#2288CC"},
        {"highlight_color_2", "#88FFFF"},
        {"highlight_color_3", "#3CDD88"},
        {"highlight_color_4", "#2CD2B1"},
        {"click_text_color", whiteColor},
        {"click_alpha", "13"},
        {"click_color", "#2CD2B1"},
        {"progress_alpha", "7"},
        {"progress_color", "#2597F7"},
        {"invert_bg_click_color", FALSE_STR},
        {"disable_selection_bg", FALSE_STR},
        {"disable_colorful_logo", FALSE_STR},
        {"logo_color_1", "#EAEAEA"},
        {"logo_color_2", whiteColor},
        {"dynamic_logo_color_1", "#C9F1FF"},
        {"dynamic_logo_color_2", "#4BCDF9"},
        {"accent_text_color", "#00FFDD"},
        {"sectiontitle_text_color", "#5DC5FB"},
        {"custom1_text_color", "#0593D3"},
        {"custom2_text_color", "#EF6F53"},
        {"custom3_text_color", "#EF5369"}
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
    
    
    
    float calculateAmplitude(float x, float peakDurationFactor) {
        const float phasePeriod = 360.0f * peakDurationFactor;  // One full phase period
    
        // Convert x from radians to degrees and calculate phase within the period
        int phase = static_cast<int>(x * RAD_TO_DEG) % static_cast<int>(phasePeriod);
    
        // Check if the phase is odd using bitwise operation
        if (phase & 1) {
            return 1.0f;  // Flat amplitude (maximum positive)
        } else {
            // Calculate the sinusoidal amplitude for the remaining period
            return (std::cos(x) + 1.0f) / 2.0f;  // Cosine function expects radians
        }
    }
            
    
    
    std::atomic<bool> refreshWallpaper(false);
    std::vector<u8> wallpaperData; 
    std::atomic<bool> inPlot(false);
    
    std::mutex wallpaperMutex;
    std::condition_variable cv;
    
    
    // Function to load the RGBA file into memory and modify wallpaperData directly
    void loadWallpaperFile(const std::string& filePath, s32 width, s32 height) {
        size_t originalDataSize = width * height * 4; // Original size in bytes (4 bytes per pixel)
        size_t compressedDataSize = originalDataSize / 2; // RGBA4444 uses half the space
        
        wallpaperData.resize(compressedDataSize);
    
        if (!isFileOrDirectory(filePath)) {
            wallpaperData.clear();
            return;
        }
    
        #if NO_FSTREAM_DIRECTIVE
            FILE* file = fopen(filePath.c_str(), "rb");
            if (!file) {
                wallpaperData.clear();
                return;
            }
    
            std::vector<uint8_t> buffer(originalDataSize);
            size_t bytesRead = fread(buffer.data(), 1, originalDataSize, file);
            fclose(file);
    
            if (bytesRead != originalDataSize) {
                wallpaperData.clear();
                return;
            }
    
        #else
            std::ifstream file(filePath, std::ios::binary);
            if (!file) {
                wallpaperData.clear();
                return;
            }
    
            std::vector<uint8_t> buffer(originalDataSize);
            file.read(reinterpret_cast<char*>(buffer.data()), originalDataSize);
            if (!file) {
                wallpaperData.clear();
                return;
            }
        #endif
    
        // Compress RGBA8888 to RGBA4444
        uint8_t* input = buffer.data();
        uint8_t* output = wallpaperData.data();
    
        for (size_t i = 0, j = 0; i < originalDataSize; i += 8, j += 4) {
            // Read 2 RGBA pixels (8 bytes)
            uint8_t r1 = input[i] >> 4;
            uint8_t g1 = input[i + 1] >> 4;
            uint8_t b1 = input[i + 2] >> 4;
            uint8_t a1 = input[i + 3] >> 4;
    
            uint8_t r2 = input[i + 4] >> 4;
            uint8_t g2 = input[i + 5] >> 4;
            uint8_t b2 = input[i + 6] >> 4;
            uint8_t a2 = input[i + 7] >> 4;
    
            // Pack them into 4 bytes (2 bytes per pixel)
            output[j] = (r1 << 4) | g1;
            output[j + 1] = (b1 << 4) | a1;
            output[j + 2] = (r2 << 4) | g2;
            output[j + 3] = (b2 << 4) | a2;
        }
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
    
    bool themeIsInitialized = false; // for loading the theme once in OverlayFrame / HeaderOverlayFrame
    
    // Variables for touch commands
    bool touchingBack = false;
    bool touchingSelect = false;
    bool touchingNextPage = false;
    bool touchingMenu = false;
    bool simulatedBack = false;
    bool simulatedBackComplete = true;
    bool simulatedSelect = false;
    bool simulatedSelectComplete = true;
    bool simulatedNextPage = false;
    bool simulatedNextPageComplete = true;
    bool simulatedMenu = false;
    bool simulatedMenuComplete = true;
    bool stillTouching = false;
    bool interruptedTouch = false;
    bool touchInBounds = false;
    
    
    #if USING_WIDGET_DIRECTIVE
    // Battery implementation
    bool powerInitialized = false;
    bool powerCacheInitialized;
    uint32_t powerCacheCharge;
    //float powerConsumption;
    bool powerCacheIsCharging;
    PsmSession powerSession;
    
    // Define variables to store previous battery charge and time
    uint32_t prevBatteryCharge = 0;
    s64 timeOut = 0;
    
    
    uint32_t batteryCharge;
    bool isCharging;
    //bool validPower;
    
    
    
    bool powerGetDetails(uint32_t *batteryCharge, bool *isCharging) {
        static auto last_call = std::chrono::steady_clock::now();
    
        // Ensure power system is initialized
        if (!powerInitialized) {
            return false;
        }
    
        // Get the current time
        auto now = std::chrono::steady_clock::now();
    
        // Check if enough time has elapsed or if cache is not initialized
        bool useCache = (now - last_call <= min_delay) && powerCacheInitialized;
        if (!useCache) {
            PsmChargerType charger = PsmChargerType_Unconnected;
            Result rc = psmGetBatteryChargePercentage(batteryCharge);
            bool hwReadsSucceeded = R_SUCCEEDED(rc);
    
            if (hwReadsSucceeded) {
                rc = psmGetChargerType(&charger);
                hwReadsSucceeded &= R_SUCCEEDED(rc);
                *isCharging = (charger != PsmChargerType_Unconnected);
    
                if (hwReadsSucceeded) {
                    // Update cache
                    powerCacheCharge = *batteryCharge;
                    powerCacheIsCharging = *isCharging;
                    powerCacheInitialized = true;
                    last_call = now; // Update last call time after successful hardware read
                    return true;
                }
            }
    
            // Use cached values if the hardware read fails
            if (powerCacheInitialized) {
                *batteryCharge = powerCacheCharge;
                *isCharging = powerCacheIsCharging;
                return hwReadsSucceeded; // Return false if hardware read failed but cache is valid
            }
    
            // Return false if cache is not initialized and hardware read failed
            return false;
        }
    
        // Use cached values if not enough time has passed
        *batteryCharge = powerCacheCharge;
        *isCharging = powerCacheIsCharging;

        return true; // Return true as cache is used
    }
    
    
    void powerInit(void) {
        uint32_t charge = 0;
        isCharging = 0;
        
        powerCacheInitialized = false;
        powerCacheCharge = 0;
        powerCacheIsCharging = false;
        
        if (!powerInitialized) {
            Result rc = psmInitialize();
            if (R_SUCCEEDED(rc)) {
                rc = psmBindStateChangeEvent(&powerSession, 1, 1, 1);
                
                if (R_FAILED(rc)) psmExit();
                if (R_SUCCEEDED(rc)) {
                    powerInitialized = true;
                    powerGetDetails(&charge, &isCharging);
                    
                    // Initialize prevBatteryCharge here with a non-zero value if needed.
                    prevBatteryCharge = charge;
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
    float PCB_temperature, SOC_temperature;
    
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
    const std::string DEFAULT_DT_FORMAT = "'%T %a'";
    std::string datetimeFormat = "%T %a";
    
    
    // Widget settings
    //std::string hideClock, hideBattery, hidePCBTemp, hideSOCTemp;
    bool hideClock, hideBattery, hidePCBTemp, hideSOCTemp;
    
    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeWidgetVars() {
        
        hideClock = (parseValueFromIniSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME, "hide_clock") != FALSE_STR);
        hideBattery = (parseValueFromIniSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME, "hide_battery") != FALSE_STR);
        hideSOCTemp = (parseValueFromIniSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME, "hide_soc_temp") != FALSE_STR);
        hidePCBTemp = (parseValueFromIniSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME, "hide_pcb_temp") != FALSE_STR);
        
    }
    #endif
    
    bool cleanVersionLabels, hideOverlayVersions, hidePackageVersions;
    
    std::string loaderInfo = envGetLoaderInfo();
    std::string loaderTitle = extractTitle(loaderInfo);
    bool expandedMemory = (loaderTitle == "nx-ovlloader+");
    
    std::string versionLabel;
    
    #if IS_LAUNCHER_DIRECTIVE
    void reinitializeVersionLabels() {
        cleanVersionLabels = (parseValueFromIniSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME, "clean_version_labels") != FALSE_STR);
        hideOverlayVersions = (parseValueFromIniSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME, "hide_overlay_versions") != FALSE_STR);
        hidePackageVersions = (parseValueFromIniSection(ULTRAHAND_CONFIG_INI_PATH, ULTRAHAND_PROJECT_NAME, "hide_package_versions") != FALSE_STR);
        #ifdef APP_VERSION
        versionLabel = std::string(APP_VERSION) + "   (" + loaderTitle + " " + (cleanVersionLabels ? "" : "v") + cleanVersionLabel(loaderInfo) + ")";
        #endif
        //versionLabel = (cleanVersionLabels) ? std::string(APP_VERSION) : (std::string(APP_VERSION) + "   (" + extractTitle(loaderInfo) + " v" + cleanVersionLabel(loaderInfo) + ")");
    }
    #endif
    
    
    // Number of renderer threads to use
    const unsigned numThreads = expandedMemory ? 4 : 0;
    std::vector<std::thread> threads(numThreads);
    s32 bmpChunkSize = (720 + numThreads - 1) / numThreads;
    std::atomic<s32> currentRow;
    
    
}