// dict.cpp
// 汉语常用词库管理系统（Windows / GCC / C++11，单文件，完全去重 + 分词）
//
// 编译（老 MinGW，无 -municode）：
//   g++ -std=c++11 -O2 -Wall -Wextra \
//       -finput-charset=UTF-8 -fexec-charset=UTF-8 \
//       dict.cpp -o dict.exe
//
// 数据文件：dict.dat，UTF-8（无 BOM），每行一个词，文件中不含重复词。

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <conio.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "config.h"
// ================= 字符串工具 =================

namespace StringUtils {

inline std::string trim(const std::string &s) {
    size_t start = 0;
    while (start < s.size() && (unsigned char)s[start] <= ' ') ++start;
    size_t end = s.size();
    while (end > start && (unsigned char)s[end - 1] <= ' ') --end;
    return s.substr(start, end - start);
}

inline void trimInPlace(std::string &s) { s = trim(s); }

} // namespace StringUtils

// ================= UTF-8 工具 =================

namespace UTF8Utils {

// 按 UTF-8 字符切分；非法字节按单字节处理，尽量不丢内容
inline std::vector<std::string> splitToChars(const std::string &s) {
    std::vector<std::string> out;
    out.reserve(s.size());
    size_t i = 0, n = s.size();
    while (i < n) {
        unsigned char c = (unsigned char)s[i];
        size_t len;
        if ((c & 0x80) == 0x00)
            len = 1;
        else if ((c & 0xE0) == 0xC0)
            len = 2;
        else if ((c & 0xF0) == 0xE0)
            len = 3;
        else if ((c & 0xF8) == 0xF0)
            len = 4;
        else
            len = 1;              // 非法首字节
        if (i + len > n) len = 1; // 截断保护
        out.push_back(s.substr(i, len));
        i += len;
    }
    return out;
}

// UTF-8 码点数（每个汉字算 1）
inline size_t length(const std::string &s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if ((c & 0xC0) != 0x80) ++n;
    }
    return n;
}
inline std::string readLineSafe() {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    std::wstring w;
    wchar_t buf[2];
    DWORD n;
    while (ReadConsoleW(h, buf, 1, &n, nullptr) && n > 0) {
        wchar_t c = buf[0];
        if (c == L'\r') continue;
        if (c == L'\n') break;
        if (c == 0x1A) break; // Ctrl+Z 视为结束
        w.push_back(c);
    }
    if (w.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr,
                                  0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string s((size_t)len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], len,
                        nullptr, nullptr);
    return s;
}
} // namespace UTF8Utils

// ================= 控制台工具 =================

namespace console {

inline void clearScreen() {
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(h, &csbi)) return;
    DWORD cellCount = csbi.dwSize.X * csbi.dwSize.Y;
    DWORD written = 0;
    COORD home = {0, 0};
    FillConsoleOutputCharacterA(h, ' ', cellCount, home, &written);
    FillConsoleOutputAttribute(h, csbi.wAttributes, cellCount, home, &written);
    SetConsoleCursorPosition(h, home);
}

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

} // namespace console

// 阻塞读键：普通键返回 ASCII；扩展键返回 0x100 | 扫描码
inline int readKeyBlocking() {
    int c = _getch();
    if (c == 0 || c == 224) {
        int c2 = _getch();
        return 0x100 | c2;
    }
    return c;
}

static const int KEY_EXT_UP = 0x100 | 72;
static const int KEY_EXT_DOWN = 0x100 | 80;
static const int KEY_ESC = 27;
static const int KEY_ENTER = 13;

inline void waitAnyKey(const char *prompt = "按任意键返回主菜单...") {
    std::cout << "\n" << prompt;
    std::cout.flush();
    _getch();
}

// ================= 词库类（完全去重） =================

class Dictionary {
  public:
    explicit Dictionary(const std::string &path) : path_(path) {}

