#include <windows.h>
#include <stdio.h>
#include <string>
#include <d2d1.h>
#include <dwrite.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_TRAY_EXIT 1001
#define ID_TRAY_ABOUT 1007
#define ID_SWITCH_BASE 2000  // Base ID for dynamic layout menu items
#define ID_SWITCH_EN 1002
#define ID_SWITCH_UA 1003
#define ID_SWITCH_RU 1004
#define TIMER_ID 1005
#define LAYOUT_CHECK_TIMER 1006  // New timer for layout checks

struct LayoutColor {
    LANGID langId;
    COLORREF color1;
    COLORREF color2;  // For special cases like Ukrainian flag
    const wchar_t* name;
};

// Preset colors for common languages
const LayoutColor LAYOUT_COLORS[] = {
    {MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),    RGB(178, 34, 52),  0,              L"EN"},  // American Red
    {MAKELANGID(LANG_UKRAINIAN, SUBLANG_DEFAULT),  RGB(0, 87, 183),   RGB(255, 215, 0), L"UA"},  // Blue and Yellow
    {MAKELANGID(LANG_RUSSIAN, SUBLANG_DEFAULT),    RGB(0, 57, 166),   0,              L"RU"},  // Russian Blue
    {MAKELANGID(LANG_CHINESE, SUBLANG_DEFAULT),    RGB(255, 40, 0),   0,              L"ZH"},  // Chinese Red
    {MAKELANGID(LANG_JAPANESE, SUBLANG_DEFAULT),   RGB(188, 0, 45),   0,              L"JA"},  // Japanese Red
    {MAKELANGID(LANG_KOREAN, SUBLANG_DEFAULT),     RGB(0, 71, 160),   0,              L"KO"},  // Korean Blue
    {MAKELANGID(LANG_GERMAN, SUBLANG_DEFAULT),     RGB(0, 0, 0),      0,              L"DE"},  // German Black
    {MAKELANGID(LANG_FRENCH, SUBLANG_DEFAULT),     RGB(0, 35, 149),   0,              L"FR"},  // French Blue
    {MAKELANGID(LANG_SPANISH, SUBLANG_DEFAULT),    RGB(170, 21, 27),  0,              L"ES"},  // Spanish Red
    {MAKELANGID(LANG_ITALIAN, SUBLANG_DEFAULT),    RGB(0, 146, 70),   0,              L"IT"},  // Italian Green
    {MAKELANGID(LANG_PORTUGUESE, SUBLANG_DEFAULT), RGB(0, 102, 0),    0,              L"PT"},  // Portuguese Green
    {MAKELANGID(LANG_ARABIC, SUBLANG_DEFAULT),     RGB(0, 122, 61),   0,              L"AR"},  // Arabic Green
    {MAKELANGID(LANG_HEBREW, SUBLANG_DEFAULT),     RGB(0, 56, 184),   0,              L"HE"},  // Hebrew Blue
    {MAKELANGID(LANG_POLISH, SUBLANG_DEFAULT),     RGB(220, 20, 60),  RGB(0, 0, 0),   L"PL"},  // Polish Red and White
    {MAKELANGID(LANG_CZECH, SUBLANG_DEFAULT),      RGB(215, 20, 26),  0,              L"CS"},  // Czech Red
    {MAKELANGID(LANG_GREEK, SUBLANG_DEFAULT),      RGB(13, 94, 175),  0,              L"EL"},  // Greek Blue
    // Add more languages as needed
};

const COLORREF DEFAULT_COLOR = RGB(100, 100, 100);  // Gray for unknown languages

HKL hklEnglish = NULL;
HKL hklSecondary = NULL;
HKL lastManualLayout = NULL;
HKL currentLayout = NULL;

ID2D1Factory* pD2DFactory = NULL;
IDWriteFactory* pDWriteFactory = NULL;

NOTIFYICONDATA nid = {0};
HWND hWnd = NULL;
HINSTANCE hInstance = NULL;

