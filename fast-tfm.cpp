// ╔══════════════════════════════════════════════════════════════════════╗
// ║          fast-tfm  ·  Terminal File Manager  v3.0                  ║
// ║  Dual-pane TUI · FTXUI v5+ · Feature-complete & Warning-free       ║
// ╚══════════════════════════════════════════════════════════════════════╝
//
// Build (single line):
//   g++ -std=c++17 -O2 fast-tfm.cpp -lftxui-component -lftxui-dom -lftxui-screen -o fast-tfm
//
// Or: cmake .. && cmake --build .

#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <system_error>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <set>
#include <functional>
#include <cstdlib>
#include <unordered_set>
#include <optional>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

namespace fs = std::filesystem;

// NOTE: We do NOT use "using namespace ftxui" to avoid the name clash
// between our struct TFMApp and ftxui::App (introduced in FTXUI v5).
// All ftxui identifiers are prefixed with ftxui:: below.

// ═══════════════════════════════════════════════════════════════════
//  Convenience aliases
// ═══════════════════════════════════════════════════════════════════

using ftxui::Color;
using ftxui::Component;
using ftxui::Components;
using ftxui::Decorator;
using ftxui::Element;
using ftxui::Elements;
using ftxui::Event;

// DOM helpers we use frequently
using ftxui::bold;
using ftxui::border;
using ftxui::borderStyled;
using ftxui::center;
using ftxui::clear_under;
using ftxui::color;
using ftxui::dbox;
using ftxui::dim;
using ftxui::EQUAL;
using ftxui::filler;
using ftxui::flex;
using ftxui::flex_grow;
using ftxui::frame;
using ftxui::GREATER_THAN;
using ftxui::hbox;
using ftxui::HEAVY;
using ftxui::inverted;
using ftxui::separator;
using ftxui::separatorEmpty;
using ftxui::size;
using ftxui::text;
using ftxui::vbox;
using ftxui::vscroll_indicator;
using ftxui::WIDTH;
using ftxui::window;

// Component helpers
using ftxui::CatchEvent;
using ftxui::Input;
using ftxui::Renderer;
using ftxui::ScreenInteractive;

namespace Container = ftxui::Container;

// ═══════════════════════════════════════════════════════════════════
//  Constants
// ═══════════════════════════════════════════════════════════════════

static constexpr int PREVIEW_LINES = 50;
static constexpr int FIND_MAX = 500;
static constexpr int PAGE_JUMP = 10;
static const std::string VERSION = "3.0.0";

// ═══════════════════════════════════════════════════════════════════
//  Extension Sets
// ═══════════════════════════════════════════════════════════════════

static const std::set<std::string> EXT_IMAGE = {
    ".png", ".jpg", ".jpeg", ".gif", ".bmp", ".svg", ".webp",
    ".ico", ".tiff", ".avif", ".heic", ".raw", ".psd"};
static const std::set<std::string> EXT_VIDEO = {
    ".mp4", ".mkv", ".avi", ".mov", ".wmv", ".flv", ".webm", ".m4v", ".ts", ".3gp"};
static const std::set<std::string> EXT_AUDIO = {
    ".mp3", ".wav", ".flac", ".ogg", ".aac", ".m4a", ".opus", ".wma", ".alac"};
static const std::set<std::string> EXT_ARCH = {
    ".zip", ".tar", ".gz", ".bz2", ".xz", ".7z", ".rar", ".deb", ".rpm",
    ".zst", ".lz4", ".lzma", ".cab", ".iso"};
static const std::set<std::string> EXT_CODE = {
    ".cpp", ".c", ".h", ".hpp", ".cc", ".cxx", ".hxx",
    ".py", ".pyw", ".pyi",
    ".js", ".ts", ".jsx", ".tsx", ".mjs", ".cjs",
    ".rs", ".go", ".java", ".cs", ".rb", ".php", ".swift", ".kt", ".kts",
    ".sh", ".bash", ".zsh", ".fish", ".ps1", ".bat", ".cmd",
    ".lua", ".vim", ".el", ".hs", ".ml", ".mli", ".r", ".jl", ".nim", ".zig",
    ".toml", ".yaml", ".yml", ".json", ".jsonc", ".json5",
    ".xml", ".html", ".htm", ".css", ".scss", ".sass", ".less",
    ".md", ".mdx", ".rst", ".tex", ".cmake",
    ".sql", ".graphql", ".proto", ".dart", ".ex", ".exs"};
static const std::set<std::string> EXT_DOC = {
    ".pdf", ".doc", ".docx", ".odt", ".xls", ".xlsx", ".ods",
    ".ppt", ".pptx", ".odp", ".epub", ".mobi"};
static const std::set<std::string> EXT_TEXT = {
    ".txt", ".log", ".conf", ".ini", ".cfg", ".env",
    ".gitignore", ".gitattributes", ".editorconfig", ".htaccess"};

// ═══════════════════════════════════════════════════════════════════
//  Enums
// ═══════════════════════════════════════════════════════════════════

enum class SortMode
{
    NAME,
    NAME_DESC,
    SIZE,
    SIZE_DESC,
    DATE,
    DATE_DESC,
    EXT
};
enum class ViewMode
{
    NORMAL,
    DETAIL
};

// ═══════════════════════════════════════════════════════════════════
//  Config Paths
// ═══════════════════════════════════════════════════════════════════

static fs::path ConfigDir()
{
    const char *h = std::getenv("HOME");
    return fs::path(h ? h : "/tmp") / ".config" / "fast_tfm";
}
static fs::path BookmarksFile() { return ConfigDir() / "bookmarks"; }
static fs::path TrashDir() { return ConfigDir() / "trash"; }

// ═══════════════════════════════════════════════════════════════════
//  Utility Functions
// ═══════════════════════════════════════════════════════════════════

static std::string GetExt(const std::string &name)
{
    const auto dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0)
        return "";
    std::string ext = name.substr(dot);
    for (auto &c : ext)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

static std::string FormatSize(uintmax_t bytes)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    if (bytes < 1024ULL)
        out << bytes << "B";
    else if (bytes < 1024ULL * 1024)
        out << bytes / 1024.0 << "K";
    else if (bytes < 1024ULL * 1024 * 1024)
        out << bytes / (1024.0 * 1024) << "M";
    else
        out << bytes / (1024.0 * 1024 * 1024) << "G";
    return out.str();
}

static std::string FormatSizeLong(uintmax_t bytes)
{
    std::ostringstream out;
    out << std::fixed << std::setprecision(1);
    if (bytes < 1024ULL)
        out << bytes << " B ";
    else if (bytes < 1024ULL * 1024)
        out << bytes / 1024.0 << " KB";
    else if (bytes < 1024ULL * 1024 * 1024)
        out << bytes / (1024.0 * 1024) << " MB";
    else if (bytes < 1024ULL * 1024 * 1024 * 1024ULL)
        out << bytes / (1024.0 * 1024 * 1024) << " GB";
    else
        out << bytes / (1024.0 * 1024 * 1024 * 1024) << " TB";
    return out.str();
}

static std::string FormatTime(const fs::file_time_type &ftime)
{
    try
    {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
        const std::time_t t = std::chrono::system_clock::to_time_t(sctp);
        std::tm *tm_info = std::localtime(&t);
        if (!tm_info)
            return "0000-00-00 00:00";
        char buf[24];
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M", tm_info);
        return std::string(buf);
    }
    catch (...)
    {
        return "0000-00-00 00:00";
    }
}

static std::string FormatPerms(fs::perms p)
{
    std::string s;
    auto has = [&](fs::perms bit)
    { return (p & bit) != fs::perms::none; };
    s += has(fs::perms::owner_read) ? 'r' : '-';
    s += has(fs::perms::owner_write) ? 'w' : '-';
    s += has(fs::perms::owner_exec) ? 'x' : '-';
    s += has(fs::perms::group_read) ? 'r' : '-';
    s += has(fs::perms::group_write) ? 'w' : '-';
    s += has(fs::perms::group_exec) ? 'x' : '-';
    s += has(fs::perms::others_read) ? 'r' : '-';
    s += has(fs::perms::others_write) ? 'w' : '-';
    s += has(fs::perms::others_exec) ? 'x' : '-';
    return s;
}

static Color FileColor(const std::string &name, bool is_dir, bool is_link, bool is_exec)
{
    if (name == "..")
        return Color::Yellow;
    if (is_link)
        return Color::Cyan;
    if (is_dir)
        return Color::CyanLight;
    if (is_exec)
        return Color::GreenLight;
    const std::string ext = GetExt(name);
    if (EXT_CODE.count(ext))
        return Color::Green;
    if (EXT_IMAGE.count(ext))
        return Color::MagentaLight;
    if (EXT_VIDEO.count(ext))
        return Color::Red;
    if (EXT_AUDIO.count(ext))
        return Color::BlueLight;
    if (EXT_ARCH.count(ext))
        return Color::RedLight;
    if (EXT_DOC.count(ext))
        return Color::Yellow;
    if (EXT_TEXT.count(ext))
        return Color::White;
    return Color::GrayLight;
}

