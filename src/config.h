// ============================================================
// config.h — 配置管理模块（dict.exe / functions.exe 共用）
// C++17
// ============================================================
#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

// ============================================================
// 关系词规则
// ============================================================
struct RelationRule {
    std::vector<std::string> keywords;
    std::string forwardTemplate;
    std::string backwardTemplate;
};

// ============================================================
// Config 类
// ============================================================
class Config {
  public:
    Config() { resetToDefaults(); }

    // ---- 目录 / 文件管理 ----
    void ensureFilesExist(const std::string &dir) const {
        std::error_code ec;
        fs::create_directories(dir, ec);
        ensureFile(dir + "/stopwords.txt", stopwordsVec_);
        ensureFile(dir + "/chatroles.txt", chatRolesVec_);
        ensureFile(dir + "/verbblacklist.txt", verbBlacklistVec_);
        ensureFile(dir + "/pronouns.txt", pronouns_);
        ensureFile(dir + "/timewords.txt", timeWords_);
        ensureFile(dir + "/connectors.txt", connectors_);
        ensureFile(dir + "/quantifiers.txt", quantifiers_);
        ensureFile(dir + "/defaults.txt", defaultResponses_);
        ensureSynonymFile(dir + "/synonyms.txt", synonyms_);
        ensureRelationFile(dir + "/relations.txt", relations_);
    }

    void loadFromDir(const std::string &dir) {
        resetToDefaults();
        loadLines(dir + "/stopwords.txt", stopwordsVec_);
        loadLines(dir + "/chatroles.txt", chatRolesVec_);
        loadLines(dir + "/verbblacklist.txt", verbBlacklistVec_);
        loadLines(dir + "/pronouns.txt", pronouns_);
        loadLines(dir + "/timewords.txt", timeWords_);
        loadLines(dir + "/connectors.txt", connectors_);
        loadLines(dir + "/quantifiers.txt", quantifiers_);
        loadLines(dir + "/defaults.txt", defaultResponses_);
        loadSynonymFile(dir + "/synonyms.txt", synonyms_);
        loadRelationFile(dir + "/relations.txt", relations_);
        rebuildSets();
    }

    bool saveToDir(const std::string &dir) const {
        std::error_code ec;
        fs::create_directories(dir, ec);
        bool ok = true;
        ok &= writeLines(dir + "/stopwords.txt", stopwordsVec_);
        ok &= writeLines(dir + "/chatroles.txt", chatRolesVec_);
        ok &= writeLines(dir + "/verbblacklist.txt", verbBlacklistVec_);
        ok &= writeLines(dir + "/pronouns.txt", pronouns_);
        ok &= writeLines(dir + "/timewords.txt", timeWords_);
        ok &= writeLines(dir + "/connectors.txt", connectors_);
        ok &= writeLines(dir + "/quantifiers.txt", quantifiers_);
        ok &= writeLines(dir + "/defaults.txt", defaultResponses_);
        ok &= saveSynonymFile(dir + "/synonyms.txt", synonyms_);
        ok &= saveRelationFile(dir + "/relations.txt", relations_);
        return ok;
    }

    // ---- 只读访问 ----
    const std::unordered_set<std::string> &stopwordsSet() const {
        return stopwordsSet_;
    }
    const std::unordered_set<std::string> &chatRolesSet() const {
        return chatRolesSet_;
    }
    const std::unordered_set<std::string> &verbBlacklistSet() const {
        return verbBlacklistSet_;
    }
    const std::vector<std::string> &pronouns() const { return pronouns_; }
    const std::vector<std::string> &timeWords() const { return timeWords_; }
    const std::vector<std::string> &connectors() const { return connectors_; }
    const std::vector<std::string> &quantifiers() const { return quantifiers_; }
    const std::vector<std::string> &defaultResponses() const {
        return defaultResponses_;
    }
    const std::vector<std::vector<std::string>> &synonyms() const {
        return synonyms_;
    }
    const std::vector<RelationRule> &relations() const { return relations_; }

    // ---- 编辑访问（dict.exe 用） ----
    std::vector<std::string> &mutableStopwords() { return stopwordsVec_; }
    std::vector<std::string> &mutableChatRoles() { return chatRolesVec_; }
    std::vector<std::string> &mutableVerbBlacklist() {
        return verbBlacklistVec_;
    }
    std::vector<std::string> &mutablePronouns() { return pronouns_; }
    std::vector<std::string> &mutableTimeWords() { return timeWords_; }
    std::vector<std::string> &mutableConnectors() { return connectors_; }
    std::vector<std::string> &mutableQuantifiers() { return quantifiers_; }
    std::vector<std::string> &mutableDefaultResponses() {
        return defaultResponses_;
    }
    std::vector<std::vector<std::string>> &mutableSynonyms() {
        return synonyms_;
    }
    std::vector<RelationRule> &mutableRelations() { return relations_; }