// Forward declarations
void LoadLayouts();
HKL GetActiveLayout();
void SwitchToLayout(HKL layout);
void SwitchLayout();
void UpdateTrayIcon(HKL layout);
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Get layout info using Windows API and preset data
void GetLayoutInfo(HKL layout, std::wstring& outCode, COLORREF& outColor1, COLORREF& outColor2) {
    LANGID langId = LOWORD((DWORD)(UINT_PTR)layout);
    LANGID primaryLangId = PRIMARYLANGID(langId);

    // First check preset colors
    for (const auto& preset : LAYOUT_COLORS) {
        if (PRIMARYLANGID(preset.langId) == primaryLangId) {
            outCode = preset.name;
            outColor1 = preset.color1;
            outColor2 = preset.color2;
            return;
        }
    }

    // If not found in presets, get from Windows
    wchar_t name[32] = {0};
    if (GetLocaleInfoW(MAKELCID(langId, SORT_DEFAULT), LOCALE_SISO639LANGNAME, name, 32)) {
        CharUpperW(name);
        outCode = name;
    } else if (GetLocaleInfoW(MAKELCID(langId, SORT_DEFAULT), LOCALE_SABBREVLANGNAME, name, 32)) {
        CharUpperW(name);
        name[2] = 0;  // Take first two characters
        outCode = name;
    } else {
        outCode = L"??";
    }
    
    outColor1 = DEFAULT_COLOR;
    outColor2 = 0;
}

HICON CreateFlagIcon(const std::wstring& text, COLORREF color1, COLORREF color2 = 0) {
    int width = 24;
    int height = 24;
    const int horizontalPadding = 2;
    
    HDC hdc = CreateCompatibleDC(NULL);
    if (!hdc) return NULL;

    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hBitmap) {
        DeleteDC(hdc);
        return NULL;
    }

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdc, hBitmap);

    if (color2 != 0) {
        RECT topHalf = { 0, 0, width, height/2 };
        RECT bottomHalf = { 0, height/2, width, height };
        HBRUSH hBrush1 = CreateSolidBrush(color1);
        HBRUSH hBrush2 = CreateSolidBrush(color2);
        FillRect(hdc, &topHalf, hBrush1);
        FillRect(hdc, &bottomHalf, hBrush2);
        DeleteObject(hBrush1);
        DeleteObject(hBrush2);
    } else {
        RECT rect = { 0, 0, width, height };
        HBRUSH hBrush = CreateSolidBrush(color1);
        FillRect(hdc, &rect, hBrush);
        DeleteObject(hBrush);
    }

    SetGraphicsMode(hdc, GM_ADVANCED);
    SetBkMode(hdc, TRANSPARENT);

    int fontSize = 16;
    bool textFits = false;
    HFONT hFont = NULL;
    SIZE textSize;
    HFONT currentFont = NULL;  // Keep track of current font
    
    while (fontSize >= 10 && !textFits) {
        if (currentFont) {
            SelectObject(hdc, currentFont);
            DeleteObject(currentFont);
        }
        
        LOGFONTW lf = {0};
        lf.lfHeight = -fontSize;
        lf.lfWeight = FW_BOLD;
        lf.lfQuality = ANTIALIASED_QUALITY;
        wcscpy_s(lf.lfFaceName, L"Arial");
        
        currentFont = CreateFontIndirectW(&lf);
        SelectObject(hdc, currentFont);
        
        GetTextExtentPoint32W(hdc, text.c_str(), text.length(), &textSize);
        
        if (textSize.cx <= (width - 2 * horizontalPadding)) {
            textFits = true;
            hFont = currentFont;
        } else {
            fontSize -= 2;
        }
    }

    // Draw text with the final font
    RECT textRect = { horizontalPadding, 0, width - horizontalPadding, height };
    SetTextColor(hdc, RGB(255, 255, 255));
    DrawTextW(hdc, text.c_str(), -1, &textRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    if (hFont) {
        SelectObject(hdc, GetStockObject(SYSTEM_FONT));
        DeleteObject(hFont);
    }
    
    SelectObject(hdc, hOldBitmap);
    DeleteDC(hdc);

    ICONINFO ii = {0};
    ii.fIcon = TRUE;
    ii.hbmColor = hBitmap;
    ii.hbmMask = CreateCompatibleBitmap(GetDC(NULL), width, height);
    HICON hIcon = CreateIconIndirect(&ii);

    DeleteObject(ii.hbmMask);
    DeleteObject(hBitmap);

    return hIcon;
}

