// ============================================================
//  离线聊天机器人 · 单文件版
//  平台: Windows (MSVC / MinGW)
//  依赖: 同目录 dict.dat（每行一词）、data.dat（自动生成）
//  新增: 同义词 / 候选列表 / 回答轮换 / forget/import/export /
//        输入防护 / 主动学习
// ============================================================

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <deque>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "config.h"
// ============================================================
// 1. 通用字符串工具
// ============================================================
namespace StringUtils {

std::string trim(const std::string &s) {
    size_t first = 0, last = s.size();
    while (first < last) {
        unsigned char c = (unsigned char)s[first];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            first++;
            continue;
        }
        if (first + 3 <= last && (unsigned char)s[first] == 0xE3 &&
            (unsigned char)s[first + 1] == 0x80 &&
            (unsigned char)s[first + 2] == 0x80) {
            first += 3;
            continue;
        }
        break;
    }
    while (last > first) {
        unsigned char c = (unsigned char)s[last - 1];
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            last--;
            continue;
        }
        if (last >= first + 3 && (unsigned char)s[last - 3] == 0xE3 &&
            (unsigned char)s[last - 2] == 0x80 &&
            (unsigned char)s[last - 1] == 0x80) {
            last -= 3;
            continue;
        }
        break;
    }
    return s.substr(first, last - first);
}

std::string toLowerAscii(const std::string &s) {
    std::string r = s;
    for (auto &c : r) {
        if ((unsigned char)c < 0x80) c = (char)std::tolower((unsigned char)c);
    }
    return r;
}

} // namespace StringUtils

// ============================================================
// 2. UTF-8 / UTF-16 工具
// ============================================================
namespace UTF8Utils {

inline int getCharLen(unsigned char c) {
    if ((c & 0x80) == 0) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

inline std::vector<std::string> splitToChars(const std::string &s) {
    std::vector<std::string> v;
    for (size_t i = 0; i < s.size();) {
        int len = getCharLen((unsigned char)s[i]);
        if (i + (size_t)len > s.size()) break;
        v.push_back(s.substr(i, len));
        i += len;
    }
    return v;
}

inline size_t charCount(const std::string &s) {
    size_t n = 0;
    for (size_t i = 0; i < s.size();) {
        int len = getCharLen((unsigned char)s[i]);
        if (i + (size_t)len > s.size()) break;
        n++;
        i += len;
    }
    return n;
}

std::wstring toWString(const std::string &s) {
    if (s.empty()) return L"";
    int wlen =
        MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    if (wlen <= 0) return L"";
    std::wstring w((size_t)wlen, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], wlen);
    return w;
}

std::string toUTF8(const std::wstring &w) {
    if (w.empty()) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr,
                                  0, nullptr, nullptr);
    if (len <= 0) return "";
    std::string s((size_t)len, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], len,
                        nullptr, nullptr);
    return s;
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
// ============================================================
// 3 中文数字解析
// ============================================================
inline int parseSimpleNumber(const std::string &s) {
    auto chars = UTF8Utils::splitToChars(s);
    static const std::unordered_map<std::string, int> cn = {
        {"一", 1}, {"二", 2}, {"两", 2}, {"三", 3}, {"四", 4}, {"五", 5},
        {"六", 6}, {"七", 7}, {"八", 8}, {"九", 9}, {"十", 10}};
    // 先找阿拉伯数字
    for (size_t i = 0; i < s.size();) {
        if (s[i] >= '1' && s[i] <= '9') {
            int n = 0;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
                n = n * 10 + (s[i] - '0');
                i++;
            }
            return n;
        }
        int len = UTF8Utils::getCharLen((unsigned char)s[i]);
        i += len;
    }
    // 再找中文数字
    for (const auto &c : chars) {
        auto it = cn.find(c);
        if (it != cn.end()) return it->second;
    }
    return 0;
}
// ============================================================
// 4. 中文标点 → 英文标点
// ============================================================
class CTC {
  public:
    static std::string RCPT(const std::string &utf8) {
        std::wstring w = UTF8Utils::toWString(utf8);
        if (w.empty()) return utf8;
        for (auto &c : w) {
            switch (c) {
            case L'，':
                c = L',';
                break;
            case L'。':
                c = L'.';
                break;
            case L'！':
                c = L'!';
                break;
            case L'？':
                c = L'?';
                break;
            case L'；':
                c = L';';
                break;
            case L'：':
                c = L':';
                break;
            case L'、':
                c = L',';
                break;
            case L'（':
                c = L'(';
                break;
            case L'）':
                c = L')';
                break;
            case L'《':
                c = L'<';
                break;
            case L'》':
                c = L'>';
                break;
            case L'“':
                c = L'"';
                break;
            case L'”':
                c = L'"';
                break;
            case L'‘':
                c = L'\'';
                break;
            case L'’':
                c = L'\'';
                break;
            case L'【':
                c = L'[';
                break;
            case L'】':
                c = L']';
                break;
            case L'—':
                c = L'-';
                break;
            case L'…':
                c = L'.';
                break;
            default:
                break;
            }
        }
        return UTF8Utils::toUTF8(w);
    }
};

// ============================================================
// 5. Trie + 分词器
// ============================================================
struct TrieNode {
    std::unordered_map<std::string, std::shared_ptr<TrieNode>> children;
    bool is_end = false;
};

class Segmenter {
  public:
    std::shared_ptr<TrieNode> root_ = std::make_shared<TrieNode>();

    bool loadDict(const std::string &path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        std::string line;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            line = StringUtils::trim(line);
            if (line.empty()) continue;
            insert(line);
        }
        return true;
    }

    void insert(const std::string &word) {
        auto node = root_;
        for (const auto &ch : UTF8Utils::splitToChars(word)) {
            auto &next = node->children[ch];
            if (!next) next = std::make_shared<TrieNode>();
            node = next;
        }
        node->is_end = true;
    }

    std::vector<std::string> segment(const std::string &text) const {
        std::vector<std::string> result;
        auto chars = UTF8Utils::splitToChars(text);
        size_t n = chars.size(), start = 0;
        while (start < n) {
            auto cur = root_;
            size_t i = start, bestEnd = start;
            while (i < n) {
                auto it = cur->children.find(chars[i]);
                if (it == cur->children.end()) break;
                cur = it->second;
                i++;
                if (cur->is_end) bestEnd = i;
            }
            if (bestEnd > start) {
                std::string w;
                for (size_t k = start; k < bestEnd; k++) w += chars[k];
                result.push_back(std::move(w));
                start = bestEnd;
            } else {
                result.push_back(chars[start]);
                start++;
            }
        }
        return result;
    }
};

// ============================================================
// 6. ChatResult
// ============================================================
struct ChatResult {
    std::string reply;
    bool awaitingAnswer = false;
    std::string pendingQuestion;
};
// ============================================================
// 文章抽取相关结构
// ============================================================
struct ExtractItem {
    std::string question;
    std::string answer;
};

struct ArticleStats {
    int sentencesScanned = 0;
    int sentencesMatched = 0;
    int itemsGenerated = 0;
    int itemsDuplicate = 0;
    int itemsTrained = 0;
    int newQuestions = 0;
    int newVariants = 0;
};
// ============================================================
// 6.5 技能系统
// ============================================================
class Skill {
  public:
    virtual ~Skill() = default;
    virtual std::string name() const = 0;
    virtual bool matches(const std::string &input) const = 0;
    virtual std::string execute(const std::string &input) = 0;
};

// ---- 时间技能 ----
class TimeSkill : public Skill {
  public:
    std::string name() const override { return "时间"; }

    bool matches(const std::string &input) const override {
        static const char *kws[] = {"几点",     "现在时间", "当前时间",
                                    "今天日期", "今天几号", "今天星期",
                                    "星期几"};
        for (auto *kw : kws) {
            if (input.find(kw) != std::string::npos) return true;
        }
        return false;
    }

    std::string execute(const std::string &input) override {
        SYSTEMTIME st;
        GetLocalTime(&st);
        static const char *weekdays[] = {"日", "一", "二", "三",
                                         "四", "五", "六"};
        char buf[128];
        if (input.find("星期") != std::string::npos) {
            snprintf(buf, sizeof(buf), "今天是星期%s", weekdays[st.wDayOfWeek]);
            return buf;
        }
        if (input.find("几号") != std::string::npos ||
            input.find("日期") != std::string::npos) {
            snprintf(buf, sizeof(buf), "今天是 %d 年 %d 月 %d 日", st.wYear,
                     st.wMonth, st.wDay);
            return buf;
        }
        snprintf(buf, sizeof(buf), "现在是 %02d:%02d:%02d", st.wHour,
                 st.wMinute, st.wSecond);
        return buf;
    }
};

// ---- 掷骰子技能 ----
class DiceSkill : public Skill {
    std::mt19937 &rng_;

  public:
    explicit DiceSkill(std::mt19937 &rng) : rng_(rng) {}
    std::string name() const override { return "骰子"; }

    bool matches(const std::string &input) const override {
        if (input.find("骰子") != std::string::npos) return true;
        if (input.find("色子") != std::string::npos) return true;
        return false;
    }

    std::string execute(const std::string &input) override {
        int n = parseSimpleNumber(input);
        if (n <= 0) n = 1;
        if (n > 20) n = 20; // 上限，防刷屏

        std::uniform_int_distribution<int> d(1, 6);
        std::vector<int> rolls;
        int total = 0;
        for (int i = 0; i < n; i++) {
            int v = d(rng_);
            rolls.push_back(v);
            total += v;
        }

        if (n == 1) {
            return "骰子点数: " + std::to_string(rolls[0]);
        }
        std::string detail;
        for (size_t i = 0; i < rolls.size(); i++) {
            if (i) detail += ", ";
            detail += std::to_string(rolls[i]);
        }
        return std::to_string(n) + " 颗骰子: " + detail + "，总和 " +
               std::to_string(total);
    }
};