    bool load() {
        data_.clear();
        std::ifstream in(path_.c_str(), std::ios::binary);
        if (!in) return false;
        std::string line;
        bool first = true;
        while (std::getline(in, line)) {
            if (first) {
                if (line.size() >= 3 && (unsigned char)line[0] == 0xEF &&
                    (unsigned char)line[1] == 0xBB &&
                    (unsigned char)line[2] == 0xBF) {
                    line.erase(0, 3);
                }
                first = false;
            }
            StringUtils::trimInPlace(line);
            if (!line.empty()) data_.insert(line);
        }
        return true;
    }

    bool save() const {
        std::string tmp = path_ + ".tmp";
        {
            std::ofstream out(tmp.c_str(), std::ios::binary | std::ios::trunc);
            if (!out) return false;
            for (std::set<std::string>::const_iterator it = data_.begin();
                 it != data_.end(); ++it) {
                out << *it << "\n";
            }
            out.flush();
            if (!out) return false;
            out.close();
        }
        if (!MoveFileExA(tmp.c_str(), path_.c_str(),
                         MOVEFILE_REPLACE_EXISTING)) {
            DeleteFileA(tmp.c_str());
            return false;
        }
        return true;
    }

    bool empty() const { return data_.empty(); }
    size_t size() const { return data_.size(); }

    bool contains(const std::string &w) const {
        return data_.find(w) != data_.end();
    }

    bool addWord(const std::string &w) { return data_.insert(w).second; }

    bool removeWord(const std::string &w) { return data_.erase(w) > 0; }

    void clearAll() { data_.clear(); }

    std::vector<std::string> allWords() const {
        return std::vector<std::string>(data_.begin(), data_.end());
    }

    std::vector<std::string> search(const std::string &kw) const {
        std::vector<std::string> v;
        for (std::set<std::string>::const_iterator it = data_.begin();
             it != data_.end(); ++it) {
            if (it->find(kw) != std::string::npos) v.push_back(*it);
        }
        return v;
    }

    void extremeByLength(std::string &shortest, std::string &longest) const {
        bool first = true;
        size_t sl = 0, ll = 0;
        for (std::set<std::string>::const_iterator it = data_.begin();
             it != data_.end(); ++it) {
            size_t len = UTF8Utils::length(*it);
            if (first) {
                shortest = longest = *it;
                sl = ll = len;
                first = false;
                continue;
            }
            if (len < sl) {
                shortest = *it;
                sl = len;
            }
            if (len > ll) {
                longest = *it;
                ll = len;
            }
        }
    }

  private:
    std::string path_;
    std::set<std::string> data_;
};

// ================= Trie 与分词 =================

struct TrieNode {
    std::map<std::string, std::shared_ptr<TrieNode>> children;
    bool is_end;
    TrieNode() : is_end(false) {}
};

class Segmenter {
  public:
    Segmenter() : root_(std::make_shared<TrieNode>()) {}

    // 从文件（每行一个词）加载词典
    bool loadDict(const std::string &path) {
        std::ifstream f(path.c_str(), std::ios::binary);
        if (!f) return false;
        std::string line;
        bool first = true;
        while (std::getline(f, line)) {
            if (first) {
                if (line.size() >= 3 && (unsigned char)line[0] == 0xEF &&
                    (unsigned char)line[1] == 0xBB &&
                    (unsigned char)line[2] == 0xBF) {
                    line.erase(0, 3);
                }
                first = false;
            }
            StringUtils::trimInPlace(line);
            if (line.empty()) continue;
            insert(line);
        }
        return true;
    }

    void insert(const std::string &word) {
        if (word.empty()) return;
        std::shared_ptr<TrieNode> node = root_;
        const std::vector<std::string> chars = UTF8Utils::splitToChars(word);
        for (size_t i = 0; i < chars.size(); ++i) {
            std::shared_ptr<TrieNode> &next = node->children[chars[i]];
            if (!next) next = std::make_shared<TrieNode>();
            node = next;
        }
        node->is_end = true;
    }