void UpdateTrayIcon(HKL layout) {
    if (!layout) return;
    
    std::wstring langCode;
    COLORREF color1, color2;
    GetLayoutInfo(layout, langCode, color1, color2);
    
    HICON hIcon = CreateFlagIcon(langCode, color1, color2);
    if (hIcon) {
        if (nid.hIcon) DestroyIcon(nid.hIcon);
        nid.hIcon = hIcon;
        Shell_NotifyIcon(NIM_MODIFY, &nid);
    }
}

void LoadLayouts() {
    HKL layouts[100];  // Support up to 100 layouts
    int count = GetKeyboardLayoutList(100, layouts);
    
    hklEnglish = NULL;
    hklSecondary = NULL;
    
    // First pass: find English
    for (int i = 0; i < count; ++i) {
        LANGID langId = LOWORD((DWORD)(UINT_PTR)layouts[i]);
        if (PRIMARYLANGID(langId) == LANG_ENGLISH) {
            hklEnglish = layouts[i];
            break;
        }
    }
    
    // If no English found, use first layout
    if (!hklEnglish && count > 0) {
        hklEnglish = layouts[0];
    }
    
    // Second pass: find first non-English
    for (int i = 0; i < count; ++i) {
        LANGID langId = LOWORD((DWORD)(UINT_PTR)layouts[i]);
        if (PRIMARYLANGID(langId) != LANG_ENGLISH) {
            hklSecondary = layouts[i];
            break;
        }
    }
    
    // If no secondary found, use first non-English layout
    if (!hklSecondary && count > 1) {
        hklSecondary = layouts[1];
    }
}

HKL GetActiveLayout() {
    HWND hwnd = GetForegroundWindow();
    DWORD threadId = GetWindowThreadProcessId(hwnd, NULL);
    return GetKeyboardLayout(threadId);
}

void SwitchToLayout(HKL layout) {
    if (!layout) return;
    PostMessage(HWND_BROADCAST, WM_INPUTLANGCHANGEREQUEST, 0, (LPARAM)layout);
    UpdateTrayIcon(layout);
}

// Helper function to find layout by language ID
HKL FindLayoutByLanguage(LANGID targetLangId) {
    HKL layouts[20];
    int count = GetKeyboardLayoutList(20, layouts);
    
    for (int i = 0; i < count; i++) {
        LANGID langId = LOWORD((DWORD)(UINT_PTR)layouts[i]);
        if (PRIMARYLANGID(langId) == PRIMARYLANGID(targetLangId)) {
            return layouts[i];
        }

    }
    return NULL;
}

void SwitchLayout() {
    HKL current = GetActiveLayout();
    
    if (PRIMARYLANGID(LOWORD((DWORD)(UINT_PTR)(current))) != LANG_ENGLISH) {
        lastManualLayout = current;
        SwitchToLayout(hklEnglish);
    } else {
        HKL target = lastManualLayout ? lastManualLayout : hklSecondary;
        SwitchToLayout(target);
    }
}

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION) {
        KBDLLHOOKSTRUCT* p = (KBDLLHOOKSTRUCT*)lParam;

        if (wParam == WM_KEYDOWN && p->vkCode == VK_CAPITAL) {
            // Check if Shift is held
            SHORT shiftState = GetAsyncKeyState(VK_SHIFT);
            if (shiftState & 0x8000) {
                // Shift + CapsLock pressed: let CapsLock toggle normally
                return CallNextHookEx(NULL, nCode, wParam, lParam);
            } else {
                // CapsLock alone: switch layout
                SwitchLayout();
                // Block original CapsLock key to prevent toggling CapsLock state
                return 1;
            }
        }
    }
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