// ---- 石头剪刀布技能 ----
class RpsSkill : public Skill {
    std::mt19937 &rng_;
    int win_ = 0, tie_ = 0, lose_ = 0;

  public:
    explicit RpsSkill(std::mt19937 &rng) : rng_(rng) {}
    std::string name() const override { return "石头剪刀布"; }

    bool matches(const std::string &input) const override {
        return input.find("石头剪刀布") != std::string::npos ||
               input.find("猜拳") != std::string::npos;
    }

    std::string execute(const std::string &input) override {
        // 战绩查询
        if (input.find("战绩") != std::string::npos ||
            input.find("成绩") != std::string::npos ||
            input.find("统计") != std::string::npos) {
            int total = win_ + tie_ + lose_;
            if (total == 0) return "还没有战绩，先来一局吧";
            return "战绩: " + std::to_string(win_) + " 胜 " +
                   std::to_string(tie_) + " 平 " + std::to_string(lose_) +
                   " 负（共 " + std::to_string(total) + " 局）";
        }

        // 剥离触发词
        std::string rest = input;
        for (const char *t : {"石头剪刀布", "猜拳"}) {
            size_t tl = strlen(t), p;
            while ((p = rest.find(t)) != std::string::npos) {
                rest.erase(p, tl);
            }
        }

        std::string player;
        if (rest.find("石头") != std::string::npos)
            player = "石头";
        else if (rest.find("剪刀") != std::string::npos)
            player = "剪刀";
        else if (rest.find("布") != std::string::npos)
            player = "布";

        if (player.empty()) {
            return "请这样出拳：\"猜拳 石头\"、\"猜拳 剪刀\"、\"猜拳 布\"";
        }

        static const char *choices[] = {"石头", "剪刀", "布"};
        std::uniform_int_distribution<int> d(0, 2);
        std::string bot = choices[d(rng_)];

        std::string result;
        if (player == bot) {
            result = "平局！";
            tie_++;
        } else if ((player == "石头" && bot == "剪刀") ||
                   (player == "剪刀" && bot == "布") ||
                   (player == "布" && bot == "石头")) {
            result = "你赢了！";
            win_++;
        } else {
            result = "我赢了！";
            lose_++;
        }

        return "你出" + player + "，我出" + bot + "，" + result;
    }
};
class TimerSkill : public Skill {
    std::chrono::steady_clock::time_point target_;
    bool active_ = false;

  public:
    std::string name() const override { return "倒计时"; }

    bool matches(const std::string &input) const override {
        return input.find("倒计时") != std::string::npos ||
               input.find("秒后提醒") != std::string::npos ||
               input.find("分钟后提醒") != std::string::npos ||
               input.find("秒后叫") != std::string::npos;
    }

    std::string execute(const std::string &input) override {
        int value = parseSimpleNumber(input);
        if (value <= 0) {
            return "请说：倒计时 10 秒 / 倒计时 5 分钟";
        }
        int seconds = value;
        if (input.find("分钟") != std::string::npos) seconds = value * 60;
        if (seconds > 3600) seconds = 3600; // 上限 1 小时

        target_ =
            std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        active_ = true;
        return "倒计时 " + std::to_string(seconds) + " 秒已开始";
    }

    // 外部调用：检查是否到时
    bool poll(std::string &outMsg) {
        if (!active_) return false;
        if (std::chrono::steady_clock::now() >= target_) {
            active_ = false;
            outMsg = "[倒计时] 时间到！";
            return true;
        }
        return false;
    }

    int remaining() const {
        if (!active_) return 0;
        auto diff = std::chrono::duration_cast<std::chrono::seconds>(
                        target_ - std::chrono::steady_clock::now())
                        .count();
        return diff > 0 ? (int)diff : 0;
    }
};
// ============================================================
// 技能：计算器（表达式解析）
// ============================================================
class CalcSkill : public Skill {
    const std::string *s_ = nullptr;
    size_t pos_ = 0;
    bool divByZero_ = false;

    void skip() {
        while (pos_ < s_->size() && (*s_)[pos_] == ' ') pos_++;
    }

    double parseExpr() {
        double v = parseTerm();
        while (true) {
            skip();
            if (pos_ >= s_->size()) break;
            char c = (*s_)[pos_];
            if (c == '+') {
                pos_++;
                v += parseTerm();
            } else if (c == '-') {
                pos_++;
                v -= parseTerm();
            } else
                break;
        }
        return v;
    }

    double parseTerm() {
        double v = parseFactor();
        while (true) {
            skip();
            if (pos_ >= s_->size()) break;
            char c = (*s_)[pos_];
            if (c == '*') {
                pos_++;
                v *= parseFactor();
            } else if (c == '/') {
                pos_++;
                double d = parseFactor();
                if (d == 0) {
                    divByZero_ = true;
                    return 0;
                }
                v /= d;
            } else
                break;
        }
        return v;
    }

    double parseFactor() {
        skip();
        if (pos_ >= s_->size()) return 0;
        char c = (*s_)[pos_];
        if (c == '-') {
            pos_++;
            return -parseFactor();
        }
        if (c == '+') {
            pos_++;
            return parseFactor();
        }
        if (c == '(') {
            pos_++;
            double v = parseExpr();
            skip();
            if (pos_ < s_->size() && (*s_)[pos_] == ')') pos_++;
            return v;
        }
        // 从当前位置读取数字
        size_t start = pos_;
        while (pos_ < s_->size() && (std::isdigit((unsigned char)(*s_)[pos_]) ||
                                     (*s_)[pos_] == '.')) {
            pos_++;
        }
        if (pos_ == start) return 0;
        // 只对 [start, pos_) 这段调用 stod
        return std::stod(s_->substr(start, pos_ - start));
    }

  public:
    std::string name() const override { return "计算"; }

    bool matches(const std::string &input) const override {
        if (input.find("计算") != std::string::npos) return true;
        if (input.find("等于多少") != std::string::npos &&
            input.find_first_of("+-*/") != std::string::npos)
            return true;
        return false;
    }

    std::string execute(const std::string &input) override {
        divByZero_ = false;

        // 抠出算式：从第一个数字或括号开始，到最后一个数字、括号、运算符结束
        size_t start = std::string::npos;
        size_t end = 0;
        for (size_t i = 0; i < input.size(); i++) {
            char c = input[i];
            if (std::isdigit((unsigned char)c) || c == '(') {
                if (start == std::string::npos) start = i;
                end = i + 1;
            } else if ((c == '+' || c == '-' || c == '*' || c == '/' ||
                        c == ')' || c == '.') &&
                       start != std::string::npos) {
                end = i + 1;
            }
        }
        if (start == std::string::npos) return "没找到算式，请说：计算 3+5*2";

        std::string expr = input.substr(start, end - start);
        s_ = &expr;
        pos_ = 0;
        double v = parseExpr();
        if (divByZero_) return "错误：除数不能为 0";

        std::ostringstream oss;
        if (v == (long long)v)
            oss << (long long)v;
        else
            oss << v;
        return expr + " = " + oss.str();
    }
};

// ============================================================
// 技能：随机数（用户指定范围）
// ============================================================
class RandomSkill : public Skill {
    std::mt19937 &rng_;

  public:
    explicit RandomSkill(std::mt19937 &rng) : rng_(rng) {}
    std::string name() const override { return "随机数"; }

    bool matches(const std::string &input) const override {
        return input.find("随机") != std::string::npos &&
               (input.find("到") != std::string::npos ||
                input.find("-") != std::string::npos ||
                input.find("~") != std::string::npos);
    }

    std::string execute(const std::string &input) override {
        // 抓所有数字
        std::vector<int> nums;
        int cur = 0;
        bool has = false;
        for (char c : input) {
            if (c >= '0' && c <= '9') {
                cur = cur * 10 + (c - '0');
                has = true;
            } else if (has) {
                nums.push_back(cur);
                cur = 0;
                has = false;
            }
        }
        if (has) nums.push_back(cur);
        if (nums.size() < 2) return "请说：随机 1 到 100";
        int a = nums[0], b = nums[1];
        if (a > b) std::swap(a, b);
        if (b - a > 1000000) return "范围太大";
        std::uniform_int_distribution<int> d(a, b);
        return std::to_string(a) + " ~ " + std::to_string(b) +
               " 随机: " + std::to_string(d(rng_));
    }
};

// ============================================================
// 技能：抛硬币
// ============================================================
class CoinSkill : public Skill {
    std::mt19937 &rng_;

  public:
    explicit CoinSkill(std::mt19937 &rng) : rng_(rng) {}
    std::string name() const override { return "硬币"; }

    bool matches(const std::string &input) const override {
        return input.find("抛硬币") != std::string::npos ||
               input.find("扔硬币") != std::string::npos ||
               input.find("掷硬币") != std::string::npos;
    }

    std::string execute(const std::string &) override {
        std::uniform_int_distribution<int> d(0, 1);
        return d(rng_) == 0 ? "正面！" : "反面！";
    }
};

// ============================================================
// 技能：今天吃什么
// ============================================================
class FoodSkill : public Skill {
    std::mt19937 &rng_;
    std::vector<std::string> menu_ = {
        "牛肉面", "兰州拉面", "黄焖鸡米饭", "沙县小吃", "小笼包配粥",
        "麻辣烫", "炒饭",     "盖浇饭",     "汉堡薯条", "烤串",
        "饺子",   "火锅",     "酸菜鱼",     "麻辣香锅", "刀削面",
        "肠粉",   "煲仔饭",   "咖喱饭",     "寿司",     "意面"};

  public:
    explicit FoodSkill(std::mt19937 &rng) : rng_(rng) {}
    std::string name() const override { return "吃啥"; }