    // 正向最大匹配（FMM）
    std::vector<std::string> segment(const std::string &text) const {
        std::vector<std::string> result;
        const std::vector<std::string> chars = UTF8Utils::splitToChars(text);
        const size_t n = chars.size();
        size_t start = 0;
        while (start < n) {
            std::shared_ptr<const TrieNode> cur = root_;
            size_t i = start, bestEnd = start;
            while (i < n) {
                std::map<std::string, std::shared_ptr<TrieNode>>::const_iterator
                    it = cur->children.find(chars[i]);
                if (it == cur->children.end()) break;
                cur = it->second;
                ++i;
                if (cur->is_end) bestEnd = i;
            }
            if (bestEnd > start) {
                std::string w;
                for (size_t k = start; k < bestEnd; ++k) w += chars[k];
                result.push_back(std::move(w));
                start = bestEnd;
            } else {
                result.push_back(chars[start]);
                ++start;
            }
        }
        return result;
    }

  private:
    std::shared_ptr<TrieNode> root_;
};

// ================= 分页查看 =================

static void pagedView(const std::string &title,
                      const std::vector<std::string> &words) {
    if (words.empty()) {
        console::clearScreen();
        std::cout << "\n--- " << title << " ---\n";
        std::cout << "(无匹配结果)\n";
        waitAnyKey();
        return;
    }

    const int PAGE_SIZE = 10;
    const int total = (int)words.size();
    const int totalPages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    int page = 0;

    console::hideCursor();

    bool running = true;
    while (running) {
        std::ostringstream oss;
        oss << "--- " << title << " (第 " << (page + 1) << "/" << totalPages
            << " 页) ---\n";
        oss << "-----------------------------------------\n";

        const int start = page * PAGE_SIZE;
        const int end = std::min(start + PAGE_SIZE, total);
        for (int i = start; i < end; ++i) {
            char buf[16];
            std::snprintf(buf, sizeof(buf), "%05d", i + 1);
            oss << "[" << buf << "] " << words[i] << "\n";
        }
        for (int i = end - start; i < PAGE_SIZE; ++i) oss << "\n";

        oss << "-----------------------------------------\n";
        oss << "共 " << total << " 个词语。\n";
        oss << "[↑/↓] 翻页  [ESC/Enter] 返回\n";

        console::clearScreen();
        std::cout << oss.str();
        std::cout.flush();

        int key = readKeyBlocking();
        if (key == KEY_ESC || key == KEY_ENTER) {
            running = false;
        } else if (key == KEY_EXT_UP) {
            if (page > 0) --page;
        } else if (key == KEY_EXT_DOWN) {
            if (page < totalPages - 1) ++page;
        }
    }

    console::showCursor();
    console::clearScreen();
}

// ================= 菜单 =================

static void showMenu() {
    console::clearScreen();
    std::cout << "\n=== 汉语常用词库管理系统 ===\n\n";
    std::cout << "【词库】\n";
    std::cout << "  1. 录入新词语\n";
    std::cout << "  2. 查看现有词库\n";
    std::cout << "  3. 搜索词语\n";
    std::cout << "  4. 删除词语\n";
    std::cout << "  5. 统计词库信息\n";
    std::cout << "  6. 分词\n\n";
    std::cout << "【配置】\n";
    std::cout << "  7. 编辑同义词\n";
    std::cout << "  8. 编辑停用词\n";
    std::cout << "  9. 编辑关系词\n";
    std::cout << " 10. 编辑代词\n";
    std::cout << " 11. 编辑时间词\n";
    std::cout << " 12. 编辑连接词 / 量词\n";
    std::cout << " 13. 编辑兜底回复\n";
    std::cout << " 14. 编辑聊天角色 / 动词黑名单\n\n";
    std::cout << "【维护】\n";
    std::cout << " 15. 重建所有配置文件（覆盖）\n";
    std::cout << " 16. 清空词库\n\n";
    std::cout << "  0. 退出程序\n\n";
    std::cout << "请选择操作: ";
    std::cout.flush();
}

// ================= 功能实现 =================