static std::string FileTypeLabel(const std::string &name, bool is_dir, bool is_link, bool is_exec)
{
    if (is_link)
        return "Symlink";
    if (is_dir)
        return "Directory";
    if (is_exec)
        return "Executable";
    const std::string ext = GetExt(name);
    if (EXT_CODE.count(ext))
        return "Source Code";
    if (EXT_IMAGE.count(ext))
        return "Image";
    if (EXT_VIDEO.count(ext))
        return "Video";
    if (EXT_AUDIO.count(ext))
        return "Audio";
    if (EXT_ARCH.count(ext))
        return "Archive";
    if (EXT_DOC.count(ext))
        return "Document";
    if (EXT_TEXT.count(ext))
        return "Text";
    if (ext.empty())
        return "File";
    return ext.substr(1) + " File";
}

static bool IsBinary(const fs::path &p)
{
    std::ifstream f(p, std::ios::binary);
    if (!f)
        return true;
    char buf[2048];
    f.read(buf, sizeof(buf));
    const auto n = f.gcount();
    for (std::streamsize i = 0; i < n; ++i)
        if (buf[i] == '\0')
            return true;
    return false;
}

static std::pair<int, int> CountLinesWords(const fs::path &p)
{
    std::ifstream f(p);
    if (!f)
        return {0, 0};
    int lines = 0, words = 0;
    std::string line;
    while (std::getline(f, line))
    {
        ++lines;
        std::istringstream ss(line);
        std::string w;
        while (ss >> w)
            ++words;
    }
    return {lines, words};
}

static std::vector<std::string> GetFilePreview(const fs::path &path,
                                               int &out_lines, int &out_words)
{
    out_lines = out_words = 0;
    std::error_code ec;
    const auto sz = fs::file_size(path, ec);
    if (ec || sz == 0)
        return {"[ Empty file ]"};
    if (sz > 10ULL * 1024 * 1024)
        return {"[ File too large to preview (> 10 MB) ]"};
    if (IsBinary(path))
        return {"[ Binary file — no text preview ]"};

    auto [lc, wc] = CountLinesWords(path);
    out_lines = lc;
    out_words = wc;

    std::vector<std::string> lines;
    std::ifstream file(path);
    if (!file)
        return {"[ Cannot open file ]"};

    std::string line;
    int count = 0;
    while (std::getline(file, line) && count < PREVIEW_LINES)
    {
        std::string clean;
        int col = 0;
        for (unsigned char c : line)
        {
            if (c == '\t')
            {
                const int sp = 4 - (col % 4);
                clean.append(sp, ' ');
                col += sp;
            }
            else if (c >= 32)
            {
                clean += static_cast<char>(c);
                ++col;
            }
            if (col > 200)
            {
                clean += " ...";
                break;
            }
        }
        const std::string num = std::to_string(count + 1);
        clean = std::string(4 - std::min(4, static_cast<int>(num.size())), ' ') + num + " | " + clean;
        lines.push_back(std::move(clean));
        ++count;
    }
    if (!file.eof() && lc > PREVIEW_LINES)
        lines.push_back("     ... " + std::to_string(lc - PREVIEW_LINES) + " more lines ...");
    return lines;
}

static uintmax_t DirSize(const fs::path &p)
{
    uintmax_t total = 0;
    std::error_code ec;
    for (auto &de : fs::recursive_directory_iterator(
             p, fs::directory_options::skip_permission_denied, ec))
    {
        if (!fs::is_directory(de.path(), ec))
        {
            const auto s = fs::file_size(de.path(), ec);
            if (!ec)
                total += s;
        }
    }
    return total;
}

static std::vector<fs::path> FindFiles(const fs::path &root,
                                       const std::string &pat, int max_results)
{
    std::vector<fs::path> res;
    std::string lop = pat;
    for (auto &c : lop)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    try
    {
        for (auto &de : fs::recursive_directory_iterator(
                 root, fs::directory_options::skip_permission_denied))
        {
            const std::string n = de.path().filename().string();
            std::string lo = n;
            for (auto &c : lo)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (lo.find(lop) != std::string::npos)
            {
                res.push_back(de.path());
                if (static_cast<int>(res.size()) >= max_results)
                    break;
            }
        }
    }
    catch (...)
    {
    }
    return res;
}

static void YankToClipboard(const std::string &txt)
{
    std::string safe;
    for (char c : txt)
    {
        if (c == '\'')
            safe += "'\\''";
        else
            safe += c;
    }
    const char *cmds[] = {
        "xclip -selection clipboard",
        "xsel --clipboard --input",
        "wl-copy",
        "pbcopy",
        nullptr};
    for (int i = 0; cmds[i]; ++i)
    {
        std::string cmd = "printf '%s' '" + safe + "' | " + cmds[i] + " 2>/dev/null";
        if (std::system(cmd.c_str()) == 0)
            break;
    }
}

// ═══════════════════════════════════════════════════════════════════
//  Entry
// ═══════════════════════════════════════════════════════════════════

struct Entry
{
    std::string name;
    bool is_dir = false;
    bool is_link = false;
    bool is_exec = false;
    uintmax_t size = 0;
    std::string mod_time;
    std::string perms_str;
    std::string link_target;
};

// ═══════════════════════════════════════════════════════════════════
//  Pane
// ═══════════════════════════════════════════════════════════════════

struct Pane
{
    fs::path cwd;
    std::vector<Entry> entries;
    int selected = 0;
    int last_sel = -1;
    std::vector<std::string> preview_cache;
    int prev_lines = 0;
    int prev_words = 0;
    SortMode sort_mode = SortMode::NAME;
    ViewMode view_mode = ViewMode::DETAIL;
    bool show_hidden = false;
    std::string filter;
    std::unordered_set<int> sel_set;

    void Load()
    {
        entries.clear();
        sel_set.clear();

        if (cwd != cwd.root_path())
        {
            Entry up;
            up.name = "..";
            up.is_dir = true;
            entries.push_back(up);
        }

        std::vector<Entry> raw;
        try
        {
            for (const auto &de : fs::directory_iterator(
                     cwd, fs::directory_options::skip_permission_denied))
            {
                const std::string name = de.path().filename().string();
                if (!show_hidden && !name.empty() && name[0] == '.')
                    continue;
                if (!filter.empty())
                {
                    std::string lo = name, lof = filter;
                    for (auto &c : lo)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    for (auto &c : lof)
                        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                    if (lo.find(lof) == std::string::npos)
                        continue;
                }

                Entry ent;
                ent.name = name;
                std::error_code ec;

                const auto syml_st = de.symlink_status(ec);
                ent.is_link = !ec && fs::is_symlink(syml_st);

                const auto rst = de.status(ec);
                ent.is_dir = !ec && fs::is_directory(rst);

                if (!ec)
                {
                    const auto prm = rst.permissions();
                    ent.is_exec = (prm & fs::perms::owner_exec) != fs::perms::none;
                    if (!ent.is_dir)
                    {
                        ent.size = fs::file_size(de.path(), ec);
                        ent.perms_str = FormatPerms(prm);
                    }
                }

                if (ent.is_link)
                {
                    const auto tgt = fs::read_symlink(de.path(), ec);
                    if (!ec)
                        ent.link_target = tgt.string();
                }

                const auto mt = fs::last_write_time(de.path(), ec);
                if (!ec)
                    ent.mod_time = FormatTime(mt);

                raw.push_back(std::move(ent));
            }
        }
        catch (...)
        {
        }

        std::stable_sort(raw.begin(), raw.end(), [&](const Entry &a, const Entry &b)
                         {
            if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
            switch (sort_mode) {
                case SortMode::NAME:      return a.name     <  b.name;
                case SortMode::NAME_DESC: return a.name     >  b.name;
                case SortMode::SIZE:      return a.size     <  b.size;
                case SortMode::SIZE_DESC: return a.size     >  b.size;
                case SortMode::DATE:      return a.mod_time <  b.mod_time;
                case SortMode::DATE_DESC: return a.mod_time >  b.mod_time;
                case SortMode::EXT:       return GetExt(a.name) < GetExt(b.name);
            }
            return false; });

        for (auto &e : raw)
            entries.push_back(std::move(e));
        selected = std::max(0, std::min(selected, static_cast<int>(entries.size()) - 1));
        last_sel = -1;
    }

