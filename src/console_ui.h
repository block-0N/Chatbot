// ============================================================
// console_ui.h — 控制台美化工具（functions.exe / dict.exe 共用）
// C++17 / Windows
// ============================================================
#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

namespace UI {

// ============================================================
// 颜色
// ============================================================
enum Color {
    BLACK = 0, BLUE = 1, GREEN = 2, CYAN = 3,
    RED = 4, MAGENTA = 5, YELLOW = 6, WHITE = 7,
    GRAY = 8,
    BRIGHT_BLUE = 9, BRIGHT_GREEN = 10, BRIGHT_CYAN = 11,
    BRIGHT_RED = 12, BRIGHT_MAGENTA = 13, BRIGHT_YELLOW = 14,
    BRIGHT_WHITE = 15
};

inline void setColor(int fg, int bg = 0) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTextAttribute(h, (WORD)((bg << 4) | fg));
}

inline void resetColor() {
    setColor(WHITE);
}

inline void print(const std::string& text, int fg, int bg = 0) {
    setColor(fg, bg);
    std::cout << text;
    resetColor();
}

inline void printLine(const std::string& text, int fg, int bg = 0) {
    print(text, fg, bg);
    std::cout << "\n";
}

// ============================================================
// 光标
// ============================================================
inline void hideCursor() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci;
    if (!GetConsoleCursorInfo(h, &ci)) return;
    ci.bVisible = FALSE;
    SetConsoleCursorInfo(h, &ci);
}

inline void showCursor() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_CURSOR_INFO ci;
    if (!GetConsoleCursorInfo(h, &ci)) return;
    ci.bVisible = TRUE;
    SetConsoleCursorInfo(h, &ci);
}

// ============================================================
// 字体
// ============================================================
inline void setFont(const wchar_t* faceName = L"Consolas", short fontSize = 18) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE) return;

    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    if (!GetCurrentConsoleFontEx(h, FALSE, &cfi)) {
        ZeroMemory(&cfi, sizeof(cfi));
        cfi.cbSize = sizeof(cfi);
        cfi.FontFamily = FF_DONTCARE;
        cfi.FontWeight = FW_NORMAL;
    }
    wcsncpy(cfi.FaceName, faceName, LF_FACESIZE - 1);
    cfi.FaceName[LF_FACESIZE - 1] = L'\0';
    cfi.dwFontSize.X = 0;
    cfi.dwFontSize.Y = fontSize;
    SetCurrentConsoleFontEx(h, FALSE, &cfi);
}

// ============================================================
// 窗口
// ============================================================
inline void setWindowSize(int cols, int rows) {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    // 先缩小窗口，再改缓冲区（避免 SetConsoleScreenBufferSize 失败）
    SMALL_RECT smallRect = { 0, 0, 1, 1 };
    SetConsoleWindowInfo(h, TRUE, &smallRect);

    COORD bufferSize = { (SHORT)cols, (SHORT)rows };
    SetConsoleScreenBufferSize(h, bufferSize);

    SMALL_RECT rect = { 0, 0, (SHORT)(cols - 1), (SHORT)(rows - 1) };
    SetConsoleWindowInfo(h, TRUE, &rect);
}

inline void disableResize() {
    HWND hwnd = GetConsoleWindow();
    if (!hwnd) return;
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    style &= ~(WS_MAXIMIZEBOX | WS_SIZEBOX);
    SetWindowLong(hwnd, GWL_STYLE, style);
}

inline void centerWindow() {
    HWND hwnd = GetConsoleWindow();
    if (!hwnd) return;
    RECT r;
    GetWindowRect(hwnd, &r);
    int w = r.right - r.left;
    int h = r.bottom - r.top;
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    MoveWindow(hwnd, (sw - w) / 2, (sh - h) / 2, w, h, TRUE);
}

// ============================================================
// 清屏
// ============================================================
inline void clearScreen() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(h, &csbi)) return;
    DWORD cellCount = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD written = 0;
    COORD home = { 0, 0 };
    FillConsoleOutputCharacterA(h, ' ', cellCount, home, &written);
    FillConsoleOutputAttribute(h, csbi.wAttributes, cellCount, home, &written);
    SetConsoleCursorPosition(h, home);
}

// ============================================================
// 加载动画
// ============================================================
inline void showLoading(const std::string& msg, int totalMs = 600) {
    setColor(BRIGHT_CYAN);
    std::cout << "  " << msg;
    int steps = 6;
    int interval = totalMs / steps;
    for (int i = 0; i < steps; i++) {
        std::cout << ".";
        std::cout.flush();
        std::this_thread::sleep_for(std::chrono::milliseconds(interval));
    }
    std::cout << "  ";
    resetColor();
    print("[完成]\n", BRIGHT_GREEN);
}