static void inputWords(Dictionary &dict) {
    console::clearScreen();
    std::cout << "\n--- 录入新词语 ---\n";
    std::cout << "请输入词语，以空白分隔（词内不能含空格）。\n";
    std::cout << "输入完成后按 Ctrl+Z 再回车结束。\n";
    std::cout << "已存在的词会被自动忽略（完全去重）。\n\n";

    int added = 0, dup = 0;
    while (true) {
        std::string line = UTF8Utils::readLineSafe();
        if (line.empty()) break;
        std::istringstream iss(line);
        std::string word;
        while (iss >> word) {
            if (dict.addWord(word))
                ++added;
            else
                ++dup;
        }
    }

    if (!dict.save()) {
        std::cout << "\n保存失败！请检查 dict.dat 是否被其他程序占用。\n";
    } else {
        std::cout << "\n录入完成！\n";
        std::cout << "新增词语: " << added << " 个\n";
        std::cout << "重复忽略: " << dup << " 个\n";
        std::cout << "当前词库总数: " << dict.size() << " 个\n";
    }
    waitAnyKey();
}

static void viewDictionary(const Dictionary &dict) {
    pagedView("现有词库", dict.allWords());
}

static void searchDictionary(const Dictionary &dict) {
    console::clearScreen();
    std::cout << "\n--- 搜索词语 ---\n";
    std::cout << "请输入关键字: ";
    std::cout.flush();

    std::string kw = UTF8Utils::readLineSafe();
    StringUtils::trimInPlace(kw);
    if (kw.empty()) return;

    std::vector<std::string> hits = dict.search(kw);
    pagedView("搜索结果 \"" + kw + "\"", hits);
}

static bool confirm(const std::string &prompt) {
    std::cout << prompt << " (y/N): ";
    std::cout.flush();
    std::string ans;
    if (!std::getline(std::cin, ans)) {
        std::cin.clear();
        return false;
    }
    return !ans.empty() && (ans[0] == 'y' || ans[0] == 'Y');
}

static void deleteWords(Dictionary &dict) {
    while (true) {
        console::clearScreen();
        std::cout << "\n--- 删除词语 ---\n";
        std::cout << "1. 按词语删除\n";
        std::cout << "2. 按序号删除（编号与查看界面一致）\n";
        std::cout << "0. 返回主菜单\n\n";
        std::cout << "请选择: ";
        std::cout.flush();

        int opt = -1;
        if (!(std::cin >> opt)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        if (opt == 0) return;

        std::vector<std::string> words = dict.allWords();

        if (opt == 1) {
            std::cout << "请输入要删除的词: ";
            std::cout.flush();
            std::string w = UTF8Utils::readLineSafe();
            StringUtils::trimInPlace(w);
            if (w.empty()) continue;
            if (!dict.contains(w)) {
                std::cout << "词库中不存在该词。\n";
                waitAnyKey();
                continue;
            }
            if (confirm("确认删除 \"" + w + "\" ？")) {
                dict.removeWord(w);
                std::cout << (dict.save() ? "已删除。\n" : "保存失败！\n");
            } else {
                std::cout << "已取消。\n";
            }
            waitAnyKey();
        } else if (opt == 2) {
            if (words.empty()) {
                std::cout << "词库为空。\n";
                waitAnyKey();
                continue;
            }
            std::cout << "当前词条数: " << words.size() << "\n";
            std::cout << "请输入要删除的序号（1-" << words.size()
                      << "，0 取消）: ";
            std::cout.flush();
            long idx = 0;
            if (!(std::cin >> idx)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(),
                                '\n');
                continue;
            }
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (idx <= 0 || idx > (long)words.size()) {
                std::cout << "序号无效。\n";
                waitAnyKey();
                continue;
            }
            std::string w = words[(size_t)(idx - 1)];
            std::ostringstream prompt;
            prompt << "确认删除 [" << idx << "] \"" << w << "\" ？";
            if (confirm(prompt.str())) {
                dict.removeWord(w);
                std::cout << (dict.save() ? "已删除。\n" : "保存失败！\n");
            } else {
                std::cout << "已取消。\n";
            }
            waitAnyKey();
        }
    }
}