    bool Empty() const { return entries.empty(); }

    const Entry *Cur() const
    {
        if (entries.empty())
            return nullptr;
        return &entries[selected];
    }

    fs::path SelPath() const
    {
        const auto *e = Cur();
        if (!e)
            return cwd;
        if (e->name == "..")
            return cwd.parent_path();
        return cwd / e->name;
    }

    bool SelIsDir() const
    {
        const auto *e = Cur();
        return e && (e->name == ".." || e->is_dir);
    }

    std::string SelName() const
    {
        const auto *e = Cur();
        return e ? e->name : "";
    }

    const std::vector<std::string> &Preview()
    {
        if (selected == last_sel)
            return preview_cache;
        last_sel = selected;
        const auto *e = Cur();
        if (!e || e->is_dir || e->name == "..")
        {
            preview_cache.clear();
            prev_lines = prev_words = 0;
        }
        else
        {
            preview_cache = GetFilePreview(SelPath(), prev_lines, prev_words);
        }
        return preview_cache;
    }

    void NavigateInto()
    {
        if (entries.empty())
            return;
        const fs::path t = SelPath();
        std::error_code ec;
        if (fs::is_directory(t, ec))
        {
            const auto c = fs::canonical(t, ec);
            if (!ec)
            {
                cwd = c;
                selected = 0;
                Load();
            }
        }
    }

    std::vector<fs::path> GetSelection() const
    {
        if (!sel_set.empty())
        {
            std::vector<fs::path> v;
            for (int i : sel_set)
                if (i > 0 && i < static_cast<int>(entries.size()))
                    v.push_back(cwd / entries[i].name);
            return v;
        }
        const auto *e = Cur();
        if (!e || e->name == "..")
            return {};
        return {SelPath()};
    }

    void ToggleSel()
    {
        if (Empty())
            return;
        const auto *e = Cur();
        if (!e || e->name == "..")
            return;
        if (sel_set.count(selected))
            sel_set.erase(selected);
        else
            sel_set.insert(selected);
    }

    void ClearSel() { sel_set.clear(); }

    std::string SortLabel() const
    {
        switch (sort_mode)
        {
        case SortMode::NAME:
            return "^N";
        case SortMode::NAME_DESC:
            return "vN";
        case SortMode::SIZE:
            return "^S";
        case SortMode::SIZE_DESC:
            return "vS";
        case SortMode::DATE:
            return "^D";
        case SortMode::DATE_DESC:
            return "vD";
        case SortMode::EXT:
            return "^E";
        }
        return "?";
    }

    void CycleSortForward()
    {
        switch (sort_mode)
        {
        case SortMode::NAME:
            sort_mode = SortMode::NAME_DESC;
            break;
        case SortMode::NAME_DESC:
            sort_mode = SortMode::SIZE;
            break;
        case SortMode::SIZE:
            sort_mode = SortMode::SIZE_DESC;
            break;
        case SortMode::SIZE_DESC:
            sort_mode = SortMode::DATE;
            break;
        case SortMode::DATE:
            sort_mode = SortMode::DATE_DESC;
            break;
        case SortMode::DATE_DESC:
            sort_mode = SortMode::EXT;
            break;
        case SortMode::EXT:
            sort_mode = SortMode::NAME;
            break;
        }
    }
};

// ═══════════════════════════════════════════════════════════════════
//  Dialog
// ═══════════════════════════════════════════════════════════════════

struct Dialog
{
    bool active = false;
    bool has_input = false;
    std::string title;
    std::string body;
    std::string input;
    std::function<void(bool, const std::string &)> cb;

    void YesNo(const std::string &t, const std::string &b,
               std::function<void(bool, const std::string &)> fn)
    {
        title = t;
        body = b;
        cb = std::move(fn);
        has_input = false;
        input.clear();
        active = true;
    }

    void TextInput(const std::string &t, const std::string &b,
                   std::function<void(bool, const std::string &)> fn,
                   const std::string &def = "")
    {
        title = t;
        body = b;
        cb = std::move(fn);
        has_input = true;
        input = def;
        active = true;
    }
};

// ═══════════════════════════════════════════════════════════════════
//  Bookmarks
// ═══════════════════════════════════════════════════════════════════

struct Bookmarks
{
    std::vector<fs::path> list;
    int sel = 0;

    void Load()
    {
        std::ifstream f(BookmarksFile());
        if (!f)
            return;
        std::string line;
        while (std::getline(f, line))
            if (!line.empty())
                list.push_back(line);
    }

    void Save() const
    {
        std::error_code ec;
        fs::create_directories(ConfigDir(), ec);
        std::ofstream f(BookmarksFile());
        if (!f)
            return;
        for (const auto &p : list)
            f << p.string() << "\n";
    }

    void Add(const fs::path &p)
    {
        if (std::find(list.begin(), list.end(), p) == list.end())
        {
            list.push_back(p);
            Save();
        }
    }

    void Remove(int i)
    {
        if (i >= 0 && i < static_cast<int>(list.size()))
        {
            list.erase(list.begin() + i);
            Save();
        }
    }
};

// ═══════════════════════════════════════════════════════════════════
//  Clipboard
// ═══════════════════════════════════════════════════════════════════

struct Clipboard
{
    std::vector<fs::path> paths;
    bool is_cut = false;
    bool has = false;

    void Set(std::vector<fs::path> p, bool cut)
    {
        paths = std::move(p);
        is_cut = cut;
        has = !paths.empty();
    }
    void Clear()
    {
        paths.clear();
        has = false;
    }

    std::string Label() const
    {
        if (!has)
            return "";
        std::string s = is_cut ? "Cut" : "Copy";
        if (paths.size() == 1)
            s += ": " + paths[0].filename().string();
        else
            s += ": " + std::to_string(paths.size()) + " items";
        return s;
    }
};

// ═══════════════════════════════════════════════════════════════════
//  StatusBar
// ═══════════════════════════════════════════════════════════════════

struct StatusBar
{
    std::string msg;
    Color col = Color::GreenLight;

    void Set(const std::string &m, Color c = Color::GreenLight)
    {
        msg = m;
        col = c;
    }
    void Ok(const std::string &m) { Set("OK " + m, Color::GreenLight); }
    void Err(const std::string &m) { Set("ERR " + m, Color::Red); }
    void Info(const std::string &m) { Set(">> " + m, Color::Cyan); }
    void Warn(const std::string &m) { Set("!! " + m, Color::Yellow); }
    void Clear() { msg.clear(); }
};

// ═══════════════════════════════════════════════════════════════════
//  File Operations
// ═══════════════════════════════════════════════════════════════════

static bool OpDelete(const fs::path &p, std::string &err)
{
    std::error_code ec;
    fs::remove_all(p, ec);
    if (ec)
    {
        err = ec.message();
        return false;
    }
    return true;
}

static bool OpTrash(const fs::path &p, std::string &err)
{
    std::error_code ec;
    fs::create_directories(TrashDir(), ec);
    const auto ts = std::chrono::duration_cast<std::chrono::seconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    const fs::path dst = TrashDir() / (std::to_string(ts) + "_" + p.filename().string());

    fs::rename(p, dst, ec);
    if (!ec)
        return true;

    fs::copy(p, dst, fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        err = ec.message();
        return false;
    }
    fs::remove_all(p, ec);
    return true;
}

static bool OpRestoreTrash(std::string &name, std::string &err)
{
    std::error_code ec;
    if (!fs::exists(TrashDir(), ec) || ec)
    {
        err = "Trash is empty";
        return false;
    }

    fs::path latest;
    fs::file_time_type lt;
    bool found = false;
    for (auto &de : fs::directory_iterator(TrashDir(), ec))
    {
        const auto t = fs::last_write_time(de.path(), ec);
        if (!found || t > lt)
        {
            latest = de.path();
            lt = t;
            found = true;
        }
    }
    if (!found)
    {
        err = "Trash is empty";
        return false;
    }

    const std::string fn = latest.filename().string();
    const auto us = fn.find('_');
    const std::string orig = (us != std::string::npos) ? fn.substr(us + 1) : fn;
    const fs::path dst = fs::current_path() / orig;

    fs::rename(latest, dst, ec);
    if (ec)
    {
        err = ec.message();
        return false;
    }
    name = orig;
    return true;
}