    void rebuildSets() {
        stopwordsSet_.clear();
        for (const auto &s : stopwordsVec_) stopwordsSet_.insert(s);
        chatRolesSet_.clear();
        for (const auto &s : chatRolesVec_) chatRolesSet_.insert(s);
        verbBlacklistSet_.clear();
        for (const auto &s : verbBlacklistVec_) verbBlacklistSet_.insert(s);
    }

    void resetToDefaults() {
        stopwordsVec_ = {
            "什么", "怎么",  "怎样", "这样", "那样", "因为", "所以", "但是",
            "而且", "如果",  "虽然", "已经", "正在", "可以", "我们", "你们",
            "他们", "这个",  "那个", "一个", "一些", "这些", "那些", "自己",
            "大家", "然后",  "于是", "只是", "还是", "或者", "以及", "关于",
            "对于", "由于",  "为了", "并且", "不过", "其实", "就是", "不是",
            "你好", "请问",  "麻烦", "一下", "的话", "时候", "是",   "的",
            "了",   "和",    "与",   "及",   "能",   "会",   "要",   "想",
            "吗",   "呢",    "吧",   "啊",   "呀",   "地",   "得",   "谁",
            "哪",   "为什么"};
        chatRolesVec_ = {"你",   "我",   "他",   "她",   "它",   "我们",
                         "你们", "他们", "她们", "它们", "自己", "大家"};
        verbBlacklistVec_ = {
            "做", "干", "搞", "来", "去", "走", "跑", "看", "听", "说", "讲",
            "问", "答", "给", "拿", "用", "学", "教", "买", "卖", "写", "读",
            "改", "加", "减", "想", "要", "能", "会", "可", "该", "应", "得",
            "有", "是", "在", "把", "被", "让", "使", "叫", "称", "好"};
        synonyms_ = {{"怎么样", "咋样"}, {"怎么", "咋"},
                     {"什么", "啥"},     {"苹果", "苹菓"},
                     {"香蕉", "大蕉"},   {"好吃", "美味", "可口"},
                     {"首都", "首府"},   {"游泳", "游水", "浮水"},
                     {"疫苗", "防疫针"}, {"多少", "几个"}};
        relations_ = {{{"是"}, "{S}是什么？", "什么是{O}？"},
                      {{"属于"}, "{S}属于什么？", "什么属于{O}？"},
                      {{"包括", "包含"}, "{S}包括什么？", "什么包括{O}？"},
                      {{"喜欢"}, "{S}喜欢什么？", "谁喜欢{O}？"},
                      {{"会", "能", "可以"}, "{S}会什么？", ""},
                      {{"需要"}, "{S}需要什么？", ""},
                      {{"位于"}, "{S}位于哪里？", ""},
                      {{"被称为", "叫做", "叫作"}, "{S}叫什么？", ""}};
        pronouns_ = {"它们", "他们", "她们", "它", "他", "她"};
        timeWords_ = {"今天", "明天", "后天", "昨天", "前天", "现在", "目前",
                      "本周", "下周", "上周", "本月", "下月", "上月", "今年",
                      "明年", "去年", "早上", "中午", "晚上"};
        connectors_ = {"但是", "而且", "所以", "因此", "于是", "然后",
                       "不过", "其实", "只是", "如果", "因为", "虽然",
                       "以及", "并且", "另外", "此外"};
        quantifiers_ = {"一种", "一个", "一位", "一家", "一篇", "一根", "一本",
                        "一辆", "一座", "一名", "一头", "一类", "一群"};
        defaultResponses_ = {"我还在学习中，能换个方式问吗？",
                             "这个问题有点深奥，我暂时不知道。",
                             "你可以教我吗？请进入训练模式来教我。",
                             "我不太明白，请再说一遍。"};
        rebuildSets();
    }

  private:
    std::vector<std::string> stopwordsVec_;
    std::vector<std::string> chatRolesVec_;
    std::vector<std::string> verbBlacklistVec_;
    std::unordered_set<std::string> stopwordsSet_;
    std::unordered_set<std::string> chatRolesSet_;
    std::unordered_set<std::string> verbBlacklistSet_;

    std::vector<std::string> pronouns_;
    std::vector<std::string> timeWords_;
    std::vector<std::string> connectors_;
    std::vector<std::string> quantifiers_;
    std::vector<std::string> defaultResponses_;
    std::vector<std::vector<std::string>> synonyms_;
    std::vector<RelationRule> relations_;

    // ---- 工具 ----
    static std::string trim(const std::string &s) {
        size_t first = 0, last = s.size();
        while (first < last && (unsigned char)s[first] <= ' ') first++;
        while (last > first && (unsigned char)s[last - 1] <= ' ') last--;
        return s.substr(first, last - first);
    }

    static std::string stripBOM(const std::string &line) {
        if (line.size() >= 3 && (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB && (unsigned char)line[2] == 0xBF) {
            return line.substr(3);
        }
        return line;
    }

    static bool readLines(const std::string &path,
                          std::vector<std::string> &out) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return false;
        out.clear();
        std::string line;
        bool first = true;
        while (std::getline(f, line)) {
            if (first) {
                line = stripBOM(line);
                first = false;
            }
            if (!line.empty() && line.back() == '\r') line.pop_back();
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            out.push_back(line);
        }
        return true;
    }