static void statsDictionary(const Dictionary &dict) {
    console::clearScreen();
    std::cout << "\n--- 词库统计信息 ---\n";

    if (dict.empty()) {
        std::cout << "词库为空。\n";
        waitAnyKey();
        return;
    }

    std::cout << "词汇总数: " << dict.size() << " 个\n";

    std::string shortest, longest;
    dict.extremeByLength(shortest, longest);
    std::cout << "最短词语: " << shortest << " (" << UTF8Utils::length(shortest)
              << " 字)\n";
    std::cout << "最长词语: " << longest << " (" << UTF8Utils::length(longest)
              << " 字)\n";

    waitAnyKey();
}

// 分词：从当前 Dictionary 重建 Trie，循环处理多行输入
static void segmentText(const Dictionary &dict) {
    console::clearScreen();
    std::cout << "\n--- 分词 ---\n";

    if (dict.empty()) {
        std::cout << "词库为空，请先录入词语。\n";
        waitAnyKey();
        return;
    }

    // 用当前词库构建 Trie
    Segmenter seg;
    {
        std::vector<std::string> words = dict.allWords();
        for (size_t i = 0; i < words.size(); ++i) seg.insert(words[i]);
        std::cout << "分词词典已就绪（" << words.size() << " 词）。\n";
    }
    std::cout << "输入一行文本进行分词；直接回车返回主菜单。\n\n";

    while (true) {
        std::cout << "> ";
        std::cout.flush();

        std::string line = UTF8Utils::readLineSafe();
        StringUtils::trimInPlace(line);
        if (line.empty()) break;

        std::vector<std::string> segs = seg.segment(line);

        std::cout << "  ";
        size_t width = 0;
        for (size_t i = 0; i < segs.size(); ++i) {
            std::string piece = "[" + segs[i] + "]";
            size_t w = UTF8Utils::length(piece);
            if (width + w > 60 && width > 0) {
                std::cout << "\n  ";
                width = 0;
            }
            std::cout << piece;
            width += w;
        }
        std::cout << "\n\n";
    }

    console::clearScreen();
}