    bool matches(const std::string &input) const override {
        return input.find("今天吃什么") != std::string::npos ||
               input.find("今天吃啥") != std::string::npos ||
               input.find("中午吃什么") != std::string::npos ||
               input.find("晚上吃什么") != std::string::npos ||
               input.find("吃什么好") != std::string::npos;
    }

    std::string execute(const std::string &) override {
        std::uniform_int_distribution<size_t> d(0, menu_.size() - 1);
        return "建议：吃 " + menu_[d(rng_)];
    }
};

// ============================================================
// 技能：字数统计
// ============================================================
class CharCountSkill : public Skill {
  public:
    std::string name() const override { return "字数"; }

    bool matches(const std::string &input) const override {
        return input.find("字数") != std::string::npos ||
               input.find("几个字") != std::string::npos;
    }

    std::string execute(const std::string &input) override {
        // 找出"字数"或"几个字"之后的正文
        std::string text = input;
        for (const char *kw :
             {"统计字数", "字数统计", "字数", "有几个字", "几个字"}) {
            size_t p = text.find(kw);
            if (p != std::string::npos) {
                text = StringUtils::trim(text.substr(p + strlen(kw)));
                break;
            }
        }
        if (text.empty()) return "请说：字数 你的文本";
        size_t total = UTF8Utils::charCount(text);
        size_t han = 0;
        for (const auto &c : UTF8Utils::splitToChars(text)) {
            unsigned char uc = (unsigned char)c[0];
            if (uc >= 0xE0) han++; // 粗略判汉字
        }
        return "共 " + std::to_string(total) + " 个字符，其中汉字约 " +
               std::to_string(han) + " 个";
    }
};

// ============================================================
// 技能：单位换算
// ============================================================
class UnitSkill : public Skill {
  public:
    std::string name() const override { return "换算"; }

    bool matches(const std::string &input) const override {
        static const char *kws[] = {"公里",   "千米", "米", "厘米", "毫米",
                                    "公斤",   "千克", "克", "斤",   "摄氏度",
                                    "华氏度", "℃",    "℉"};
        bool hasUnit = false;
        for (auto *k : kws)
            if (input.find(k) != std::string::npos) {
                hasUnit = true;
                break;
            }
        if (!hasUnit) return false;
        return input.find("换算") != std::string::npos ||
               input.find("等于") != std::string::npos ||
               input.find("转") != std::string::npos;
    }

    std::string execute(const std::string &input) override {
        double v = 0;
        bool has = false;
        for (size_t i = 0; i < input.size(); i++) {
            if (std::isdigit((unsigned char)input[i]) ||
                (input[i] == '.' && has)) {
                v = std::stod(input.substr(i));
                has = true;
                break;
            }
        }
        if (!has) return "请说：10公里等于多少米";
        bool metric = input.find("公里") != std::string::npos ||
                      input.find("千米") != std::string::npos;
        bool meters = input.find("米") != std::string::npos && !metric;
        bool cm = input.find("厘米") != std::string::npos;
        bool mm = input.find("毫米") != std::string::npos;
        bool kg = input.find("公斤") != std::string::npos ||
                  input.find("千克") != std::string::npos;
        bool g = input.find("克") != std::string::npos && !kg;
        size_t posC = input.find("摄氏度");
        if (posC == std::string::npos) posC = input.find("℃");
        size_t posF = input.find("华氏度");
        if (posF == std::string::npos) posF = input.find("℉");

        bool cToF = false, fToC = false;
        if (posC != std::string::npos && posF != std::string::npos) {
            if (posC < posF)
                cToF = true;
            else
                fToC = true;
        } else if (posC != std::string::npos) {
            cToF = true;
        } else if (posF != std::string::npos) {
            fToC = true;
        }
        std::ostringstream oss;
        if (cToF) {
            oss << v << " 摄氏度 = " << (v * 9.0 / 5.0 + 32) << " 华氏度";
        } else if (fToC) {
            oss << v << " 华氏度 = " << ((v - 32) * 5.0 / 9.0) << " 摄氏度";
        } else if (metric) {
            oss << v << " 公里 = " << v * 1000 << " 米";
        } else if (meters) {
            oss << v << " 米 = " << v * 100 << " 厘米";
        } else if (cm) {
            oss << v << " 厘米 = " << v * 10 << " 毫米";
        } else if (mm) {
            oss << v << " 毫米 = " << v / 10 << " 厘米";
        } else if (kg) {
            oss << v << " 公斤 = " << v * 2 << " 斤 = " << v * 1000 << " 克";
        } else if (g) {
            oss << v << " 克 = " << v / 1000 << " 公斤";
        } else {
            oss << "无法识别单位";
        }
        return oss.str();
    }
};

// ============================================================
// 技能：猜数字（多轮）
// ============================================================
class GuessSkill : public Skill {
    std::mt19937 &rng_;
    int target_ = -1;
    int tries_ = 0;

  public:
    explicit GuessSkill(std::mt19937 &rng) : rng_(rng) {}
    std::string name() const override { return "猜数字"; }

    bool matches(const std::string &input) const override {
        if (target_ != -1) {
            // 游戏中：接受纯数字或"退出"
            bool allDigits = !input.empty();
            for (char c : input)
                if (c < '0' || c > '9') {
                    allDigits = false;
                    break;
                }
            return allDigits || input.find("退出") != std::string::npos;
        }
        return input.find("猜数字") != std::string::npos;
    }

    std::string execute(const std::string &input) override {
        if (target_ == -1) {
            std::uniform_int_distribution<int> d(1, 100);
            target_ = d(rng_);
            tries_ = 0;
            return "我想了个 1~100 的数字，来猜吧（输入\"退出\"结束）";
        }
        if (input.find("退出") != std::string::npos) {
            int t = target_;
            target_ = -1;
            return "答案是 " + std::to_string(t) + "，下次再来";
        }
        int guess = std::stoi(input);
        tries_++;
        if (guess < target_) return "小了";
        if (guess > target_) return "大了";
        int t = target_;
        target_ = -1;
        return "猜对了！答案是 " + std::to_string(t) + "，用了 " +
               std::to_string(tries_) + " 次";
    }
};

// ============================================================
// 技能：内置诗词
// ============================================================
class PoemSkill : public Skill {
    std::mt19937 &rng_;
    std::vector<std::string> poems_ = {
        "静夜思：床前明月光，疑是地上霜。举头望明月，低头思故乡。",
        "登鹳雀楼：白日依山尽，黄河入海流。欲穷千里目，更上一层楼。",
        "春晓：春眠不觉晓，处处闻啼鸟。夜来风雨声，花落知多少。",
        "相思：红豆生南国，春来发几枝。愿君多采撷，此物最相思。",
        "悯农：锄禾日当午，汗滴禾下土。谁知盘中餐，粒粒皆辛苦。",
        "江雪：千山鸟飞绝，万径人踪灭。孤舟蓑笠翁，独钓寒江雪。",
        "鹿柴：空山不见人，但闻人语响。返景入深林，复照青苔上。",
        "山中送别：山中相送罢，日暮掩柴扉。春草明年绿，王孙归不归。",
        "早发白帝城：朝辞白帝彩云间，千里江陵一日还。",
        "黄鹤楼送孟浩然：故人西辞黄鹤楼，烟花三月下扬州。",
        "送元二使安西：渭城朝雨浥轻尘，客舍青青柳色新。",
        "九月九日忆山东兄弟：独在异乡为异客，每逢佳节倍思亲。",
        "望庐山瀑布：日照香炉生紫烟，遥看瀑布挂前川。",
        "赠汪伦：李白乘舟将欲行，忽闻岸上踏歌声。",
        "春夜喜雨：好雨知时节，当春乃发生。随风潜入夜，润物细无声。",
        "绝句：两个黄鹂鸣翠柳，一行白鹭上青天。",
        "山行：远上寒山石径斜，白云生处有人家。",
        "江南春：千里莺啼绿映红，水村山郭酒旗风。",
        "泊船瓜洲：京口瓜洲一水间，钟山只隔数重山。",
        "题西林壁：横看成岭侧成峰，远近高低各不同。",
        "小池：泉眼无声惜细流，树阴照水爱晴柔。",
        "村居：草长莺飞二月天，拂堤杨柳醉春烟。",
        "清明：清明时节雨纷纷，路上行人欲断魂。",
        "元日：爆竹声中一岁除，春风送暖入屠苏。",
        "咏柳：碧玉妆成一树高，万条垂下绿丝绦。",
        "望天门山：天门中断楚江开，碧水东流至此回。",
        "鸟鸣涧：人闲桂花落，夜静春山空。",
        "秋浦歌：白发三千丈，缘愁似个长。",
        "竹里馆：独坐幽篁里，弹琴复长啸。",
        "杂诗：君自故乡来，应知故乡事。"};

  public:
    explicit PoemSkill(std::mt19937 &rng) : rng_(rng) {}
    std::string name() const override { return "诗词"; }

    bool matches(const std::string &input) const override {
        return input.find("来首诗") != std::string::npos ||
               input.find("来首古诗") != std::string::npos ||
               input.find("随机古诗") != std::string::npos ||
               input.find("背首诗") != std::string::npos ||
               input.find("古诗") != std::string::npos;
    }

    std::string execute(const std::string &) override {
        std::uniform_int_distribution<size_t> d(0, poems_.size() - 1);
        return poems_[d(rng_)];
    }
};