static bool OpCopy(const fs::path &src, const fs::path &dst, std::string &err)
{
    std::error_code ec;
    fs::copy(src, dst,
             fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
    if (ec)
    {
        err = ec.message();
        return false;
    }
    return true;
}

static bool OpMove(const fs::path &src, const fs::path &dst, std::string &err)
{
    std::error_code ec;
    fs::rename(src, dst, ec);
    if (!ec)
        return true;
    if (!OpCopy(src, dst, err))
        return false;
    fs::remove_all(src, ec);
    if (ec)
    {
        err = ec.message();
        return false;
    }
    return true;
}

static bool OpRename(const fs::path &src, const std::string &nm, std::string &err)
{
    return OpMove(src, src.parent_path() / nm, err);
}

static bool OpMkdir(const fs::path &p, std::string &err)
{
    std::error_code ec;
    fs::create_directories(p, ec);
    if (ec)
    {
        err = ec.message();
        return false;
    }
    return true;
}

static bool OpTouch(const fs::path &p, std::string &err)
{
    std::ofstream f(p);
    if (!f)
    {
        err = "Cannot create file";
        return false;
    }
    return true;
}

static bool OpSymlink(const fs::path &tgt, const fs::path &link, std::string &err)
{
    std::error_code ec;
    fs::create_symlink(tgt, link, ec);
    if (ec)
    {
        err = ec.message();
        return false;
    }
    return true;
}

static bool OpChmod(const fs::path &p, const std::string &mode_str, std::string &err)
{
    try
    {
        const unsigned long v = std::stoul(mode_str, nullptr, 8);
        const fs::perms prm = static_cast<fs::perms>(v & 0777u);
        std::error_code ec;
        fs::permissions(p, prm, fs::perm_options::replace, ec);
        if (ec)
        {
            err = ec.message();
            return false;
        }
        return true;
    }
    catch (...)
    {
        err = "Invalid octal mode: " + mode_str;
        return false;
    }
}

// ═══════════════════════════════════════════════════════════════════
//  Navigation History
// ═══════════════════════════════════════════════════════════════════

struct NavHistory
{
    std::vector<fs::path> stack;
    int pos = -1;

    void Push(const fs::path &p)
    {
        if (pos < static_cast<int>(stack.size()) - 1)
            stack.erase(stack.begin() + pos + 1, stack.end());
        if (stack.empty() || stack.back() != p)
        {
            stack.push_back(p);
            ++pos;
        }
    }

    std::optional<fs::path> Back()
    {
        if (pos > 0)
        {
            --pos;
            return stack[pos];
        }
        return std::nullopt;
    }

    std::optional<fs::path> Forward()
    {
        if (pos < static_cast<int>(stack.size()) - 1)
        {
            ++pos;
            return stack[pos];
        }
        return std::nullopt;
    }
};

// ═══════════════════════════════════════════════════════════════════
//  UI Builder Helpers
// ═══════════════════════════════════════════════════════════════════

static Element BuildEntryList(const Pane &pane, bool focused)
{
    Elements rows;
    for (int i = 0; i < static_cast<int>(pane.entries.size()); ++i)
    {
        const auto &e = pane.entries[i];
        const bool msel = pane.sel_set.count(i) > 0;
        const bool isd = e.is_dir || e.name == "..";
        const Color fc = FileColor(e.name, e.is_dir, e.is_link, e.is_exec);

        std::string disp = e.name;
        if (isd && e.name != "..")
            disp += "/";
        if (e.is_link)
            disp = "-> " + disp;

        Element row;
        if (pane.view_mode == ViewMode::DETAIL && e.name != "..")
        {
            const std::string sz = isd ? "     -" : FormatSize(e.size);
            const std::string dt = e.mod_time.empty() ? "               " : e.mod_time;
            Element nm = text(disp) | color(fc);
            if (isd)
                nm = nm | bold;
            // Build row as explicit Elements vector to satisfy FTXUI v5 API
            Elements cells;
            cells.push_back(text(msel ? " * " : "   ") | color(Color::Yellow));
            cells.push_back(nm | flex);
            cells.push_back(text(sz) | color(Color::GrayDark) | size(WIDTH, EQUAL, 7));
            cells.push_back(text("  " + dt) | color(Color::GrayDark) | size(WIDTH, EQUAL, 18));
            row = hbox(std::move(cells));
        }
        else
        {
            Element nm = text(disp) | color(fc);
            if (isd)
                nm = nm | bold;
            Elements cells;
            cells.push_back(text(msel ? " * " : "   ") | color(Color::Yellow));
            cells.push_back(nm);
            row = hbox(std::move(cells));
        }

        if (focused && i == pane.selected)
            row = row | inverted;
        rows.push_back(row);
    }
    if (rows.empty())
        rows.push_back(text("  (empty)") | color(Color::GrayDark));
    return vbox(std::move(rows));
}

static Element BuildBreadcrumb(const fs::path &cwd)
{
    Elements parts;
    parts.push_back(text("/") | color(Color::GrayDark));
    std::vector<std::string> segs;
    for (const auto &s : cwd)
    {
        const std::string ss = s.string();
        if (!ss.empty() && ss != "/")
            segs.push_back(ss);
    }
    for (int i = 0; i < static_cast<int>(segs.size()); ++i)
    {
        const bool last = (i == static_cast<int>(segs.size()) - 1);
        parts.push_back(text(segs[i]) | color(last ? Color::White : Color::GrayLight));
        if (!last)
            parts.push_back(text("/") | color(Color::GrayDark));
    }
    return hbox(std::move(parts));
}

// ═══════════════════════════════════════════════════════════════════
//  Application State  (renamed TFMApp to avoid clash with ftxui::App)
// ═══════════════════════════════════════════════════════════════════

struct TFMApp
{
    Pane left, right;
    bool active_left = true;
    Bookmarks bookmarks;
    Clipboard clipboard;
    StatusBar status;
    Dialog dialog;
    NavHistory hist_left, hist_right;

    bool show_bookmarks = false;
    bool show_help = false;
    bool show_find = false;
    bool filter_mode = false;
    bool sync_panes = false;

    std::string find_query;
    std::vector<fs::path> find_results;
    int find_sel = 0;

    std::string pending_editor;
    std::string pending_xopen;
    bool should_quit = false;

    Pane &AP() { return active_left ? left : right; }
    Pane &PP() { return active_left ? right : left; }
    NavHistory &AH() { return active_left ? hist_left : hist_right; }

    void Sync()
    {
        if (sync_panes)
        {
            PP().cwd = AP().cwd;
            PP().Load();
        }
    }

    void Navigate(Pane &pane, NavHistory &hist, const fs::path &dest)
    {
        hist.Push(dest);
        pane.cwd = dest;
        pane.selected = 0;
        pane.Load();
    }
};

// ═══════════════════════════════════════════════════════════════════
//  Main
// ═══════════════════════════════════════════════════════════════════

int main()
{
    TFMApp app;

    {
        std::error_code ec;
        fs::create_directories(ConfigDir(), ec);
        fs::create_directories(TrashDir(), ec);
    }

    const char *HOME = std::getenv("HOME");

    app.left.cwd = fs::current_path();
    app.left.Load();
    app.hist_left.Push(app.left.cwd);

    app.right.cwd = HOME ? fs::path(HOME) : fs::current_path();
    app.right.Load();
    app.hist_right.Push(app.right.cwd);

    app.bookmarks.Load();
    if (app.bookmarks.list.empty())
    {
        if (HOME)
            app.bookmarks.Add(fs::path(HOME));
        app.bookmarks.Add(fs::path("/tmp"));
        app.bookmarks.Add(fs::path("/"));
    }

    // ── Application loop ────────────────────────────────────────────
    while (!app.should_quit)
    {
        app.pending_editor.clear();
        app.pending_xopen.clear();

        auto screen = ScreenInteractive::Fullscreen();
        auto input_comp = Input(&app.dialog.input, "");
        auto find_input = Input(&app.find_query, "filename pattern...");

        // Container::Tab needs an explicit Components vector in FTXUI v5
        Components tab_children;
        tab_children.push_back(input_comp);
        tab_children.push_back(find_input);
        int tab_selected = 0;
        auto tab_cont = Container::Tab(tab_children, &tab_selected);

        // ── Renderer ────────────────────────────────────────────────
        auto renderer = Renderer(tab_cont, [&]() -> Element
                                 {
                                     const bool lf = app.active_left;
                                     const bool rf = !app.active_left;

                                     // ── Left pane ──
                                     Element lc = BuildEntryList(app.left, lf) | vscroll_indicator | frame;
                                     {
                                         const std::string ltit =
                                             " " + (app.left.cwd.filename().empty() ? std::string("/") : app.left.cwd.filename().string()) + " ";
                                         Elements lhdr;
                                         lhdr.push_back(text(ltit) | (lf ? bold : dim));
                                         lhdr.push_back(filler());
                                         lhdr.push_back(text(app.left.SortLabel() + " ") | color(Color::GrayDark));
                                         lhdr.push_back(text(app.left.view_mode == ViewMode::DETAIL ? "= " : "- ") | color(Color::GrayDark));
                                         Element lbox = window(hbox(std::move(lhdr)), lc) | flex;
                                         if (lf)
                                             lbox = lbox | borderStyled(HEAVY);

                                         // ── Right pane ──
                                         Element rc = BuildEntryList(app.right, rf) | vscroll_indicator | frame;
                                         const std::string rtit =
                                             " " + (app.right.cwd.filename().empty() ? std::string("/") : app.right.cwd.filename().string()) + " ";
                                         Elements rhdr;
                                         rhdr.push_back(text(rtit) | (rf ? bold : dim));
                                         rhdr.push_back(filler());
                                         rhdr.push_back(text(app.right.SortLabel() + " ") | color(Color::GrayDark));
                                         rhdr.push_back(text(app.right.view_mode == ViewMode::DETAIL ? "= " : "- ") | color(Color::GrayDark));
                                         Element rbox = window(hbox(std::move(rhdr)), rc) | flex;
                                         if (rf)
                                             rbox = rbox | borderStyled(HEAVY);

                                         // ── Info / preview panel ──
                                         Elements info;
                                         {
                                             const Pane &ap = app.AP();
                                             const Entry *e = ap.Cur();
                                             if (e)
                                             {
                                                 const bool isd = e->is_dir || e->name == "..";
                                                 const Color ec2 = FileColor(e->name, e->is_dir, e->is_link, e->is_exec);
                                                 const std::string lbl = e->name + (isd && e->name != ".." ? "/" : "");
                                                 info.push_back(text(" " + lbl) | color(ec2) | bold);
                                                 info.push_back(separatorEmpty());
                                                 info.push_back(text(" Type  : " + FileTypeLabel(e->name, e->is_dir, e->is_link, e->is_exec)) | color(Color::GrayLight));
                                                 if (e->name != "..")
                                                 {
                                                     if (!isd)
                                                     {
                                                         info.push_back(text(" Size  : " + FormatSizeLong(e->size)) | color(Color::GrayLight));
                                                     }
                                                     else
                                                     {
                                                         int cnt = 0;
                                                         try
                                                         {
                                                             for (auto &entry : fs::directory_iterator(ap.SelPath()))
                                                             {
                                                                 (void)entry;
                                                                 ++cnt;
                                                             }
                                                         }
                                                         catch (...)
                                                         {
                                                         }
                                                         info.push_back(text(" Items : " + std::to_string(cnt)) | color(Color::GrayLight));
                                                     }
                                                     if (!e->mod_time.empty())
                                                         info.push_back(text(" Mod   : " + e->mod_time) | color(Color::GrayLight));
                                                     if (!e->perms_str.empty())
                                                         info.push_back(text(" Perms : " + e->perms_str) | color(Color::GrayLight));
                                                     if (!isd)
                                                     {
                                                         const std::string ext = GetExt(e->name);
                                                         info.push_back(text(" Ext   : " + (ext.empty() ? "--" : ext)) | color(Color::GrayLight));
                                                     }
                                                     if (!e->link_target.empty())
                                                         info.push_back(text(" Link  : " + e->link_target) | color(Color::Cyan));
                                                 }
                                                 if (!ap.sel_set.empty())
                                                 {
                                                     info.push_back(separatorEmpty());
                                                     info.push_back(text(" * " + std::to_string(ap.sel_set.size()) + " selected") | color(Color::Yellow) | bold);
                                                 }
                                                 if (!isd && e->name != "..")
                                                 {
                                                     info.push_back(separator());
                                                     const_cast<Pane &>(ap).Preview();
                                                     const int lc2 = ap.prev_lines;
                                                     const int wc2 = ap.prev_words;
                                                     Elements phdr;
                                                     phdr.push_back(text(" Preview") | bold);
                                                     phdr.push_back(filler());
                                                     phdr.push_back(text(lc2 > 0
                                                                             ? std::to_string(lc2) + "L " + std::to_string(wc2) + "W  "
                                                                             : "") |
                                                                    color(Color::GrayDark));
                                                     info.push_back(hbox(std::move(phdr)));
                                                     info.push_back(separatorEmpty());
                                                     for (const auto &ln : ap.preview_cache)
                                                         info.push_back(text(ln) | color(Color::GrayLight));
                                                 }
                                             }
                                         }
                                         Element info_panel = window(text(" Info "),
                                                                     vbox(std::move(info)) | vscroll_indicator | frame) |
                                                              size(WIDTH, GREATER_THAN, 30) | flex;

                                         // ── Main body ──
                                         Elements body_cols;
                                         Elements panes_row;
                                         panes_row.push_back(lbox);
                                         panes_row.push_back(rbox);
                                         body_cols.push_back(hbox(std::move(panes_row)) | flex_grow);
                                         body_cols.push_back(info_panel);
                                         Element body = hbox(std::move(body_cols));

                                         // ── Breadcrumb ──
                                         const Pane &ap = app.AP();
                                         const std::string cnt_str = " " + std::to_string(ap.entries.size()) + " items";
                                         Elements bc_parts;
                                         bc_parts.push_back(text(" ") | color(Color::GrayDark));
                                         bc_parts.push_back(BuildBreadcrumb(ap.cwd));
                                         bc_parts.push_back(filler());
                                         bc_parts.push_back(text(app.sync_panes ? "  sync" : "") | color(Color::Cyan));
                                         bc_parts.push_back(text(ap.show_hidden ? "  .hid" : "") | color(Color::GrayDark));
                                         bc_parts.push_back(text(cnt_str) | color(Color::GrayDark));
                                         bc_parts.push_back(text(" "));
                                         Element breadcrumb = hbox(std::move(bc_parts));

                                         // ── Status bar ──
                                         const std::string fs_str = ap.filter.empty() ? "" : "  /" + ap.filter;
                                         const std::string cs = app.clipboard.Label();
                                         Elements sb_parts;
                                         sb_parts.push_back(text(" ") | color(Color::GrayDark));
                                         sb_parts.push_back(text(fs_str) | color(Color::Yellow));
                                         sb_parts.push_back(text(cs.empty() ? "" : " [" + cs + "]") | color(Color::MagentaLight));
                                         sb_parts.push_back(filler());
                                         sb_parts.push_back(text(app.status.msg) | color(app.status.col));
                                         sb_parts.push_back(text(" "));
                                         Element statusbar = hbox(std::move(sb_parts));

                                         // ── Footer ──
                                         auto kb = [](const std::string &k, const std::string &d) -> Element
                                         {
                                             return hbox(text(k) | bold | color(Color::CyanLight),
                                                         text(d + "  ") | color(Color::GrayDark));
                                         };
                                         Element footer = hbox(
                                             text(" "),
                                             kb("RET", "open"), kb("e", "edit"), kb("o", "xopen"), kb("t", "term"),
                                             kb("Tab", "pane"), kb("BS", "up"), kb("A-</>", "hist"),
                                             kb("~", "home"), kb("g", "goto"),
                                             kb("Spc", "sel"), kb("A", "all"),
                                             kb("c", "copy"), kb("x", "cut"), kb("p", "paste"),
                                             kb("d", "trash"), kb("D", "del"), kb("u", "undo"),
                                             kb("r", "ren"), kb("m", "mkdir"), kb("n", "touch"),
                                             kb("l", "link"), kb("P", "perms"),
                                             kb("f", "find"), kb("/", "filter"),
                                             kb("s", "sort"), kb("v", "view"),
                                             kb("z", "sync"), kb("w", "swap"),
                                             kb("S", "dsize"), kb("Y", "yank"),
                                             kb("b", "bmarks"), kb("?", "help"), kb("q", "quit"));

                                         Element main_view = vbox(
                                             breadcrumb, separator(),
                                             body | flex,
                                             separator(), statusbar, separator(),
                                             footer | color(Color::GrayDark));

                                         // ── Bookmarks overlay ──
                                         if (app.show_bookmarks)
                                         {
                                             Elements br;
                                             br.push_back(text(" Bookmarks") | bold | color(Color::Yellow));
                                             br.push_back(text("  up/down=nav  Enter=go  B=add  D=del  Esc=close") | color(Color::GrayDark));
                                             br.push_back(separator());
                                             for (int i = 0; i < static_cast<int>(app.bookmarks.list.size()); ++i)
                                             {
                                                 auto entry = text("  " + app.bookmarks.list[i].string());
                                                 if (i == app.bookmarks.sel)
                                                     entry = entry | inverted;
                                                 br.push_back(entry);
                                             }
                                             if (app.bookmarks.list.empty())
                                                 br.push_back(text("  (empty)") | color(Color::GrayDark));
                                             main_view = dbox(
                                                 main_view,
                                                 vbox(std::move(br)) | border | color(Color::Yellow) | clear_under | center);
                                         }

                                         // ── Find overlay ──
                                         if (app.show_find)
                                         {
                                             Elements fr;
                                             fr.push_back(text(" Find Files (recursive)") | bold | color(Color::Cyan));
                                             Elements find_row;
                                             find_row.push_back(text(" Pattern: ") | bold);
                                             find_row.push_back(find_input->Render());
                                             fr.push_back(hbox(std::move(find_row)));
                                             fr.push_back(separator());
                                             if (app.find_results.empty())
                                             {
                                                 fr.push_back(text("  Type to search...") | color(Color::GrayDark));
                                             }
                                             else
                                             {
                                                 const int shown = std::min(static_cast<int>(app.find_results.size()), 20);
                                                 for (int i = 0; i < shown; ++i)
                                                 {
                                                     std::error_code ec2;
                                                     std::string rel;
                                                     try
                                                     {
                                                         rel = fs::relative(app.find_results[i], app.AP().cwd, ec2).string();
                                                     }
                                                     catch (...)
                                                     {
                                                         rel = app.find_results[i].string();
                                                     }
                                                     auto entry2 = text("  " + rel);
                                                     if (i == app.find_sel)
                                                         entry2 = entry2 | inverted | color(Color::Cyan);
                                                     fr.push_back(entry2);
                                                 }
                                                 if (static_cast<int>(app.find_results.size()) > 20)
                                                     fr.push_back(text("  ...and " + std::to_string(app.find_results.size() - 20) + " more") | color(Color::GrayDark));
                                             }
                                             fr.push_back(separator());
                                             fr.push_back(text("  Enter=jump  Esc=close") | color(Color::GrayDark));
                                             main_view = dbox(
                                                 main_view,
                                                 vbox(std::move(fr)) | border | color(Color::Cyan) | clear_under | center);
                                         }

                                         // ── Help overlay ──
                                         if (app.show_help)
                                         {
                                             auto hk = [](const std::string &k, const std::string &d) -> Element
                                             {
                                                 const int pad = std::max(0, 16 - static_cast<int>(k.size()));
                                                 return hbox(
                                                     text(std::string(pad, ' ') + k) | bold | color(Color::CyanLight),
                                                     text(" | " + d) | color(Color::GrayLight));
                                             };
                                             Elements hl;
                                             hl.push_back(text("  fast-tfm v" + VERSION + "  --  Keybindings") | bold | color(Color::White));
                                             hl.push_back(separator());
                                             hl.push_back(hk("up/down  k/j", "Move cursor"));
                                             hl.push_back(hk("PgUp / PgDn", "Jump " + std::to_string(PAGE_JUMP) + " entries"));
                                             hl.push_back(hk("Home / End", "First / last entry"));
                                             hl.push_back(hk("Enter", "Open directory  |  edit file"));
                                             hl.push_back(hk("e", "Open file in $EDITOR explicitly"));
                                             hl.push_back(hk("o", "Open with xdg-open"));
                                             hl.push_back(hk("t", "Open terminal here"));
                                             hl.push_back(hk("Tab", "Switch active pane"));
                                             hl.push_back(hk("Backspace", "Go to parent directory"));
                                             hl.push_back(hk("Alt+Left/Right", "Navigate history back/forward"));
                                             hl.push_back(hk("~", "Jump to $HOME"));
                                             hl.push_back(hk("g", "Go to typed path"));
                                             hl.push_back(separator());
                                             hl.push_back(hk("Space", "Toggle multi-select on entry"));
                                             hl.push_back(hk("A", "Select all / deselect all"));
                                             hl.push_back(hk("c", "Copy selection to clipboard"));
                                             hl.push_back(hk("x", "Cut selection to clipboard"));
                                             hl.push_back(hk("p", "Paste clipboard into active pane"));
                                             hl.push_back(hk("d", "Move to trash  (u = undo)"));
                                             hl.push_back(hk("D", "PERMANENTLY delete selected"));
                                             hl.push_back(hk("u", "Restore last trashed item"));
                                             hl.push_back(hk("r", "Rename selected"));
                                             hl.push_back(hk("m", "New directory  (a/b/c = nested)"));
                                             hl.push_back(hk("n", "New empty file  (touch)"));
                                             hl.push_back(hk("l", "Create symlink from clipboard target"));
                                             hl.push_back(hk("P", "Change permissions  (octal chmod)"));
                                             hl.push_back(hk("Y", "Yank path to system clipboard"));
                                             hl.push_back(separator());
                                             hl.push_back(hk("s", "Cycle sort: ^N vN ^S vS ^D vD ^E"));
                                             hl.push_back(hk(".", "Toggle hidden files"));
                                             hl.push_back(hk("/", "Live filter  (Esc to clear)"));
                                             hl.push_back(hk("v", "Toggle Normal / Detail view"));
                                             hl.push_back(hk("f", "Recursive find overlay"));
                                             hl.push_back(hk("S", "Calculate directory size"));
                                             hl.push_back(hk("z", "Toggle pane sync"));
                                             hl.push_back(hk("w", "Swap pane directories"));
                                             hl.push_back(separator());
                                             hl.push_back(hk("b", "Open bookmarks panel"));
                                             hl.push_back(hk("B", "Bookmark current directory"));
                                             hl.push_back(hk("Ctrl+R", "Force refresh both panes"));
                                             hl.push_back(hk("?", "Toggle this help"));
                                             hl.push_back(hk("q", "Quit"));
                                             hl.push_back(separator());
                                             hl.push_back(text("  Press any key to close") | color(Color::GrayDark));
                                             main_view = dbox(
                                                 main_view,
                                                 vbox(std::move(hl)) | border | color(Color::Cyan) | clear_under | center);
                                         }

                                         // ── Dialog overlay ──
                                         if (app.dialog.active)
                                         {
                                             Elements dl;
                                             dl.push_back(text(" " + app.dialog.title) | bold | color(Color::Yellow));
                                             dl.push_back(separator());
                                             dl.push_back(text(" " + app.dialog.body) | color(Color::White));
                                             if (app.dialog.has_input)
                                             {
                                                 dl.push_back(separatorEmpty());
                                                 Elements input_row;
                                                 input_row.push_back(text(" > ") | bold | color(Color::Cyan));
                                                 input_row.push_back(input_comp->Render());
                                                 dl.push_back(hbox(std::move(input_row)));
                                                 dl.push_back(separatorEmpty());
                                                 dl.push_back(text("  Enter=confirm  Esc=cancel") | color(Color::GrayDark));
                                             }
                                             else
                                             {
                                                 dl.push_back(separatorEmpty());
                                                 dl.push_back(text("  Y=yes  N/Esc=no") | color(Color::GrayDark));
                                             }
                                             main_view = dbox(
                                                 main_view,
                                                 vbox(std::move(dl)) | border | color(Color::Yellow) | clear_under | center);
                                         }

                                         return main_view;
                                     } // end inner scope (lbox declared here)
                                 }); // end Renderer

        // ── Event Handler ────────────────────────────────────────────
        auto component = CatchEvent(renderer, [&](Event ev) -> bool
                                    {
            Pane &ap = app.AP();
            Pane &pp = app.PP();

            // ── Dialog ──
            if (app.dialog.active) {
                if (app.dialog.has_input) {
                    if (ev == Event::Escape) {
                        app.dialog.active = false; app.dialog.input.clear(); return true;
                    }
                    if (ev == Event::Return) {
                        auto fn = app.dialog.cb; auto val = app.dialog.input;
                        app.dialog.active = false; app.dialog.input.clear();
                        if (fn) fn(true, val); return true;
                    }
                    return input_comp->OnEvent(ev);
                } else {
                    if (ev == Event::Character('y') || ev == Event::Character('Y')) {
                        auto fn = app.dialog.cb; app.dialog.active = false;
                        if (fn) fn(true, ""); return true;
                    }
                    app.dialog.active = false; return true;
                }
            }

            // ── Bookmarks panel ──
            if (app.show_bookmarks) {
                if (ev == Event::Escape) { app.show_bookmarks = false; return true; }
                if (ev == Event::ArrowUp || ev == Event::Character('k'))
                { app.bookmarks.sel = std::max(0, app.bookmarks.sel - 1); return true; }
                if (ev == Event::ArrowDown || ev == Event::Character('j'))
                { app.bookmarks.sel = std::min(static_cast<int>(app.bookmarks.list.size()) - 1,
                                               app.bookmarks.sel + 1); return true; }
                if (ev == Event::Return && !app.bookmarks.list.empty()) {
                    app.Navigate(ap, app.AH(), app.bookmarks.list[app.bookmarks.sel]);
                    app.Sync(); app.show_bookmarks = false; return true;
                }
                if (ev == Event::Character('B'))
                { app.bookmarks.Add(ap.cwd); app.status.Ok("Bookmarked"); return true; }
                if (ev == Event::Character('D') && !app.bookmarks.list.empty()) {
                    app.bookmarks.Remove(app.bookmarks.sel);
                    app.bookmarks.sel = std::max(0, app.bookmarks.sel - 1); return true;
                }
                return true;
            }

            // ── Help overlay ──
            if (app.show_help) { app.show_help = false; return true; }

            // ── Find overlay ──
            if (app.show_find) {
                if (ev == Event::Escape) {
                    app.show_find = false; app.find_query.clear(); return true;
                }
                if (ev == Event::ArrowUp || ev == Event::Character('k'))
                { app.find_sel = std::max(0, app.find_sel - 1); return true; }
                if (ev == Event::ArrowDown || ev == Event::Character('j'))
                { app.find_sel = std::min(static_cast<int>(app.find_results.size()) - 1,
                                          app.find_sel + 1); return true; }
                if (ev == Event::Return && !app.find_results.empty()) {
                    app.Navigate(ap, app.AH(), app.find_results[app.find_sel].parent_path());
                    app.Sync(); app.show_find = false; app.find_query.clear(); return true;
                }
                const bool handled = find_input->OnEvent(ev);
                if (handled && !app.find_query.empty()) {
                    app.find_results = FindFiles(ap.cwd, app.find_query, FIND_MAX);
                    app.find_sel = 0;
                }
                return true;
            }

            // ── Filter mode ──
            if (app.filter_mode) {
                if (ev == Event::Escape) {
                    app.filter_mode = false; ap.filter.clear(); ap.Load(); return true;
                }
                if (ev == Event::Backspace && !ap.filter.empty())
                { ap.filter.pop_back(); ap.Load(); return true; }
                if (ev == Event::Return) { app.filter_mode = false; return true; }
                if (ev.is_character()) { ap.filter += ev.character(); ap.Load(); return true; }
                return true;
            }

            // ── Cursor navigation ──
            if (ev == Event::ArrowUp || ev == Event::Character('k'))
            { if (!ap.Empty()) ap.selected = std::max(0, ap.selected - 1); return true; }
            if (ev == Event::ArrowDown || ev == Event::Character('j'))
            { if (!ap.Empty()) ap.selected = std::min(static_cast<int>(ap.entries.size()) - 1,
                                                       ap.selected + 1); return true; }
            if (ev == Event::PageUp)
            { ap.selected = std::max(0, ap.selected - PAGE_JUMP); return true; }
            if (ev == Event::PageDown)
            { ap.selected = std::min(static_cast<int>(ap.entries.size()) - 1,
                                     ap.selected + PAGE_JUMP); return true; }
            if (ev == Event::Home) { ap.selected = 0; return true; }
            if (ev == Event::End)
            { ap.selected = std::max(0, static_cast<int>(ap.entries.size()) - 1); return true; }

            // ── Enter: open dir / launch editor ──
            if (ev == Event::Return) {
                if (ap.SelIsDir()) {
                    app.Navigate(ap, app.AH(), ap.SelPath());
                    ap.ClearSel(); app.Sync();
                } else if (!ap.Empty()) {
                    app.pending_editor = ap.SelPath().string();
                    screen.Exit();
                }
                return true;
            }

            // ── Backspace: parent ──
            if (ev == Event::Backspace) {
                if (ap.cwd != ap.cwd.root_path()) {
                    app.Navigate(ap, app.AH(), ap.cwd.parent_path());
                    app.Sync();
                }
                return true;
            }

            // ── Alt+Left: history back  (ESC [ 1 ; 3 D  or  ESC ESC [ D) ──
            if (ev == Event::Special("\x1b[1;3D") ||
                ev == Event::Special("\x1b\x1b[D")) {
                if (auto dest = app.AH().Back()) {
                    ap.cwd = *dest; ap.selected = 0; ap.Load(); app.Sync();
                    app.status.Info("History back");
                }
                return true;
            }
            // ── Alt+Right: history forward  (ESC [ 1 ; 3 C  or  ESC ESC [ C) ──
            if (ev == Event::Special("\x1b[1;3C") ||
                ev == Event::Special("\x1b\x1b[C")) {
                if (auto dest = app.AH().Forward()) {
                    ap.cwd = *dest; ap.selected = 0; ap.Load(); app.Sync();
                    app.status.Info("History forward");
                }
                return true;
            }

            // ── Tab: switch pane ──
            if (ev == Event::Tab || ev == Event::Character('\t'))
            { app.active_left = !app.active_left; return true; }

            // ── ~ : home ──
            if (ev == Event::Character('~')) {
                if (HOME) { app.Navigate(ap, app.AH(), fs::path(HOME)); app.Sync(); }
                return true;
            }

            // ── g : goto path ──
            if (ev == Event::Character('g')) {
                app.dialog.TextInput("Go To Path", "Enter path (absolute or relative):",
                    [&](bool ok, const std::string &p) {
                        if (!ok || p.empty()) return;
                        fs::path t = p[0] == '/' ? fs::path(p) : ap.cwd / p;
                        std::error_code ec;
                        const auto c = fs::canonical(t, ec);
                        if (!ec && fs::is_directory(c, ec)) {
                            app.Navigate(ap, app.AH(), c); app.Sync();
                            app.status.Ok("Navigated");
                        } else {
                            app.status.Err("Invalid path: " + p);
                        }
                    });
                return true;
            }

            // ── e : editor ──
            if (ev == Event::Character('e')) {
                if (!ap.SelIsDir() && !ap.Empty())
                { app.pending_editor = ap.SelPath().string(); screen.Exit(); }
                return true;
            }

            // ── o : xdg-open ──
            if (ev == Event::Character('o')) {
                if (!ap.Empty() && ap.Cur() && ap.Cur()->name != "..")
                { app.pending_xopen = ap.SelPath().string(); screen.Exit(); }
                return true;
            }

            // ── t : terminal ──
            if (ev == Event::Character('t')) {
                const std::string cwd_s = ap.cwd.string();
                screen.WithRestoredIO([&] {
                    const char *T = std::getenv("TERMINAL");
                    const std::string term = T ? T : "xterm";
                    (void)std::system(("cd \"" + cwd_s + "\" && " + term + " &").c_str());
                })();
                return true;
            }

            // ── Y : yank path ──
            if (ev == Event::Character('Y')) {
                if (!ap.Empty()) {
                    YankToClipboard(ap.SelPath().string());
                    app.status.Ok("Yanked: " + ap.SelPath().string());
                }
                return true;
            }

            // ── Space : multi-select ──
            if (ev == Event::Character(' ')) {
                ap.ToggleSel();
                if (ap.selected < static_cast<int>(ap.entries.size()) - 1) ap.selected++;
                return true;
            }

            // ── A : select all / deselect all ──
            if (ev == Event::Character('A')) {
                if (ap.sel_set.empty()) {
                    for (int i = 1; i < static_cast<int>(ap.entries.size()); ++i)
                        ap.sel_set.insert(i);
                    app.status.Info("Selected " + std::to_string(ap.sel_set.size()));
                } else {
                    ap.ClearSel(); app.status.Info("Deselected all");
                }
                return true;
            }

            // ── c : copy ──
            if (ev == Event::Character('c')) {
                auto sel = ap.GetSelection();
                if (!sel.empty()) {
                    app.clipboard.Set(sel, false); ap.ClearSel();
                    app.status.Ok("Copied " + std::to_string(sel.size()) + " item(s)");
                }
                return true;
            }

            // ── x : cut ──
            if (ev == Event::Character('x')) {
                auto sel = ap.GetSelection();
                if (!sel.empty()) {
                    app.clipboard.Set(sel, true); ap.ClearSel();
                    app.status.Ok("Cut " + std::to_string(sel.size()) + " item(s)");
                }
                return true;
            }

            // ── p : paste ──
            if (ev == Event::Character('p')) {
                if (!app.clipboard.has) { app.status.Err("Nothing in clipboard"); return true; }
                int ok = 0, fail = 0; std::string lerr;
                for (const auto &src : app.clipboard.paths) {
                    const fs::path dst = ap.cwd / src.filename();
                    std::string err;
                    const bool r = app.clipboard.is_cut
                        ? OpMove(src, dst, err) : OpCopy(src, dst, err);
                    if (r) ++ok; else { ++fail; lerr = err; }
                }
                if (app.clipboard.is_cut && fail == 0) app.clipboard.Clear();
                ap.Load();
                if (fail == 0) app.status.Ok("Pasted " + std::to_string(ok));
                else           app.status.Err(std::to_string(fail) + " failed: " + lerr);
                return true;
            }

            // ── d : trash ──
            if (ev == Event::Character('d')) {
                auto sel = ap.GetSelection(); if (sel.empty()) return true;
                const std::string desc = sel.size() == 1
                    ? sel[0].filename().string()
                    : std::to_string(sel.size()) + " items";
                app.dialog.YesNo("Trash", "Move '" + desc + "' to trash? (u=undo)",
                    [&, sel](bool yes, const std::string &) {
                        if (!yes) return;
                        int ok = 0; std::string err;
                        for (const auto &p : sel) if (OpTrash(p, err)) ++ok;
                        ap.Load(); ap.ClearSel();
                        app.status.Ok("Trashed " + std::to_string(ok));
                    });
                return true;
            }

            // ── D : permanent delete ──
            if (ev == Event::Character('D')) {
                auto sel = ap.GetSelection(); if (sel.empty()) return true;
                const std::string desc = sel.size() == 1
                    ? sel[0].filename().string()
                    : std::to_string(sel.size()) + " items";
                app.dialog.YesNo("PERMANENT DELETE",
                    "Delete '" + desc + "' FOREVER?  Irreversible!",
                    [&, sel](bool yes, const std::string &) {
                        if (!yes) return;
                        int ok = 0; std::string err;
                        for (const auto &p : sel) if (OpDelete(p, err)) ++ok;
                        ap.Load(); ap.ClearSel();
                        app.status.Ok("Deleted " + std::to_string(ok));
                    });
                return true;
            }

            // ── u : restore trash ──
            if (ev == Event::Character('u')) {
                std::string name, err;
                if (OpRestoreTrash(name, err)) { ap.Load(); app.status.Ok("Restored: " + name); }
                else app.status.Err(err);
                return true;
            }

            // ── r : rename ──
            if (ev == Event::Character('r')) {
                if (ap.Empty() || ap.SelName() == "..") return true;
                const fs::path path2 = ap.SelPath();
                const std::string orig = ap.SelName();
                app.dialog.TextInput("Rename", "New name for '" + orig + "':",
                    [&, path2](bool ok, const std::string &nm) {
                        if (!ok || nm.empty()) return;
                        std::string err;
                        if (OpRename(path2, nm, err)) { ap.Load(); app.status.Ok("-> " + nm); }
                        else app.status.Err(err);
                    }, orig);
                return true;
            }

            // ── m : mkdir ──
            if (ev == Event::Character('m')) {
                app.dialog.TextInput("New Directory", "Name (a/b/c creates nested):",
                    [&](bool ok, const std::string &nm) {
                        if (!ok || nm.empty()) return;
                        std::string err;
                        if (OpMkdir(ap.cwd / nm, err)) { ap.Load(); app.status.Ok("Created: " + nm); }
                        else app.status.Err(err);
                    });
                return true;
            }

            // ── n : touch ──
            if (ev == Event::Character('n')) {
                app.dialog.TextInput("New File", "File name:",
                    [&](bool ok, const std::string &nm) {
                        if (!ok || nm.empty()) return;
                        std::string err;
                        if (OpTouch(ap.cwd / nm, err)) { ap.Load(); app.status.Ok("Created: " + nm); }
                        else app.status.Err(err);
                    });
                return true;
            }

            // ── l : symlink ──
            if (ev == Event::Character('l')) {
                if (!app.clipboard.has || app.clipboard.paths.empty()) {
                    app.status.Err("Copy a file (c) first to use as symlink target"); return true;
                }
                const fs::path tgt  = app.clipboard.paths[0];
                const fs::path link = ap.cwd / tgt.filename();
                std::string err;
                if (OpSymlink(tgt, link, err)) { ap.Load(); app.status.Ok("Symlink created"); }
                else app.status.Err(err);
                return true;
            }

            // ── P : chmod ──
            if (ev == Event::Character('P')) {
                if (ap.Empty() || ap.SelName() == "..") return true;
                const fs::path path3 = ap.SelPath();
                app.dialog.TextInput("Change Permissions", "Octal mode (e.g. 755, 644):",
                    [&, path3](bool ok, const std::string &mode) {
                        if (!ok || mode.empty()) return;
                        std::string err;
                        if (OpChmod(path3, mode, err))
                        { ap.Load(); app.status.Ok("Permissions: " + mode); }
                        else app.status.Err(err);
                    });
                return true;
            }

            // ── S : directory size ──
            if (ev == Event::Character('S')) {
                if (ap.SelIsDir() && ap.Cur() && ap.Cur()->name != "..") {
                    app.status.Info("Calculating...");
                    const auto sz = DirSize(ap.SelPath());
                    app.status.Info(ap.SelName() + ": " + FormatSizeLong(sz));
                }
                return true;
            }

            // ── s : sort ──
            if (ev == Event::Character('s')) {
                ap.CycleSortForward(); ap.Load();
                app.status.Info("Sort: " + ap.SortLabel());
                return true;
            }

            // ── . : toggle hidden ──
            if (ev == Event::Character('.')) {
                ap.show_hidden = !ap.show_hidden; ap.Load();
                app.status.Info(ap.show_hidden ? "Showing hidden files" : "Hidden files off");
                return true;
            }

            // ── v : view mode ──
            if (ev == Event::Character('v')) {
                ap.view_mode = (ap.view_mode == ViewMode::NORMAL)
                    ? ViewMode::DETAIL : ViewMode::NORMAL;
                app.status.Info(ap.view_mode == ViewMode::DETAIL ? "Detail view" : "Normal view");
                return true;
            }

            // ── / : filter ──
            if (ev == Event::Character('/')) {
                app.filter_mode = true; ap.filter.clear(); ap.Load(); return true;
            }

            // ── Esc : clear ──
            if (ev == Event::Escape) {
                if (!ap.filter.empty()) { ap.filter.clear(); ap.Load(); }
                ap.ClearSel(); app.status.Clear(); return true;
            }

            // ── f : find ──
            if (ev == Event::Character('f')) {
                app.show_find = true; app.find_query.clear();
                app.find_results.clear(); app.find_sel = 0;
                return true;
            }

            // ── z : sync panes ──
            if (ev == Event::Character('z')) {
                app.sync_panes = !app.sync_panes;
                app.status.Info(app.sync_panes ? "Pane sync ON" : "Pane sync OFF");
                if (app.sync_panes) { pp.cwd = ap.cwd; pp.Load(); }
                return true;
            }

            // ── w : swap panes ──
            if (ev == Event::Character('w')) {
                std::swap(app.left.cwd, app.right.cwd);
                app.left.selected = 0; app.right.selected = 0;
                app.left.Load(); app.right.Load();
                app.status.Info("Panes swapped");
                return true;
            }

            // ── b / B : bookmarks ──
            if (ev == Event::Character('b')) { app.show_bookmarks = true; return true; }
            if (ev == Event::Character('B')) {
                app.bookmarks.Add(ap.cwd);
                app.status.Ok("Bookmarked: " + ap.cwd.string());
                return true;
            }

            // ── Ctrl+R : refresh ──
            if (ev == Event::Character('\x12')) {
                ap.Load(); pp.Load(); app.status.Info("Refreshed"); return true;
            }

            // ── ? : help ──
            if (ev == Event::Character('?')) { app.show_help = true; return true; }

            // ── q : quit ──
            if (ev == Event::Character('q')) {
                app.should_quit = true; screen.Exit(); return true;
            }

            return false; }); // end CatchEvent

        screen.Loop(component);
        if (app.should_quit)
            break;

        // ── Post-loop: launch editor ──
        if (!app.pending_editor.empty())
        {
            const char *ed = std::getenv("EDITOR");
            if (!ed)
                ed = std::getenv("VISUAL");
            const std::string editor = ed ? ed : "nano";
            std::string safe_path;
            for (char c : app.pending_editor)
                safe_path += (c == '"') ? "\\\"" : std::string(1, c);
            (void)std::system((editor + " \"" + safe_path + "\"").c_str());
            app.left.Load();
            app.right.Load();
        }

        // ── Post-loop: xdg-open ──
        if (!app.pending_xopen.empty())
        {
            std::string safe_path;
            for (char c : app.pending_xopen)
                safe_path += (c == '"') ? "\\\"" : std::string(1, c);
            (void)std::system(("xdg-open \"" + safe_path + "\" >/dev/null 2>&1 &").c_str());
        }

    } // end while(!app.should_quit)

    return 0;
}