// Add function to check current layout
void CheckAndUpdateLayout() {
    HKL currentLayout = GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), NULL));
    
    static HKL lastLayout = NULL;
    if (currentLayout != lastLayout) {
        lastLayout = currentLayout;
        UpdateTrayIcon(currentLayout);
    }
}

// Helper function to get layout name for menu
std::wstring GetLayoutMenuName(HKL layout) {
    wchar_t debugBuf[512];
    wchar_t name[KL_NAMELENGTH] = {0};
    LANGID langId = LOWORD((DWORD)(UINT_PTR)layout);
    
    swprintf(debugBuf, L"[LangSwitcher] Processing layout - HKL: %p, LANGID: %04X\n", layout, langId);
    OutputDebugStringW(debugBuf);

    // Convert LANGID to locale name
    wchar_t localeName[LOCALE_NAME_MAX_LENGTH] = {0};
    if (LCIDToLocaleName(MAKELCID(langId, SORT_DEFAULT), localeName, LOCALE_NAME_MAX_LENGTH, 0)) {
        swprintf(debugBuf, L"[LangSwitcher] Locale name: %s\n", localeName);
        OutputDebugStringW(debugBuf);

        // Try to get display name using modern API
        if (GetLocaleInfoEx(localeName, LOCALE_SLOCALIZEDDISPLAYNAME, name, KL_NAMELENGTH)) {
            swprintf(debugBuf, L"[LangSwitcher] LOCALE_SLOCALIZEDDISPLAYNAME returned: '%s'\n", name);
            OutputDebugStringW(debugBuf);
            return std::wstring(name);
        }
        
        // Fallback to native language name
        if (GetLocaleInfoEx(localeName, LOCALE_SNATIVELANGNAME, name, KL_NAMELENGTH)) {
            swprintf(debugBuf, L"[LangSwitcher] LOCALE_SNATIVELANGNAME returned: '%s'\n", name);
            OutputDebugStringW(debugBuf);
            return std::wstring(name);
        }
    }

    // Special handling for known problematic layouts
    switch(PRIMARYLANGID(langId)) {
        case LANG_UKRAINIAN: {
            swprintf(debugBuf, L"[LangSwitcher] Using hardcoded Ukrainian name\n");
            OutputDebugStringW(debugBuf);
            return L"українська";
        }
        case LANG_PORTUGUESE: {
            if (SUBLANGID(langId) == SUBLANG_PORTUGUESE_BRAZILIAN) {
                swprintf(debugBuf, L"[LangSwitcher] Using hardcoded Brazilian Portuguese name\n");
                OutputDebugStringW(debugBuf);
                return L"Português (Brasil)";
            } else {
                swprintf(debugBuf, L"[LangSwitcher] Using hardcoded Portuguese name\n");
                OutputDebugStringW(debugBuf);
                return L"Português (Portugal)";
            }
        }
    }

    // Try LOCALE_SLANGUAGE as another fallback
    LCID lcid = MAKELCID(langId, SORT_DEFAULT);
    if (GetLocaleInfoW(lcid, LOCALE_SLANGUAGE, name, KL_NAMELENGTH)) {
        swprintf(debugBuf, L"[LangSwitcher] LOCALE_SLANGUAGE fallback returned: '%s'\n", name);
        OutputDebugStringW(debugBuf);
        return std::wstring(name);
    }

    // Ultimate fallback: try to get layout name
    if (GetKeyboardLayoutNameW(name)) {
        swprintf(debugBuf, L"[LangSwitcher] Ultimate fallback to layout name: '%s'\n", name);
        OutputDebugStringW(debugBuf);
        return std::wstring(name);
    }

    OutputDebugStringW(L"[LangSwitcher] All methods failed, returning 'Unknown Layout'\n");
    return L"Unknown Layout";
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // Start timer for layout checks
        SetTimer(hwnd, LAYOUT_CHECK_TIMER, 100, NULL);  // Check every 100ms
        break;
        
    case WM_TIMER:
        if (wParam == LAYOUT_CHECK_TIMER) {
            CheckAndUpdateLayout();
        }
        break;

    case WM_TRAYICON:
        if (lParam == WM_RBUTTONUP) {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            if (hMenu) {
                // Get all available layouts
                HKL layouts[100];
                int layoutCount = GetKeyboardLayoutList(100, layouts);
                
                // Add menu items for each layout
                for (int i = 0; i < layoutCount; i++) {
                    std::wstring menuName = GetLayoutMenuName(layouts[i]);
                    InsertMenuW(hMenu, -1, MF_BYPOSITION, ID_SWITCH_BASE + i, menuName.c_str());
                    
                    // Mark current layout
                    HKL currentLayout = GetKeyboardLayout(GetWindowThreadProcessId(GetForegroundWindow(), NULL));
                    if (layouts[i] == currentLayout) {
                        CheckMenuItem(hMenu, ID_SWITCH_BASE + i, MF_CHECKED);
                    }
                }
                
                InsertMenu(hMenu, -1, MF_SEPARATOR, 0, NULL);
                InsertMenu(hMenu, -1, MF_BYPOSITION, ID_TRAY_ABOUT, "About");
                InsertMenu(hMenu, -1, MF_BYPOSITION, ID_TRAY_EXIT, "Exit");
                
                SetForegroundWindow(hwnd);
                TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
            }
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            PostQuitMessage(0);
        }
        else if (LOWORD(wParam) == ID_TRAY_ABOUT) {
            MessageBoxW(hwnd, 
                L"Language Switcher v1.0\n\n"
                L"Created by Oleksandr Tolstov\n"
                L"GitHub: https://github.com/KillerOKPlanet\n"
                L"Email: alext0401@gmail.com\n\n"
                L"A lightweight keyboard layout switcher that uses CapsLock\n"
                L"to toggle between English and last used language.\n"
                L"Shift+CapsLock works as regular CapsLock.",
                L"About Language Switcher",
                MB_ICONINFORMATION | MB_OK);
        }
        else if (LOWORD(wParam) >= ID_SWITCH_BASE) {
            int layoutIndex = LOWORD(wParam) - ID_SWITCH_BASE;
            HKL layouts[100];
            int layoutCount = GetKeyboardLayoutList(100, layouts);
            
            if (layoutIndex < layoutCount) {
                HKL targetLayout = layouts[layoutIndex];
                if (PRIMARYLANGID(LOWORD((DWORD)(UINT_PTR)targetLayout)) != LANG_ENGLISH) {
                    lastManualLayout = targetLayout;
                }
                SwitchToLayout(targetLayout);
            }
        }
        break;
    case WM_DESTROY:
        KillTimer(hwnd, LAYOUT_CHECK_TIMER);  // Clean up timer
        Shell_NotifyIcon(NIM_DELETE, &nid);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    hInstance = hInst;

    WNDCLASS wc = {0};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "TrayAppClass";

    if (!RegisterClass(&wc)) {
        MessageBoxA(NULL, "Failed to register window class", "Error", MB_OK);
        return 1;
    }

    hWnd = CreateWindow("TrayAppClass", "TrayApp", WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, 300, 200,
                        NULL, NULL, hInstance, NULL);

    if (!hWnd) {
        MessageBoxA(NULL, "Failed to create window", "Error", MB_OK);
        return 1;
    }

    // Initialize DirectWrite and Direct2D
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pD2DFactory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(&pDWriteFactory));

    // Add tray icon with initial emoji
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    nid.hIcon = CreateFlagIcon(L"EN", RGB(178, 34, 52));
    strcpy_s(nid.szTip, sizeof(nid.szTip), "Keyboard Layout Switcher");

    if (!Shell_NotifyIcon(NIM_ADD, &nid)) {
        MessageBoxA(NULL, "Failed to add tray icon", "Error", MB_OK);
        return 1;
    }

    LoadLayouts();

    HHOOK hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
    if (!hook) {
        MessageBoxA(NULL, "Failed to set hook", "Error", MB_OK);
        return 1;
    }

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    UnhookWindowsHookEx(hook);

    // Cleanup Direct2D and DirectWrite
    if (pD2DFactory) pD2DFactory->Release();
    if (pDWriteFactory) pDWriteFactory->Release();

    return 0;
}