// ============================================================
// 技能：名言
// ============================================================
class QuoteSkill : public Skill {
    std::mt19937 &rng_;
    std::vector<std::string> quotes_ = {
        "不积跬步，无以至千里。——荀子",
        "路漫漫其修远兮，吾将上下而求索。——屈原",
        "千里之行，始于足下。——老子",
        "三人行，必有我师焉。——孔子",
        "生于忧患，死于安乐。——孟子",
        "天行健，君子以自强不息。——《周易》",
        "业精于勤，荒于嬉；行成于思，毁于随。——韩愈",
        "读书破万卷，下笔如有神。——杜甫",
        "长风破浪会有时，直挂云帆济沧海。——李白",
        "宝剑锋从磨砺出，梅花香自苦寒来。——《警世贤文》",
        "知识就是力量。——培根",
        "生活不止眼前的苟且，还有诗和远方。——高晓松",
        "一个人可以被毁灭，但不能被打败。——海明威",
        "苦难是人生的老师。——巴尔扎克",
        "我们唯一恐惧的就是恐惧本身。——罗斯福",
        "成功没有捷径，唯有努力。——佚名",
        "己所不欲，勿施于人。——孔子",
        "学而不思则罔，思而不学则殆。——孔子",
        "穷则独善其身，达则兼济天下。——孟子",
        "海纳百川，有容乃大。——林则徐",
        "书山有路勤为径，学海无涯苦作舟。——韩愈",
        "少年易老学难成，一寸光阴不可轻。——朱熹",
        "宝剑不磨要生锈，人不学习要落后。——谚语",
        "天下事有难易乎？为之，则难者亦易矣。——彭端淑",
        "有志者事竟成。——《后汉书》",
        "天生我材必有用。——李白",
        "会当凌绝顶，一览众山小。——杜甫",
        "看似寻常最奇崛，成如容易却艰辛。——王安石",
        "问渠那得清如许，为有源头活水来。——朱熹",
        "纸上得来终觉浅，绝知此事要躬行。——陆游"};

  public:
    explicit QuoteSkill(std::mt19937 &rng) : rng_(rng) {}
    std::string name() const override { return "名言"; }

    bool matches(const std::string &input) const override {
        return input.find("名言") != std::string::npos ||
               input.find("来句鸡汤") != std::string::npos ||
               input.find("励志句") != std::string::npos;
    }

    std::string execute(const std::string &) override {
        std::uniform_int_distribution<size_t> d(0, quotes_.size() - 1);
        return quotes_[d(rng_)];
    }
};

// ============================================================
// 技能：成语接龙
// ============================================================
class IdiomSkill : public Skill {
    std::mt19937 &rng_;
    std::vector<std::string> idioms_ = {
        "一马当先", "先发制人", "人才辈出", "出类拔萃", "萃于一堂", "堂堂正正",
        "正大光明", "明察秋毫", "毫不犹豫", "豫樟之材", "材大难用", "用兵如神",
        "神采飞扬", "扬眉吐气", "气吞山河", "河东狮吼", "吼天喊地", "地久天长",
        "长治久安", "安然无恙", "恙无大小", "小试牛刀", "刀山火海", "海阔天空",
        "空前绝后", "后来居上", "上行下效", "效犬马力", "力挽狂澜", "澜倒波随",
        "随心所欲", "欲盖弥彰", "彰明较著", "著述等身", "身体力行", "行之有效",
        "效死疆场", "场场爆满", "满载而归", "归心似箭", "箭在弦上", "上下其手",
        "手到擒来", "来龙去脉", "脉络分明", "明镜高悬", "悬崖勒马", "马到成功",
        "功成名就", "就地取材"};

  public:
    explicit IdiomSkill(std::mt19937 &rng) : rng_(rng) {}
    std::string name() const override { return "成语接龙"; }

    bool matches(const std::string &input) const override {
        return input.find("成语接龙") != std::string::npos;
    }

    std::string execute(const std::string &input) override {
        // 抠出用户给的成语
        std::string user;
        size_t p = input.find("成语接龙");
        if (p != std::string::npos) {
            user = StringUtils::trim(input.substr(p + 12));
        }
        if (user.empty()) {
            std::uniform_int_distribution<size_t> d(0, idioms_.size() - 1);
            std::string start = idioms_[d(rng_)];
            return "我先出：" + start + "。请接（首字需为\"" + getLast(start) +
                   "\"）";
        }
        std::string last = getLast(user);
        if (last.empty()) return "无法识别你给的成语";

        std::vector<std::string> candidates;
        for (const auto &id : idioms_) {
            if (getFirst(id) == last && id != user) candidates.push_back(id);
        }
        if (candidates.empty()) return "接不上了，你赢！";
        std::uniform_int_distribution<size_t> d(0, candidates.size() - 1);
        std::string picked = candidates[d(rng_)];
        return "我接：" + picked + "（首字\"" + getFirst(picked) + "\"）";
    }

  private:
    std::string getFirst(const std::string &s) const {
        auto cs = UTF8Utils::splitToChars(s);
        return cs.empty() ? "" : cs.front();
    }
    std::string getLast(const std::string &s) const {
        auto cs = UTF8Utils::splitToChars(s);
        return cs.empty() ? "" : cs.back();
    }
};
// ============================================================
// 7. ChatBot
// ============================================================
class ChatBot {
  public:
    // ---- 数据 ----
    std::unordered_map<std::string, std::vector<std::string>> kb_;
    std::unordered_map<std::string, std::vector<std::string>> invertedIndex_;
    std::unordered_map<std::string, size_t> answerRotation_;
    Segmenter segmenter_;
    Config config_;
    std::mt19937 rng_{std::random_device{}()};

    // ---- 上下文 ----
    std::deque<std::string> contextKeys_;
    static constexpr size_t MAX_CONTEXT = 3;

    // ---- 主动学习状态 ----
    bool awaitingAnswer_ = false;
    std::string pendingQuestion_;
    std::deque<std::string> declinedQuestions_;
    static constexpr size_t MAX_DECLINED = 20;

    // ---- 调试 ----
    bool debugMode_ = false;

    // ---- 技能 ----
    std::vector<std::shared_ptr<Skill>> skills_;
    std::shared_ptr<TimerSkill> timerSkill_;

    static constexpr int MATCH_THRESHOLD = 5;
    static constexpr double CORRECT_RATIO = 0.33;

    // ========================================================
    // 初始化 / 重载配置
    // ========================================================
    bool init(const std::string &configDir = "config") {
        config_.ensureFilesExist(configDir);
        config_.loadFromDir(configDir);
        return true;
    }

    void reloadConfig(const std::string &configDir = "config") {
        config_.loadFromDir(configDir);
        rebuildIndex();
    }

    // ========================================================
    // 工具
    // ========================================================
    std::string randomFrom(const std::vector<std::string> &v) {
        if (v.empty()) return "";
        std::uniform_int_distribution<size_t> d(0, v.size() - 1);
        return v[d(rng_)];
    }

    std::string pickAnswer(const std::string &key) {
        auto it = kb_.find(key);
        if (it == kb_.end() || it->second.empty()) return "";
        auto &answers = it->second;
        size_t &idx = answerRotation_[key];
        std::string result = answers[idx % answers.size()];
        idx++;
        return result;
    }

    static bool isUsableToken(const std::string &t) {
        if (t.empty()) return false;
        if (t.size() == 1) {
            unsigned char c = (unsigned char)t[0];
            if (c < 0x80 && !std::isalnum(c)) return false;
        }
        return true;
    }

    bool isGibberish(const std::string &s) const {
        int run = 0;
        for (char c : s) {
            if ((unsigned char)c < 0x80 && std::isalpha((unsigned char)c)) {
                if (++run > 20) return true;
            } else {
                run = 0;
            }
        }
        return false;
    }

    // ---- 同义词应用 ----
    std::string applySynonyms(const std::string &s) const {
        std::string result = s;
        for (const auto &group : config_.synonyms()) {
            if (group.empty()) continue;
            const std::string &canonical = group[0];
            for (size_t i = 1; i < group.size(); i++) {
                size_t pos = 0;
                while ((pos = result.find(group[i], pos)) !=
                       std::string::npos) {
                    result.replace(pos, group[i].size(), canonical);
                    pos += canonical.size();
                }
            }
        }
        return result;
    }

    // ---- 归一化 ----
    std::string normalizeQuestion(const std::string &q) const {
        std::string t = StringUtils::trim(q);
        t = CTC::RCPT(t);
        t = applySynonyms(t);

        std::string cleaned;
        cleaned.reserve(t.size());
        for (char c : t) {
            unsigned char uc = (unsigned char)c;
            if (uc < 0x80) {
                if (std::isspace(uc) || std::ispunct(uc)) continue;
            }
            cleaned += c;
        }

        // 语序归一化：什么是X → X是什么
        const std::string prefix = "什么是";
        if (cleaned.rfind(prefix, 0) == 0 && cleaned.size() > prefix.size()) {
            cleaned = cleaned.substr(prefix.size()) + "是什么";
        }

        return StringUtils::toLowerAscii(cleaned);
    }

    std::vector<std::string>
    extractKeywords(const std::string &question) const {
        std::vector<std::string> result;
        for (const auto &t : segmenter_.segment(question)) {
            if (config_.stopwordsSet().count(t)) continue;
            if (!isUsableToken(t)) continue;
            result.push_back(t);
        }
        return result;
    }

    void rebuildIndex() {
        invertedIndex_.clear();
        for (const auto &kv : kb_) {
            for (const auto &kw : extractKeywords(kv.first)) {
                auto &vec = invertedIndex_[kw];
                if (std::find(vec.begin(), vec.end(), kv.first) == vec.end()) {
                    vec.push_back(kv.first);
                }
            }
        }
    }

    // ========================================================
    // 上下文
    // ========================================================
    void pushContext(const std::string &key) {
        auto it = std::find(contextKeys_.begin(), contextKeys_.end(), key);
        if (it != contextKeys_.end()) contextKeys_.erase(it);
        contextKeys_.push_back(key);
        while (contextKeys_.size() > MAX_CONTEXT) contextKeys_.pop_front();
    }