static void clearDictionary(Dictionary &dict) {
    console::clearScreen();
    std::cout << "\n--- 警告: 清空词库 ---\n";
    std::cout << "此操作将删除所有词语，且不可恢复！\n";
    if (confirm("确定要清空吗？")) {
        dict.clearAll();
        std::cout << (dict.save() ? "词库已清空。\n" : "保存失败！\n");
    } else {
        std::cout << "操作已取消。\n";
    }
    waitAnyKey();
}
static void
editStringList(const std::string &title, std::vector<std::string> &list,
               const std::string &configPath,
               std::function<std::vector<std::string>()> getDefault) {
    while (true) {
        console::clearScreen();
        std::cout << "\n--- " << title << " ---\n";
        std::cout << "文件: " << configPath << "\n";
        std::cout << "当前条数: " << list.size() << "\n\n";
        std::cout << "  a. 添加一项\n";
        std::cout << "  d. 删除一项\n";
        std::cout << "  v. 查看全部（分页）\n";
        std::cout << "  e. 用记事本打开\n";
        std::cout << "  r. 重置为默认\n";
        std::cout << "  0. 返回（自动保存）\n\n";
        std::cout << "请选择: ";
        std::cout.flush();

        std::string opt;
        if (!std::getline(std::cin, opt)) return;
        StringUtils::trimInPlace(opt);
        if (opt.empty()) continue;

        if (opt == "0") {
            // 保存到文件
            std::ofstream f(configPath, std::ios::binary | std::ios::trunc);
            for (auto &x : list) f << x << "\n";
            return;
        }

        if (opt == "a") {
            std::cout << "请输入要添加的内容: ";
            std::string s;
            std::getline(std::cin, s);
            StringUtils::trimInPlace(s);
            if (!s.empty()) {
                bool dup = false;
                for (auto &x : list)
                    if (x == s) {
                        dup = true;
                        break;
                    }
                if (dup)
                    std::cout << "已存在。\n";
                else {
                    list.push_back(s);
                    std::cout << "已添加。\n";
                }
            }
            Sleep(500);
        } else if (opt == "d") {
            if (list.empty()) {
                std::cout << "列表为空。\n";
                Sleep(800);
                continue;
            }
            std::cout << "请输入要删除的序号（1-" << list.size() << "）: ";
            long idx = 0;
            std::cin >> idx;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (idx >= 1 && idx <= (long)list.size()) {
                list.erase(list.begin() + (idx - 1));
                std::cout << "已删除。\n";
            } else {
                std::cout << "序号无效。\n";
            }
            Sleep(500);
        } else if (opt == "v") {
            pagedView(title, list);
        } else if (opt == "e") {
            system(("notepad " + configPath).c_str());
            // 重新加载
            std::ifstream f(configPath, std::ios::binary);
            if (f) {
                list.clear();
                std::string line;
                bool first = true;
                while (std::getline(f, line)) {
                    if (first) {
                        if (line.size() >= 3 &&
                            (unsigned char)line[0] == 0xEF &&
                            (unsigned char)line[1] == 0xBB &&
                            (unsigned char)line[2] == 0xBF)
                            line.erase(0, 3);
                        first = false;
                    }
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    StringUtils::trimInPlace(line);
                    if (!line.empty() && line[0] != '#') list.push_back(line);
                }
            }
        } else if (opt == "r") {
            if (confirm("确认重置为默认？")) {
                list = getDefault();
                std::cout << "已重置。\n";
                Sleep(500);
            }
        }
    }
}
// 同义词编辑
static void editSynonyms(Config &cfg) {
    const std::string path = "config/synonyms.txt";
    while (true) {
        console::clearScreen();
        auto &list = cfg.mutableSynonyms();
        std::cout << "\n--- 编辑同义词 ---\n";
        std::cout << "文件: " << path << "\n";
        std::cout << "当前组数: " << list.size() << "\n\n";
        for (size_t i = 0; i < list.size(); i++) {
            std::cout << "  " << (i + 1) << ". ";
            for (size_t j = 0; j < list[i].size(); j++) {
                if (j) std::cout << "|";
                std::cout << list[i][j];
            }
            std::cout << "\n";
        }
        std::cout << "\n  a. 添加一组\n";
        std::cout << "  d. 删除一组\n";
        std::cout << "  e. 用记事本打开\n";
        std::cout << "  r. 重置为默认\n";
        std::cout << "  0. 返回（自动保存）\n\n";
        std::cout << "请选择: ";
        std::cout.flush();

        std::string opt;
        std::getline(std::cin, opt);
        StringUtils::trimInPlace(opt);
        if (opt.empty()) continue;
        if (opt == "0") {
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            for (auto &g : list) {
                for (size_t i = 0; i < g.size(); i++) {
                    if (i) f << "|";
                    f << g[i];
                }
                f << "\n";
            }
            return;
        }
        if (opt == "a") {
            std::cout << "请输入一组（用 | 分隔，第一个是规范形式）: ";
            std::string s = UTF8Utils::readLineSafe();
            StringUtils::trimInPlace(s);
            if (s.empty()) continue;
            std::vector<std::string> group;
            std::istringstream iss(s);
            std::string cur;
            while (std::getline(iss, cur, '|')) {
                StringUtils::trimInPlace(cur);
                if (!cur.empty()) group.push_back(cur);
            }
            if (group.size() >= 2) {
                list.push_back(group);
                std::cout << "已添加。\n";
            } else {
                std::cout << "格式错误，至少需要两个词。\n";
            }
            Sleep(600);
        } else if (opt == "d") {
            std::cout << "请输入要删除的组号: ";
            long idx;
            std::cin >> idx;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (idx >= 1 && idx <= (long)list.size()) {
                list.erase(list.begin() + (idx - 1));
                std::cout << "已删除。\n";
            }
            Sleep(500);
        } else if (opt == "e") {
            system(("notepad " + path).c_str());
            // 简单起见：让用户返回后按 0 触发重新加载
            cfg.loadFromDir("config");
        } else if (opt == "r") {
            if (confirm("确认重置为默认？")) {
                Config tmp;
                tmp.resetToDefaults();
                list = tmp.synonyms();
            }
        }
    }
}