    static void loadLines(const std::string &path,
                          std::vector<std::string> &out) {
        std::vector<std::string> tmp;
        if (readLines(path, tmp) && !tmp.empty()) {
            out = std::move(tmp);
        }
    }

    static bool writeLines(const std::string &path,
                           const std::vector<std::string> &v) {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        for (const auto &line : v) f << line << "\n";
        return (bool)f;
    }

    static void ensureFile(const std::string &path,
                           const std::vector<std::string> &v) {
        std::ifstream check(path);
        if (check.good()) return;
        check.close();
        writeLines(path, v);
    }

    static void loadSynonymFile(const std::string &path,
                                std::vector<std::vector<std::string>> &out) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return;
        std::vector<std::vector<std::string>> tmp;
        std::string line;
        bool first = true;
        while (std::getline(f, line)) {
            if (first) {
                line = stripBOM(line);
                first = false;
            }
            if (!line.empty() && line.back() == '\r') line.pop_back();
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            std::vector<std::string> group;
            std::istringstream iss(line);
            std::string cur;
            while (std::getline(iss, cur, '|')) {
                cur = trim(cur);
                if (!cur.empty()) group.push_back(cur);
            }
            if (group.size() >= 2) tmp.push_back(std::move(group));
        }
        if (!tmp.empty()) out = std::move(tmp);
    }

    static bool
    saveSynonymFile(const std::string &path,
                    const std::vector<std::vector<std::string>> &v) {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        for (const auto &group : v) {
            for (size_t i = 0; i < group.size(); i++) {
                if (i) f << "|";
                f << group[i];
            }
            f << "\n";
        }
        return (bool)f;
    }

    static void
    ensureSynonymFile(const std::string &path,
                      const std::vector<std::vector<std::string>> &v) {
        std::ifstream check(path);
        if (check.good()) return;
        check.close();
        saveSynonymFile(path, v);
    }

    static void loadRelationFile(const std::string &path,
                                 std::vector<RelationRule> &out) {
        std::ifstream f(path, std::ios::binary);
        if (!f) return;
        std::vector<RelationRule> tmp;
        std::string line;
        bool first = true;
        while (std::getline(f, line)) {
            if (first) {
                line = stripBOM(line);
                first = false;
            }
            if (!line.empty() && line.back() == '\r') line.pop_back();
            line = trim(line);
            if (line.empty() || line[0] == '#') continue;
            std::vector<std::string> parts;
            std::istringstream iss(line);
            std::string cur;
            while (std::getline(iss, cur, '|')) parts.push_back(trim(cur));
            if (parts.size() < 2) continue;
            RelationRule rule;
            std::istringstream kis(parts[0]);
            std::string kw;
            while (std::getline(kis, kw, ',')) {
                kw = trim(kw);
                if (!kw.empty()) rule.keywords.push_back(kw);
            }
            if (rule.keywords.empty()) continue;
            rule.forwardTemplate = parts[1];
            if (parts.size() >= 3) rule.backwardTemplate = parts[2];
            tmp.push_back(std::move(rule));
        }
        if (!tmp.empty()) out = std::move(tmp);
    }

    static bool saveRelationFile(const std::string &path,
                                 const std::vector<RelationRule> &v) {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (!f) return false;
        for (const auto &rule : v) {
            for (size_t i = 0; i < rule.keywords.size(); i++) {
                if (i) f << ",";
                f << rule.keywords[i];
            }
            f << "|" << rule.forwardTemplate << "|" << rule.backwardTemplate
              << "\n";
        }
        return (bool)f;
    }

    static void ensureRelationFile(const std::string &path,
                                   const std::vector<RelationRule> &v) {
        std::ifstream check(path);
        if (check.good()) return;
        check.close();
        saveRelationFile(path, v);
    }
};
// ============================================================
// 控制台字体设置
// ============================================================
void setConsoleFont(const wchar_t *faceName = L"Consolas",
                    short fontSize = 20) {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return;

    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    if (!GetCurrentConsoleFontEx(hOut, FALSE, &cfi)) {
        // 拿不到当前信息，就用默认值试一次
        ZeroMemory(&cfi, sizeof(cfi));
        cfi.cbSize = sizeof(cfi);
        cfi.nFont = 0;
        cfi.FontFamily = FF_DONTCARE;
        cfi.FontWeight = FW_NORMAL;
    }
    // 改字体名和字号
    wcsncpy(cfi.FaceName, faceName, LF_FACESIZE - 1);
    cfi.FaceName[LF_FACESIZE - 1] = L'\0';
    cfi.dwFontSize.X = 0;        // 宽度自动
    cfi.dwFontSize.Y = fontSize; // 高度
    // 应用
    SetCurrentConsoleFontEx(hOut, FALSE, &cfi);
}