    bool containsPronoun(const std::string &key) const {
        for (const auto &p : config_.pronouns()) {
            if (key.find(p) != std::string::npos) return true;
        }
        return false;
    }

    bool isChatRole(const std::string &t) const {
        return config_.chatRolesSet().count(t) > 0;
    }

    std::string extractTopic(const std::string &questionKey) const {
        for (const auto &t : segmenter_.segment(questionKey)) {
            if (!isUsableToken(t)) continue;
            if (config_.stopwordsSet().count(t)) continue;
            if (isChatRole(t)) continue;
            if (config_.verbBlacklistSet().count(t)) continue;
            return t;
        }
        return "";
    }

    std::string resolvePronoun(const std::string &key) const {
        for (auto it = contextKeys_.rbegin(); it != contextKeys_.rend(); ++it) {
            std::string topic = extractTopic(*it);
            if (topic.empty()) continue;
            std::string result = key;
            for (const auto &p : config_.pronouns()) {
                size_t pos = 0;
                while ((pos = result.find(p, pos)) != std::string::npos) {
                    result.replace(pos, p.size(), topic);
                    pos += topic.size();
                }
            }
            if (result != key) return result;
        }
        return key;
    }

    // ========================================================
    // 训练 / 删除 / 导入 / 导出
    // ========================================================
    void train(const std::string &rawInput) {
        size_t pos = rawInput.find('|');
        if (pos == std::string::npos) {
            std::cout << "[系统] 格式错误。请使用格式：问题|回答\n";
            return;
        }
        std::string questionRaw = StringUtils::trim(rawInput.substr(0, pos));
        std::string answerRaw = StringUtils::trim(rawInput.substr(pos + 1));
        if (questionRaw.empty() || answerRaw.empty()) {
            std::cout << "[系统] 问题或回答不能为空。\n";
            return;
        }
        std::string key = normalizeQuestion(questionRaw);

        auto it = kb_.find(key);
        if (it != kb_.end()) {
            it->second.push_back(answerRaw);
            std::cout << "[系统] 已为问题 '" << questionRaw
                      << "' 添加新的回答变体。\n";
        } else {
            kb_[key] = {answerRaw};
            std::cout << "[系统] 学习成功！我记住了关于 '" << questionRaw
                      << "' 的知识。\n";
        }
        for (const auto &kw : extractKeywords(key)) {
            auto &vec = invertedIndex_[kw];
            if (std::find(vec.begin(), vec.end(), key) == vec.end()) {
                vec.push_back(key);
            }
        }
    }

    bool forget(const std::string &question) {
        std::string key = normalizeQuestion(question);
        auto it = kb_.find(key);
        if (it == kb_.end()) return false;
        kb_.erase(it);
        answerRotation_.erase(key);
        rebuildIndex();
        return true;
    }