// 关系词编辑
static void editRelations(Config &cfg) {
    const std::string path = "config/relations.txt";
    while (true) {
        console::clearScreen();
        auto &list = cfg.mutableRelations();
        std::cout << "\n--- 编辑关系词 ---\n";
        std::cout << "文件: " << path << "\n";
        std::cout << "当前规则数: " << list.size() << "\n\n";
        for (size_t i = 0; i < list.size(); i++) {
            std::cout << "  " << (i + 1) << ". ";
            for (size_t j = 0; j < list[i].keywords.size(); j++) {
                if (j) std::cout << ",";
                std::cout << list[i].keywords[j];
            }
            std::cout << " | " << list[i].forwardTemplate;
            if (!list[i].backwardTemplate.empty())
                std::cout << " | " << list[i].backwardTemplate;
            std::cout << "\n";
        }
        std::cout << "\n  a. 添加规则\n";
        std::cout << "  d. 删除规则\n";
        std::cout << "  e. 用记事本打开\n";
        std::cout << "  r. 重置为默认\n";
        std::cout << "  0. 返回（自动保存）\n\n";
        std::cout << "请选择: ";
        std::cout.flush();

        std::string opt;
        std::getline(std::cin, opt);
        StringUtils::trimInPlace(opt);
        if (opt.empty()) continue;
        if (opt == "0") {
            std::ofstream f(path, std::ios::binary | std::ios::trunc);
            for (auto &r : list) {
                for (size_t i = 0; i < r.keywords.size(); i++) {
                    if (i) f << ",";
                    f << r.keywords[i];
                }
                f << "|" << r.forwardTemplate << "|" << r.backwardTemplate
                  << "\n";
            }
            return;
        }
        if (opt == "a") {
            std::cout << "请输入规则（关键词1,关键词2|正向模板|反向模板）：\n";
            std::cout << "示例：是|{S}是什么？|什么是{O}？\n";
            std::cout << "> ";
            std::string s = UTF8Utils::readLineSafe();
            StringUtils::trimInPlace(s);
            if (s.empty()) continue;
            std::vector<std::string> parts;
            std::istringstream iss(s);
            std::string cur;
            while (std::getline(iss, cur, '|')) parts.push_back(cur);
            if (parts.size() < 2) {
                std::cout << "格式错误。\n";
                Sleep(800);
                continue;
            }
            RelationRule r;
            std::istringstream kis(parts[0]);
            std::string kw;
            while (std::getline(kis, kw, ',')) {
                StringUtils::trimInPlace(kw);
                if (!kw.empty()) r.keywords.push_back(kw);
            }
            if (r.keywords.empty()) {
                std::cout << "关键词不能为空。\n";
                Sleep(800);
                continue;
            }
            r.forwardTemplate = parts[1];
            if (parts.size() >= 3) r.backwardTemplate = parts[2];
            list.push_back(r);
            std::cout << "已添加。\n";
            Sleep(600);
        } else if (opt == "d") {
            std::cout << "请输入要删除的规则号: ";
            long idx;
            std::cin >> idx;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (idx >= 1 && idx <= (long)list.size()) {
                list.erase(list.begin() + (idx - 1));
                std::cout << "已删除。\n";
            }
            Sleep(500);
        } else if (opt == "e") {
            system(("notepad " + path).c_str());
            cfg.loadFromDir("config");
        } else if (opt == "r") {
            if (confirm("确认重置为默认？")) {
                Config tmp;
                tmp.resetToDefaults();
                list = tmp.relations();
            }
        }
    }
}