// ============================================================
// 分隔线 / 标题
// ============================================================
inline void printDivider(int width = 44, int color = BRIGHT_CYAN) {
    setColor(color);
    std::string line;
    for (int i = 0; i < width; i++) line += "─";
    std::cout << "  " << line << "\n";
    resetColor();
}

inline void printTitle(const std::string& title, int width = 44) {
    setColor(BRIGHT_CYAN);
    std::cout << "  ╔";
    for (int i = 0; i < width - 2; i++) std::cout << "═";
    std::cout << "╗\n";

    // 居中标题
    int len = 0;
    for (size_t i = 0; i < title.size(); ) {
        unsigned char c = (unsigned char)title[i];
        int cl = 1;
        if ((c & 0x80) == 0) cl = 1;
        else if ((c & 0xE0) == 0xC0) cl = 2;
        else if ((c & 0xF0) == 0xE0) cl = 3;
        else if ((c & 0xF8) == 0xF0) cl = 4;
        i += cl;
        len++;
    }
    int padding = (width - 4 - len) / 2;
    if (padding < 0) padding = 0;

    std::cout << "  ║ ";
    for (int i = 0; i < padding; i++) std::cout << " ";
    setColor(BRIGHT_YELLOW);
    std::cout << title;
    setColor(BRIGHT_CYAN);
    int rightPad = width - 4 - len - padding;
    for (int i = 0; i < rightPad; i++) std::cout << " ";
    std::cout << " ║\n";

    std::cout << "  ╚";
    for (int i = 0; i < width - 2; i++) std::cout << "═";
    std::cout << "╝\n";
    resetColor();
}

// ============================================================
// 消息类型
// ============================================================
inline void info(const std::string& msg) {
    setColor(BRIGHT_CYAN);
    std::cout << "[系统] ";
    resetColor();
    std::cout << msg << "\n";
}

inline void warn(const std::string& msg) {
    setColor(BRIGHT_YELLOW);
    std::cout << "[警告] ";
    resetColor();
    std::cout << msg << "\n";
}

inline void error(const std::string& msg) {
    setColor(BRIGHT_RED);
    std::cout << "[错误] ";
    resetColor();
    std::cout << msg << "\n";
}

inline void success(const std::string& msg) {
    setColor(BRIGHT_GREEN);
    std::cout << "[成功] ";
    resetColor();
    std::cout << msg << "\n";
}

inline void printUser(const std::string& text) {
    setColor(BRIGHT_YELLOW);
    std::cout << "> ";
    resetColor();
    std::cout << text << "\n";
}

inline void printAI(const std::string& text) {
    setColor(BRIGHT_CYAN);
    std::cout << "AI: ";
    resetColor();
    std::cout << text << "\n";
}

// ============================================================
// ASCII 标题（启动时显示）
// ============================================================
inline void printBanner() {
    setColor(BRIGHT_CYAN);
    std::cout << "\n";
    std::cout << "    ██████╗██╗  ██╗ █████╗ ████████╗██████╗  ██████╗ ████████╗\n";
    std::cout << "   ██╔════╝██║  ██║██╔══██╗╚══██╔══╝██╔══██╗██╔═══██╗╚══██╔══╝\n";
    std::cout << "   ██║     ███████║███████║   ██║   ██████╔╝██║   ██║   ██║   \n";
    std::cout << "   ██║     ██╔══██║██╔══██║   ██║   ██╔══██╗██║   ██║   ██║   \n";
    std::cout << "   ╚██████╗██║  ██║██║  ██║   ██║   ██████╔╝╚██████╔╝   ██║   \n";
    std::cout << "    ╚═════╝╚═╝  ╚═╝╚═╝  ╚═╝   ╚═╝   ╚═════╝  ╚═════╝    ╚═╝   \n";
    setColor(BRIGHT_YELLOW);
    std::cout << "             C++ 离线 AI 聊天机器人  v2.0\n";
    resetColor();
    std::cout << "\n";
}

// ============================================================
// 初始化（一行调用）
// ============================================================
inline void init(const std::wstring& title = L"C++ 离线 AI 聊天机器人") {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(title.c_str());
    setFont(L"Consolas", 18);
    setWindowSize(90, 32);
    disableResize();
    centerWindow();
}

} // namespace UI