    int importFromFile(const std::string &path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return -1;
        std::string line;
        int count = 0;
        while (std::getline(f, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            size_t pos = line.find('|');
            if (pos == std::string::npos) continue;
            std::string q = StringUtils::trim(line.substr(0, pos));
            std::string a = StringUtils::trim(line.substr(pos + 1));
            if (q.empty() || a.empty()) continue;
            std::string key = normalizeQuestion(q);
            if (kb_.find(key) == kb_.end()) {
                kb_[key] = {a};
            } else {
                kb_[key].push_back(a);
            }
            count++;
        }
        rebuildIndex();
        return count;
    }

    bool exportToFile(const std::string &path) const {
        std::ofstream f(path, std::ios::binary);
        if (!f) return false;
        for (const auto &kv : kb_) {
            for (const auto &a : kv.second) {
                f << kv.first << "|" << a << "\n";
            }
        }
        return true;
    }

    // ========================================================
    // 匹配
    // ========================================================
    const std::vector<std::string> *exactMatch(const std::string &key) const {
        auto it = kb_.find(key);
        return it == kb_.end() ? nullptr : &it->second;
    }

    static int editDistance(const std::vector<std::string> &a,
                            const std::vector<std::string> &b) {
        int m = (int)a.size(), n = (int)b.size();
        std::vector<int> prev((size_t)n + 1), cur((size_t)n + 1);
        for (int j = 0; j <= n; j++) prev[(size_t)j] = j;
        for (int i = 1; i <= m; i++) {
            cur[0] = i;
            for (int j = 1; j <= n; j++) {
                if (a[(size_t)i - 1] == b[(size_t)j - 1])
                    cur[(size_t)j] = prev[(size_t)j - 1];
                else
                    cur[(size_t)j] =
                        std::min({prev[(size_t)j], cur[(size_t)j - 1],
                                  prev[(size_t)j - 1]}) +
                        1;
            }
            std::swap(prev, cur);
        }
        return prev[(size_t)n];
    }

    std::string fuzzyCorrect(const std::string &key) const {
        auto a = UTF8Utils::splitToChars(key);
        if (a.size() < 2) return "";
        std::string best;
        int bestDist = 999;
        double bestRatio = 2.0;
        for (const auto &kv : kb_) {
            auto b = UTF8Utils::splitToChars(kv.first);
            if (b.empty()) continue;
            if (a.size() > b.size()) continue;
            size_t maxLen = std::max(a.size(), b.size());
            if (maxLen == 0) continue;
            int diff = (int)a.size() - (int)b.size();
            if (diff < 0) diff = -diff;
            if ((double)diff / (double)maxLen > CORRECT_RATIO) continue;
            int d = editDistance(a, b);
            if (d == 0) continue;
            double ratio = (double)d / (double)maxLen;
            if (ratio <= CORRECT_RATIO) {
                if (d < bestDist || (d == bestDist && ratio < bestRatio)) {
                    bestDist = d;
                    bestRatio = ratio;
                    best = kv.first;
                }
            }
        }
        return best;
    }

    int scoreAgainst(const std::string &q, const std::string &key,
                     const std::vector<std::string> &tokens) const {
        int score = 0;
        for (const auto &t : tokens) {
            if (t.empty()) continue;
            if (config_.stopwordsSet().count(t)) continue;
            if (q.find(t) != std::string::npos) {
                size_t tc = UTF8Utils::charCount(t);
                if (tc >= 2)
                    score += 3;
                else {
                    unsigned char c = (unsigned char)t[0];
                    score += (c >= 0x80) ? 3 : 1;
                }
            }
        }
        // 时间词一致加分
        for (const auto &tw : config_.timeWords()) {
            if (key.find(tw) != std::string::npos &&
                q.find(tw) != std::string::npos) {
                score += 5;
            }
        }
        // 上下文加分（仅长输入）
        if (UTF8Utils::charCount(key) >= 4) {
            for (const auto &ctx : contextKeys_) {
                if (ctx == q) {
                    score += 3;
                    break;
                }
            }
        }
        // 未命中惩罚
        bool hasTimeWord = false;
        for (const auto &tw : config_.timeWords()) {
            if (key.find(tw) != std::string::npos) {
                hasTimeWord = true;
                break;
            }
        }
        if (!hasTimeWord) {
            for (const auto &t : tokens) {
                if (t.empty()) continue;
                if (config_.stopwordsSet().count(t)) continue;
                if (!isUsableToken(t)) continue;
                if (q.find(t) == std::string::npos) {
                    score -= 5;
                }
            }
        }
        return score;
    }

    // ========================================================
    // 主动学习
    // ========================================================
    bool isDeclined(const std::string &normalizedKey) const {
        for (const auto &d : declinedQuestions_) {
            if (d == normalizedKey) return true;
        }
        return false;
    }

    bool shouldOfferLearning(const std::string &rawInput,
                             const std::string &normalizedKey) const {
        if (UTF8Utils::charCount(rawInput) < 2) return false;
        if (isGibberish(rawInput)) return false;
        if (isDeclined(normalizedKey)) return false;
        bool hasToken = false;
        for (const auto &t : segmenter_.segment(normalizedKey)) {
            if (isUsableToken(t) && !config_.stopwordsSet().count(t)) {
                hasToken = true;
                break;
            }
        }
        return hasToken;
    }

    void acceptPendingAnswer(const std::string &answer) {
        if (pendingQuestion_.empty()) {
            clearPending();
            return;
        }
        train(pendingQuestion_ + "|" + answer);
        clearPending();
    }

    void declinePending() {
        if (!pendingQuestion_.empty()) {
            std::string key = normalizeQuestion(pendingQuestion_);
            declinedQuestions_.push_back(key);
            while (declinedQuestions_.size() > MAX_DECLINED)
                declinedQuestions_.pop_front();
        }
        clearPending();
    }

    void clearPending() {
        awaitingAnswer_ = false;
        pendingQuestion_.clear();
    }

    // ========================================================
    // 对话主入口
    // ========================================================
    ChatResult chat(const std::string &rawInput) {
        ChatResult result;

        // 输入长度限制
        if (UTF8Utils::charCount(rawInput) > 200) {
            result.reply = "输入太长了，请精简一下。";
            return result;
        }
        // 乱码保护
        if (isGibberish(rawInput)) {
            result.reply = randomFrom(config_.defaultResponses());
            return result;
        }

        std::string trimmedRaw = StringUtils::trim(rawInput);

        // 0) 技能优先（用原始输入）
        for (auto &s : skills_) {
            if (s->matches(trimmedRaw)) {
                if (debugMode_) {
                    std::cout << "[DEBUG] 技能命中: " << s->name() << "\n";
                }
                result.reply = "[" + s->name() + "] " + s->execute(trimmedRaw);
                return result;
            }
        }

        std::string key = normalizeQuestion(rawInput);
        if (key.empty()) {
            result.reply = "你好！有什么我可以帮你的吗？";
            return result;
        }

        // 单字 chatrole 直接兜底
        if (UTF8Utils::charCount(key) == 1 && isChatRole(key)) {
            result.reply = randomFrom(config_.defaultResponses());
            return result;
        }

        std::string effectiveKey = key;
        bool fromContext = false;
        if (containsPronoun(key) && !contextKeys_.empty()) {
            std::string resolved = resolvePronoun(key);
            if (resolved != key && !resolved.empty()) {
                effectiveKey = resolved;
                fromContext = true;
            }
        }
        const std::string prefix = fromContext ? "[上下文·延续] " : "";

        // 1) 精确
        if (exactMatch(effectiveKey)) {
            if (debugMode_)
                std::cout << "[DEBUG] 精确匹配: " << effectiveKey << "\n";
            pushContext(effectiveKey);
            result.reply = prefix + pickAnswer(effectiveKey);
            return result;
        }

        // 2) 模糊纠错
        {
            std::string corrected = fuzzyCorrect(effectiveKey);
            if (!corrected.empty()) {
                if (debugMode_) {
                    auto a = UTF8Utils::splitToChars(effectiveKey);
                    auto b = UTF8Utils::splitToChars(corrected);
                    int d = editDistance(a, b);
                    size_t maxLen = std::max(a.size(), b.size());
                    std::cout << "[DEBUG] 纠错命中: " << effectiveKey << " -> "
                              << corrected << " (dist=" << d
                              << ", maxLen=" << maxLen
                              << ", ratio=" << (double)d / maxLen << ")\n";
                }
                pushContext(corrected);
                std::string pre =
                    fromContext ? "[上下文·延续] " : "[已为你修正输入] ";
                result.reply = pre + pickAnswer(corrected);
                return result;
            }
        }

        // 3) 关键词评分
        auto tokens = segmenter_.segment(effectiveKey);
        std::vector<std::string> filtered;
        for (const auto &t : tokens)
            if (isUsableToken(t)) filtered.push_back(t);
        if (filtered.empty()) filtered.push_back(effectiveKey);

        size_t eLen = UTF8Utils::charCount(effectiveKey);
        int threshold = MATCH_THRESHOLD;
        if (eLen == 1) threshold = 3;

        std::vector<std::pair<std::string, int>> ranked;
        for (const auto &kv : kb_) {
            int s = scoreAgainst(kv.first, effectiveKey, filtered);
            if (eLen >= 2 &&
                (kv.first.find(effectiveKey) != std::string::npos ||
                 effectiveKey.find(kv.first) != std::string::npos)) {
                s += 10;
            }
            ranked.push_back({kv.first, s});
        }
        std::sort(ranked.begin(), ranked.end(),
                  [](const std::pair<std::string, int> &a,
                     const std::pair<std::string, int> &b) {
                      if (a.second != b.second) return a.second > b.second;
                      return UTF8Utils::charCount(a.first) <
                             UTF8Utils::charCount(b.first);
                  });

        if (debugMode_ && !ranked.empty()) {
            std::cout << "[DEBUG] 关键词评分 Top5:\n";
            size_t n = std::min<size_t>(5, ranked.size());
            for (size_t i = 0; i < n; i++) {
                std::cout << "  [" << i << "] " << ranked[i].second << " 分  "
                          << ranked[i].first << "\n";
            }
        }

        int maxScore = ranked.empty() ? 0 : ranked[0].second;

        if (maxScore >= threshold) {
            // 候选列表
            int secondScore = ranked.size() >= 2 ? ranked[1].second : 0;
            if (eLen >= 2 && secondScore > 0 && maxScore - secondScore <= 2 &&
                ranked[0].first != ranked[1].first) {
                std::ostringstream oss;
                oss << "我想到多个相关内容：\n";
                size_t show = std::min<size_t>(3, ranked.size());
                for (size_t i = 0; i < show; i++) {
                    // 只显示分数 >= maxScore - 3 的候选
                    if (ranked[i].second < maxScore - 3) break;
                    oss << "  · " << ranked[i].first << "："
                        << kb_[ranked[i].first][0] << "\n";
                }
                oss << "你可以再具体一点，或直接问其中一个。";
                result.reply = oss.str();
                return result;
            }

            // 单字查询：选最短的
            std::string bestKey = ranked[0].first;
            if (eLen == 1) {
                size_t bestLen = UTF8Utils::charCount(bestKey);
                for (const auto &r : ranked) {
                    if (r.second != maxScore) break;
                    size_t l = UTF8Utils::charCount(r.first);
                    if (l < bestLen) {
                        bestLen = l;
                        bestKey = r.first;
                    }
                }
            }

            if (debugMode_) {
                std::cout << "[DEBUG] 命中: " << bestKey
                          << " (分数=" << maxScore << ", 阈值=" << threshold
                          << ")\n";
            }
            pushContext(bestKey);
            result.reply = prefix + pickAnswer(bestKey);
            return result;
        }

        // 4) 多关键词推理
        {
            std::vector<std::string> kws;
            std::vector<std::string> singleCharKws;
            bool hasMultiToken = false;
            for (const auto &t : filtered) {
                if (UTF8Utils::charCount(t) >= 2) hasMultiToken = true;
                if (config_.stopwordsSet().count(t)) continue;
                if (UTF8Utils::charCount(t) < 2)
                    singleCharKws.push_back(t);
                else
                    kws.push_back(t);
            }
            // 只有整个输入完全没有多字 token 时，才退化为单字
            // 且至少 3 个单字，避免"枇杷"这种 2 字误触发
            if (!hasMultiToken) {
                kws = singleCharKws;
                if (kws.size() < 3) kws.clear();
            }
            if (kws.size() >= 2) {
                std::unordered_map<std::string, int> cnt;
                std::unordered_map<std::string, std::unordered_set<std::string>>
                    hitKws;
                for (const auto &kw : kws) {
                    auto it = invertedIndex_.find(kw);
                    if (it == invertedIndex_.end()) continue;
                    int w = 2;
                    for (const auto &q : it->second) {
                        cnt[q] += w;
                        hitKws[q].insert(kw);
                    }
                }
                int best = 0;
                std::string bestQ;
                for (const auto &kv : cnt) {
                    if (hitKws[kv.first].size() < 2) continue;
                    if (kv.second > best) {
                        best = kv.second;
                        bestQ = kv.first;
                    }
                }
                if (best >= 2 && !bestQ.empty()) {
                    if (debugMode_) {
                        std::cout << "[DEBUG] 多关键词推理: " << bestQ
                                  << " (分数=" << best
                                  << ", 命中关键词数=" << hitKws[bestQ].size()
                                  << ")\n";
                    }
                    pushContext(bestQ);
                    result.reply = prefix + "[推理匹配] " + pickAnswer(bestQ);
                    return result;
                }
            }
        }

        // 5) 主动学习
        if (shouldOfferLearning(rawInput, key)) {
            awaitingAnswer_ = true;
            pendingQuestion_ = rawInput;
            result.awaitingAnswer = true;
            result.pendingQuestion = rawInput;
            result.reply = "我没学过\"" + rawInput +
                           "\"，要不要教我？\n"
                           "直接输入答案，或输入 skip 跳过。";
            return result;
        }

        // 6) 兜底
        if (debugMode_) std::cout << "[DEBUG] 走兜底\n";
        result.reply = prefix.empty() ? randomFrom(config_.defaultResponses())
                                      : "我还在学习中，能换个方式问吗？";
        return result;
    }

    // ========================================================
    // 文章抽取
    // ========================================================
    std::string stripConnector(const std::string &s) const {
        for (const auto &c : config_.connectors()) {
            if (s.rfind(c, 0) == 0) {
                return StringUtils::trim(s.substr(c.size()));
            }
        }
        return s;
    }

    std::string stripQuantifier(const std::string &s) const {
        for (const auto &q : config_.quantifiers()) {
            if (s.rfind(q, 0) == 0) {
                return StringUtils::trim(s.substr(q.size()));
            }
        }
        return s;
    }

    bool isCleanPhrase(const std::string &s, size_t minLen,
                       size_t maxLen) const {
        size_t len = UTF8Utils::charCount(s);
        if (len < minLen || len > maxLen) return false;
        for (const auto &ch : UTF8Utils::splitToChars(s)) {
            if (ch == "。" || ch == "！" || ch == "？" || ch == "." ||
                ch == "!" || ch == "?")
                return false;
        }
        if (isChatRole(s)) return false;
        if (config_.stopwordsSet().count(s)) return false;
        return true;
    }

    std::string formatTemplate(const std::string &tpl, const std::string &S,
                               const std::string &O) const {
        std::string r = tpl;
        size_t p;
        p = r.find("{S}");
        if (p != std::string::npos) r.replace(p, 3, S);
        p = r.find("{O}");
        if (p != std::string::npos) r.replace(p, 3, O);
        return r;
    }

    void parseSentence(const std::string &sentence,
                       std::vector<ExtractItem> &out) const {
        std::string s = StringUtils::trim(sentence);
        // 去末尾标点
        while (!s.empty()) {
            auto chars = UTF8Utils::splitToChars(s);
            if (chars.empty()) break;
            const std::string &last = chars.back();
            if (last == "。" || last == "！" || last == "？" || last == "." ||
                last == "!" || last == "?") {
                s.erase(s.size() - last.size());
            } else
                break;
        }
        s = StringUtils::trim(s);
        if (s.empty()) return;

        // 因为...所以...
        {
            size_t p1 = s.find("因为");
            size_t p2 = (p1 == std::string::npos) ? std::string::npos
                                                  : s.find("所以", p1);
            if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
                std::string cause =
                    StringUtils::trim(s.substr(p1 + 6, p2 - p1 - 6));
                std::string effect = StringUtils::trim(s.substr(p2 + 6));
                cause = stripConnector(cause);
                effect = stripConnector(effect);

                // 去掉尾部逗号、分号
                auto stripTail = [](std::string &x) {
                    while (!x.empty()) {
                        auto cs = UTF8Utils::splitToChars(x);
                        if (cs.empty()) break;
                        const std::string &last = cs.back();
                        if (last == "，" || last == "," || last == "；" ||
                            last == ";") {
                            x.erase(x.size() - last.size());
                            x = StringUtils::trim(x);
                        } else
                            break;
                    }
                };
                stripTail(cause);
                stripTail(effect);

                if (isCleanPhrase(cause, 2, 30) &&
                    isCleanPhrase(effect, 2, 30)) {
                    ExtractItem ei;
                    ei.question = "为什么" + effect + "？";
                    ei.answer = "因为" + cause + "，所以" + effect;
                    out.push_back(ei);
                }
            }
        }

        // 收集所有关系词，按字节长度降序
        struct Cand {
            const std::string *kw;
            const RelationRule *rule;
        };
        std::vector<Cand> cands;
        for (const auto &rule : config_.relations()) {
            for (const auto &kw : rule.keywords) {
                cands.push_back({&kw, &rule});
            }
        }
        std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
            return a.kw->size() > b.kw->size();
        });

        for (const auto &cand : cands) {
            const std::string &kw = *cand.kw;
            size_t pos = s.find(kw);
            if (pos == std::string::npos) continue;
            if (pos == 0) continue;

            std::string left = StringUtils::trim(s.substr(0, pos));
            std::string right = StringUtils::trim(s.substr(pos + kw.size()));

            if (left.find("，") != std::string::npos) continue;
            if (left.find(",") != std::string::npos) continue;

            left = stripConnector(left);
            if (!isCleanPhrase(left, 1, 15)) continue;

            for (const char *sep : {"，", "；", ",", ";"}) {
                size_t p = right.find(sep);
                if (p != std::string::npos) {
                    right = StringUtils::trim(right.substr(0, p));
                }
            }
            if (!isCleanPhrase(right, 1, 40)) continue;

            std::string rightClean = stripQuantifier(right);
            std::string answer = left + kw + right;

            if (!cand.rule->forwardTemplate.empty()) {
                ExtractItem fi;
                fi.question = formatTemplate(cand.rule->forwardTemplate, left,
                                             rightClean);
                fi.answer = answer;
                out.push_back(fi);
            }
            if (!cand.rule->backwardTemplate.empty()) {
                ExtractItem bi;
                bi.question = formatTemplate(cand.rule->backwardTemplate, left,
                                             rightClean);
                bi.answer = answer;
                out.push_back(bi);
            }
            return;
        }
    }

    void extractArticle(const std::string &article,
                        std::vector<ExtractItem> &items,
                        ArticleStats &stats) const {
        std::wstring w = UTF8Utils::toWString(article);
        std::wstring cur;
        std::vector<std::string> sentences;
        for (wchar_t c : w) {
            cur.push_back(c);
            if (c == L'。' || c == L'！' || c == L'？' || c == L'.' ||
                c == L'!' || c == L'?' || c == L'\n') {
                if (!cur.empty()) {
                    sentences.push_back(UTF8Utils::toUTF8(cur));
                    cur.clear();
                }
            }
        }
        if (!cur.empty()) sentences.push_back(UTF8Utils::toUTF8(cur));

        stats.sentencesScanned = (int)sentences.size();

        std::unordered_set<std::string> seen;
        for (const auto &s : sentences) {
            std::vector<ExtractItem> local;
            parseSentence(s, local);
            if (!local.empty()) stats.sentencesMatched++;
            for (auto &item : local) {
                stats.itemsGenerated++;
                std::string nk = normalizeQuestion(item.question);
                std::string dedupKey = nk + "|" + item.answer;
                if (seen.count(dedupKey)) {
                    stats.itemsDuplicate++;
                    continue;
                }
                seen.insert(dedupKey);
                items.push_back(item);
            }
        }
    }

    void commitExtraction(const std::vector<ExtractItem> &items,
                          ArticleStats &stats) {
        for (const auto &item : items) {
            std::string key = normalizeQuestion(item.question);
            auto it = kb_.find(key);
            if (it == kb_.end()) {
                kb_[key] = {item.answer};
                stats.newQuestions++;
                stats.itemsTrained++;
                for (const auto &kw : extractKeywords(key)) {
                    auto &vec = invertedIndex_[kw];
                    if (std::find(vec.begin(), vec.end(), key) == vec.end()) {
                        vec.push_back(key);
                    }
                }
            } else {
                bool dup = false;
                for (const auto &a : it->second) {
                    if (a == item.answer) {
                        dup = true;
                        break;
                    }
                }
                if (dup) {
                    stats.itemsDuplicate++;
                } else {
                    it->second.push_back(item.answer);
                    stats.newVariants++;
                    stats.itemsTrained++;
                }
            }
        }
    }

    // ========================================================
    // 技能
    // ========================================================
    void registerSkills() {
        skills_.clear();
        skills_.push_back(std::make_shared<TimeSkill>());
        skills_.push_back(std::make_shared<DiceSkill>(rng_));
        skills_.push_back(std::make_shared<RpsSkill>(rng_));
        timerSkill_ = std::make_shared<TimerSkill>();
        skills_.push_back(timerSkill_);
        skills_.push_back(std::make_shared<CalcSkill>());
        skills_.push_back(std::make_shared<RandomSkill>(rng_));
        skills_.push_back(std::make_shared<CoinSkill>(rng_));
        skills_.push_back(std::make_shared<FoodSkill>(rng_));
        skills_.push_back(std::make_shared<CharCountSkill>());
        skills_.push_back(std::make_shared<UnitSkill>());
        skills_.push_back(std::make_shared<GuessSkill>(rng_));
        skills_.push_back(std::make_shared<PoemSkill>(rng_));
        skills_.push_back(std::make_shared<QuoteSkill>(rng_));
        skills_.push_back(std::make_shared<IdiomSkill>(rng_));
    }

    std::string pollTimer() {
        if (!timerSkill_) return "";
        std::string msg;
        if (timerSkill_->poll(msg)) return msg;
        return "";
    }

    // ========================================================
    // 持久化
    // ========================================================
    bool save(const std::string &path) const {
        std::string tmp = path + ".tmp";
        std::ofstream f(tmp, std::ios::binary);
        if (!f) {
            std::cout << "[错误] 无法写入 " << tmp << "\n";
            return false;
        }
        for (const auto &kv : kb_) {
            const std::string &q = kv.first;
            const auto &ans = kv.second;
            f << "Q " << q.size() << "\n";
            f.write(q.data(), (std::streamsize)q.size());
            f << "\n";
            f << ans.size() << "\n";
            for (const auto &a : ans) {
                f << "A " << a.size() << "\n";
                f.write(a.data(), (std::streamsize)a.size());
                f << "\n";
            }
        }
        f.close();
        if (!MoveFileExA(tmp.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING)) {
            std::remove(tmp.c_str());
            std::cout << "[错误] 替换 " << path << " 失败\n";
            return false;
        }
        return true;
    }

    bool load(const std::string &path) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        kb_.clear();
        std::string line;
        bool anyRead = false;
        while (std::getline(f, line)) {
            if (line.size() < 2 || line[0] != 'Q' || line[1] != ' ') break;
            size_t qlen = 0;
            try {
                qlen = std::stoul(line.substr(2));
            } catch (...) {
                break;
            }
            if (qlen == 0) break;
            std::string question(qlen, '\0');
            if (!f.read(&question[0], (std::streamsize)qlen)) break;
            f.ignore(1);
            if (!std::getline(f, line)) break;
            int count = 0;
            try {
                count = std::stoi(line);
            } catch (...) {
                break;
            }
            if (count < 0) break;
            std::vector<std::string> answers;
            bool ok = true;
            for (int i = 0; i < count; ++i) {
                if (!std::getline(f, line)) {
                    ok = false;
                    break;
                }
                if (line.size() < 2 || line[0] != 'A' || line[1] != ' ') {
                    ok = false;
                    break;
                }
                size_t alen = 0;
                try {
                    alen = std::stoul(line.substr(2));
                } catch (...) {
                    ok = false;
                    break;
                }
                if (alen == 0) {
                    ok = false;
                    break;
                }
                std::string a(alen, '\0');
                if (!f.read(&a[0], (std::streamsize)alen)) {
                    ok = false;
                    break;
                }
                f.ignore(1);
                answers.push_back(std::move(a));
            }
            if (!ok) break;
            std::string normalizedKey = normalizeQuestion(question);
            auto exist = kb_.find(normalizedKey);
            if (exist != kb_.end()) {
                for (auto &a : answers) exist->second.push_back(a);
            } else {
                kb_[normalizedKey] = std::move(answers);
            }
            anyRead = true;
        }
        rebuildIndex();
        return anyRead;
    }

    void showStats() const {
        std::cout << "[状态] 已学习问题类别数: " << kb_.size()
                  << "，倒排索引关键词数: " << invertedIndex_.size()
                  << "，上下文缓存: " << contextKeys_.size()
                  << "，Debug: " << (debugMode_ ? "ON" : "OFF") << "\n";
    }
};