// 重建全部配置
static void rebuildConfigs() {
    console::clearScreen();
    std::cout << "\n--- 重建所有配置文件 ---\n";
    std::cout << "会用内置默认值覆盖 config/ 下所有文件。\n";
    if (!confirm("确认继续？")) return;
    Config cfg;
    cfg.resetToDefaults();
    if (cfg.saveToDir("config")) {
        std::cout << "已重建。\n";
    } else {
        std::cout << "重建失败。\n";
    }
    Sleep(800);
}
// ================= 入口（老 MinGW：main + SetConsoleTitleW） =================
int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"汉语常用词库管理系统");
    setConsoleFont(L"Consolas", 20);

    // 确保 dict.dat 存在
    {
        std::ifstream check("dict.dat");
        if (!check.good()) {
            std::ofstream create("dict.dat", std::ios::binary | std::ios::app);
            create.close();
        }
    }

    Dictionary dict("dict.dat");
    if (!dict.load()) {
        std::cerr << "无法加载 dict.dat\n";
        return 1;
    }

    // 加载配置
    Config config;
    config.ensureFilesExist("config");
    config.loadFromDir("config");

    while (true) {
        showMenu();
        int choice = -1;
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "\n无效输入。\n";
            Sleep(800);
            continue;
        }
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        switch (choice) {
        case 1:
            inputWords(dict);
            break;
        case 2:
            viewDictionary(dict);
            break;
        case 3:
            searchDictionary(dict);
            break;
        case 4:
            deleteWords(dict);
            break;
        case 5:
            statsDictionary(dict);
            break;
        case 6:
            segmentText(dict);
            break;
        case 7:
            editSynonyms(config);
            break;
        case 8:
            editStringList("编辑停用词", config.mutableStopwords(),
                           "config/stopwords.txt", [] {
                               Config c;
                               c.resetToDefaults();
                               return c.mutableStopwords();
                           });
            config.rebuildSets();
            break;
        case 9:
            editRelations(config);
            break;
        case 10:
            editStringList("编辑代词", config.mutablePronouns(),
                           "config/pronouns.txt", [] {
                               Config c;
                               c.resetToDefaults();
                               return c.mutablePronouns();
                           });
            break;
        case 11:
            editStringList("编辑时间词", config.mutableTimeWords(),
                           "config/timewords.txt", [] {
                               Config c;
                               c.resetToDefaults();
                               return c.mutableTimeWords();
                           });
            break;
        case 12: {
            console::clearScreen();
            std::cout << "1. 编辑连接词\n2. 编辑量词\n0. 返回\n请选择: ";
            int sub;
            std::cin >> sub;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (sub == 1)
                editStringList("编辑连接词", config.mutableConnectors(),
                               "config/connectors.txt", [] {
                                   Config c;
                                   c.resetToDefaults();
                                   return c.mutableConnectors();
                               });
            else if (sub == 2)
                editStringList("编辑量词", config.mutableQuantifiers(),
                               "config/quantifiers.txt", [] {
                                   Config c;
                                   c.resetToDefaults();
                                   return c.mutableQuantifiers();
                               });
            break;
        }
        case 13:
            editStringList("编辑兜底回复", config.mutableDefaultResponses(),
                           "config/defaults.txt", [] {
                               Config c;
                               c.resetToDefaults();
                               return c.mutableDefaultResponses();
                           });
            break;
        case 14: {
            console::clearScreen();
            std::cout
                << "1. 编辑聊天角色\n2. 编辑动词黑名单\n0. 返回\n请选择: ";
            int sub;
            std::cin >> sub;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            if (sub == 1) {
                editStringList("编辑聊天角色", config.mutableChatRoles(),
                               "config/chatroles.txt", [] {
                                   Config c;
                                   c.resetToDefaults();
                                   return c.mutableChatRoles();
                               });
                config.rebuildSets();
            } else if (sub == 2) {
                editStringList("编辑动词黑名单", config.mutableVerbBlacklist(),
                               "config/verbblacklist.txt", [] {
                                   Config c;
                                   c.resetToDefaults();
                                   return c.mutableVerbBlacklist();
                               });
                config.rebuildSets();
            }
            break;
        }
        case 15:
            rebuildConfigs();
            break;
        case 16:
            clearDictionary(dict);
            break;
        case 0:
            console::clearScreen();
            std::cout << "感谢使用，再见！\n";
            return 0;
        default:
            std::cout << "\n无效选项。\n";
            Sleep(800);
            break;
        }
    }
    return 0;
}