// ============================================================
// 8. 交互界面
// ============================================================
void printMenu() {
    system("cls");
    std::cout << "\n========================================\n";
    std::cout << "   欢迎使用 C++ 离线 AI 聊天机器人\n";
    std::cout << "========================================\n";
    std::cout << "请选择模式：\n";
    std::cout << "1. 对话训练模式 (输入: 1 或 train)\n";
    std::cout << "   格式: 问题|回答\n";
    std::cout << "2. 文章训练模式 (输入: 2 或 article)\n";
    std::cout << "3. 对话模式     (输入: 3 或 chat)\n";
    std::cout << "4. 退出         (输入: 4 或 exit)\n";
    std::cout << "5. 词库管理     (输入: 5 或 dict)\n";
    std::cout << "========================================\n";
    std::cout << "模式内: /back /exit /save /stats /debug /reload\n";
    std::cout << "额外:   /forget <问题>  /import <文件>  /export <文件>\n";
}

int main() {
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleTitleW(L"C++ 离线 AI 聊天机器人");
    setConsoleFont(L"Consolas", 20);

    ChatBot bot;
    bot.init("config");
    bot.registerSkills();
    system("cls");
    std::cout << "[系统] 词典: "
              << (bot.segmenter_.loadDict("dict.dat") ? "已加载" : "未找到")
              << "\n";
    std::cout << "[系统] 知识库: " << (bot.load("data.dat") ? "已加载" : "空")
              << "\n";
    std::cout << "[系统] 配置: config/ 已就绪\n";
    std::cout << "[状态] 问题: " << bot.kb_.size()
              << "，关键词: " << bot.invertedIndex_.size() << "\n";

    std::cout << "\n按回车进入主菜单...";
    std::cout.flush();
    UTF8Utils::readLineSafe();

    std::string mode;
    while (true) {
        if (mode.empty()) {
            printMenu();
            std::cout << "> 请输入模式选择: ";
            std::string line = UTF8Utils::readLineSafe();
            if (line.empty() && std::cin.eof()) break;
            std::string s = StringUtils::toLowerAscii(StringUtils::trim(line));
            if (s == "1" || s == "train") {
                mode = "train";
            } else if (s == "2" || s == "article") {
                mode = "article";
            } else if (s == "3" || s == "chat") {
                mode = "chat";
            } else if (s == "4" || s == "exit") {
                bot.save("data.dat");
                std::cout << "再见！\n";
                break;
            } else if (s == "5" || s == "dict") {
                std::cout << "[系统] 正在启动词库管理工具...\n";
                if (GetFileAttributesA("dict.exe") == INVALID_FILE_ATTRIBUTES) {
                    std::cout << "[错误] 找不到 dict.exe，"
                                 "请把它放在主程序同目录。\n";
                    std::this_thread::sleep_for(
                        std::chrono::milliseconds(1500));
                } else {
                    int ret = system("dict.exe");
                    if (ret == -1) {
                        std::cout << "[错误] 启动 dict.exe 失败。\n";
                    } else {
                        std::cout << "[系统] 词库管理已退出，"
                                     "正在重新加载词典...\n";
                        bot.segmenter_ = Segmenter();
                        if (bot.segmenter_.loadDict("dict.dat")) {
                            std::cout << "[系统] 词库已重新加载。\n";
                        } else {
                            std::cout << "[警告] 重新加载 dict.dat 失败。\n";
                        }
                        bot.rebuildIndex();
                        bot.reloadConfig("config");
                        std::cout << "[系统] 配置已重新加载。\n";
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(800));
                }
                continue;
            } else {
                std::cout << "[错误] 无效的模式，请输入 1/2/3/4/5 或 "
                             "train/article/chat/exit/dict。\n";
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                continue;
            }

            if (mode == "train") {
                std::cout << "[进入训练模式] 输入 '问题|回答' 进行训练。\n";
            } else if (mode == "chat") {
                std::cout << "[进入对话模式]\n";
                bot.showStats();
            } else {
                std::cout << "[进入文章模式] 请按回车进入粘贴模式。\n";
            }
            continue;
        }

        // 倒计时提醒（等用户输入前先检查一次）
        {
            std::string tm = bot.pollTimer();
            if (!tm.empty()) std::cout << "AI: " << tm << "\n";
        }

        std::cout << "> ";
        std::string raw = UTF8Utils::readLineSafe();
        if (raw.empty() && std::cin.eof()) break;
        std::string trimmed = StringUtils::trim(raw);

        // ---- 主动学习：待答状态优先 ----
        if (bot.awaitingAnswer_) {
            std::string lower = StringUtils::toLowerAscii(trimmed);
            if (lower == "skip" || lower == "n" || trimmed.empty()) {
                bot.declinePending();
                std::cout << "AI: 好的，下次再说。\n";
                continue;
            }
            if (!trimmed.empty() && trimmed[0] == '/') {
                bot.clearPending();
                // 落入下面的命令处理
            } else {
                std::string q = bot.pendingQuestion_;
                bot.acceptPendingAnswer(trimmed);
                bot.save("data.dat");
                std::cout << "AI: 学会了！以后你问\"" << q << "\"我就知道。\n";
                continue;
            }
        }

        // ---- 命令 ----
        if (trimmed == "/back") {
            mode.clear();
            continue;
        }
        if (trimmed == "/exit") {
            bot.save("data.dat");
            std::cout << "再见！\n";
            break;
        }
        if (trimmed == "/save") {
            bot.save("data.dat");
            std::cout << "[系统] 已保存。\n";
            continue;
        }
        if (trimmed == "/stats") {
            bot.showStats();
            continue;
        }
        if (trimmed == "/debug") {
            bot.debugMode_ = !bot.debugMode_;
            std::cout << "[系统] Debug 模式: "
                      << (bot.debugMode_ ? "ON" : "OFF") << "\n";
            continue;
        }
        if (trimmed == "/reload") {
            bot.reloadConfig("config");
            bot.segmenter_ = Segmenter();
            bot.segmenter_.loadDict("dict.dat");
            bot.rebuildIndex();
            std::cout << "[系统] 配置和词典已重新加载。\n";
            continue;
        }
        if (trimmed.rfind("/forget ", 0) == 0) {
            std::string q = StringUtils::trim(trimmed.substr(8));
            if (q.empty()) {
                std::cout << "[系统] 用法: /forget <问题>\n";
            } else if (bot.forget(q)) {
                bot.save("data.dat");
                std::cout << "[系统] 已删除问题: " << q << "\n";
            } else {
                std::cout << "[系统] 未找到该问题。\n";
            }
            continue;
        }
        if (trimmed.rfind("/import ", 0) == 0) {
            std::string path = StringUtils::trim(trimmed.substr(8));
            int n = bot.importFromFile(path);
            if (n < 0) {
                std::cout << "[系统] 无法打开文件: " << path << "\n";
            } else {
                bot.save("data.dat");
                std::cout << "[系统] 导入 " << n << " 条记录。\n";
            }
            continue;
        }
        if (trimmed.rfind("/export ", 0) == 0) {
            std::string path = StringUtils::trim(trimmed.substr(8));
            if (bot.exportToFile(path)) {
                std::cout << "[系统] 已导出到 " << path << "\n";
            } else {
                std::cout << "[系统] 写入失败: " << path << "\n";
            }
            continue;
        }

        // ---- 按模式处理 ----
        if (mode == "train") {
            if (trimmed.empty()) continue;
            bot.train(trimmed);
            bot.save("data.dat");
        } else if (mode == "chat") {
            if (trimmed.empty()) continue;
            ChatResult r = bot.chat(trimmed);
            std::cout << "AI: " << r.reply << "\n";
        } else if (mode == "article") {
            std::string article;
            bool userBack = false;

            // 用户选 2 后第一行可能直接输入了内容，作为文章开头
            if (!trimmed.empty()) {
                article += trimmed;
                article += "\n";
            }

            std::cout << "\n[文章模式] 粘贴文章内容，"
                         "按 Ctrl+Z 后回车结束，或输入 /end 结束。\n";
            std::cout << "           /back 返回主菜单。\n";
            std::cout << "----------------------------------------\n";

            std::string line;
            while (true) {
                line = UTF8Utils::readLineSafe();
                std::string lt = StringUtils::trim(line);
                if (lt.empty() || lt == "/end") break;
                if (lt == "/back") {
                    userBack = true;
                    break;
                }
                article += line;
                article += "\n";
            }
            if (std::cin.eof()) std::cin.clear();
            if (userBack) {
                mode.clear();
                continue;
            }

            if (article.empty()) {
                std::cout << "[系统] 没有输入内容。\n";
                continue;
            }

            std::vector<ExtractItem> items;
            ArticleStats stats;
            bot.extractArticle(article, items, stats);

            std::cout << "\n[预览] 扫描句子: " << stats.sentencesScanned
                      << "，命中: " << stats.sentencesMatched
                      << "，生成: " << stats.itemsGenerated
                      << "，去重跳过: " << stats.itemsDuplicate << "\n";

            if (items.empty()) {
                std::cout << "[系统] 没有抽到可用知识。\n";
                continue;
            }

            size_t show = std::min<size_t>(10, items.size());
            std::cout << "将训练以下 " << items.size() << " 条（前 " << show
                      << " 条）：\n";
            for (size_t i = 0; i < show; i++) {
                std::cout << "  " << (i + 1) << ". " << items[i].question
                          << " | " << items[i].answer << "\n";
            }
            if (items.size() > show) {
                std::cout << "  ... 还有 " << (items.size() - show) << " 条\n";
            }

            std::cout << "\n确认训练？(y/n): ";
            std::string confirm = UTF8Utils::readLineSafe();
            if (confirm.empty() && std::cin.eof()) break;
            std::string lc =
                StringUtils::toLowerAscii(StringUtils::trim(confirm));
            if (lc != "y" && lc != "yes") {
                std::cout << "[系统] 已取消。\n";
                continue;
            }

            bot.commitExtraction(items, stats);
            bot.save("data.dat");
            std::cout << "\n[报告] 实际训练: " << stats.itemsTrained
                      << "，新增问题: " << stats.newQuestions
                      << "，新增变体: " << stats.newVariants
                      << "，已存在跳过: " << stats.itemsDuplicate << "\n";
        }
    }
    return 0;
}