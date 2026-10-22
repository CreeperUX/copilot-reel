#pragma once
#include <algorithm>
#include <array>
#include <cstddef>
#include <cwctype>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The agent catalog and the pure rules that decide whether an executable on disk is
// one of its products. Nothing here reads the machine: AgentPresets() (src/presets.cpp)
// feeds it Start-menu shortcut targets, Uninstall values and exact folder probes.
namespace reel::catalog {
enum class Agent {ChatGpt,Claude,ClaudeScience,Antigravity,ZCode,Dsh,VsCode,JetBrains,Junie,
                  Cursor,GrokBot,Zed,Windsurf,Kiro,Trae,TraeCn,Count};

// Products in Add-page order. Core rows have bundled artwork and appear even when the
// product is missing (an empty target); the others appear only when installed and use
// their executable's own Shell icon. Names are product names in both UI languages.
struct Entry {
    std::wstring_view name;
    bool core=false;
    std::wstring_view aumid{};  // a registered package wins over every Win32 rule
};
inline constexpr Entry kEntries[]={
    // The package keeps the legacy OpenAI.Codex identity while its manifest and visible
    // app name are ChatGPT. There is no separate Codex activation ID to invent.
    {L"ChatGPT / Codex",true,L"OpenAI.Codex_2p2nqsd0c76g0!App"},
    {L"Claude Desktop",true,L"Claude_pzs8sxrjxfjjc!Claude"},
    {L"Claude Science",true},{L"Antigravity",true},{L"ZCode",true},{L"DeepSeek Harness (DSH)",true},
    {L"Visual Studio Code",true},{L"JetBrains IDE",true},
    // Junie runs in several hosts and has no desktop executable of its own. Users choose
    // their actual host rather than a guessed standalone URI or an assumed IDE plugin.
    {L"Junie",true},
    {L"Cursor"},{L"Grok Bot"},{L"Zed"},{L"Windsurf / Devin"},{L"Kiro"},{L"Trae"},{L"Trae CN"}};
static_assert(std::size(kEntries)==static_cast<std::size_t>(Agent::Count));

// Exact folders checked without recursion when no shortcut or Uninstall entry names the
// executable. Programs: %LOCALAPPDATA%\Programs\<folder> and %ProgramFiles%\<folder>.
enum class Probe {None,Programs,LocalAppData};

// One Win32 install layout: the executable's file name and its install folder, the exe's
// parent. An IDE sits in <folder>\bin\, where the folder carries an edition or version
// ("PyCharm 2025.2"), so `ide` matches it as a prefix and keeps one row per install.
struct Rule {
    Agent agent;
    std::wstring_view exe,folder;
    std::wstring_view name{};  // a channel's row name; empty uses the entry's
    Probe probe=Probe::Programs;
    bool ide=false;
};
constexpr Rule Ide(std::wstring_view exe,std::wstring_view folder) {return {Agent::JetBrains,exe,folder,{},Probe::None,true};}
inline constexpr Rule kRules[]={
    // The legacy Squirrel stub, never its app-<version>\ copy; used only without the package.
    {Agent::Claude,L"claude.exe",L"AnthropicClaude",{},Probe::LocalAppData},
    {Agent::ClaudeScience,L"claude-science.exe",L"ClaudeScience"},
    {Agent::Antigravity,L"Antigravity.exe",L"Antigravity"},
    {Agent::ZCode,L"ZCode.exe",L"ZCode"},
    {Agent::Dsh,L"DeepSeek Harness.exe",L"DeepSeek Harness"},
    // Exact: Microsoft VS Code Insiders is a separate product.
    {Agent::VsCode,L"Code.exe",L"Microsoft VS Code"},
    // Toolbox 1.x builds sit under a build-number folder and are never stored.
    Ide(L"idea64.exe",L"IntelliJ IDEA"),Ide(L"pycharm64.exe",L"PyCharm"),Ide(L"webstorm64.exe",L"WebStorm"),
    Ide(L"rider64.exe",L"Rider"),Ide(L"rider64.exe",L"JetBrains Rider"),Ide(L"clion64.exe",L"CLion"),
    Ide(L"goland64.exe",L"GoLand"),Ide(L"phpstorm64.exe",L"PhpStorm"),Ide(L"rubymine64.exe",L"RubyMine"),
    Ide(L"rustrover64.exe",L"RustRover"),
    {Agent::Cursor,L"Cursor.exe",L"cursor"},
    {Agent::GrokBot,L"Grok Bot.exe",L"Grok Bot"},
    // Every channel ships Zed.exe; its folder tells them apart. bin\zed.exe is the CLI.
    {Agent::Zed,L"Zed.exe",L"Zed"},
    {Agent::Zed,L"Zed.exe",L"Zed Preview",L"Zed Preview"},
    {Agent::Zed,L"Zed.exe",L"Zed Nightly",L"Zed Nightly"},
    {Agent::Windsurf,L"Windsurf.exe",L"Windsurf",L"Windsurf"},
    // Windsurf renamed to Devin Desktop; its executable name is not confirmed on an install yet.
    {Agent::Windsurf,L"Devin.exe",L"Devin",L"Devin Desktop"},
    {Agent::Windsurf,L"Windsurf.exe",L"Devin",L"Devin Desktop"},
    {Agent::Kiro,L"Kiro.exe",L"Kiro"},
    {Agent::Trae,L"Trae.exe",L"Trae"},
    {Agent::TraeCn,L"Trae CN.exe",L"Trae CN"}};

inline constexpr unsigned kGuiSubsystem=2;  // IMAGE_SUBSYSTEM_WINDOWS_GUI; 3 is a console program

inline const Entry& EntryOf(const Rule& rule) {return kEntries[static_cast<std::size_t>(rule.agent)];}
inline std::wstring_view RowName(const Rule& rule) {return rule.name.empty()?EntryOf(rule).name:rule.name;}

inline bool Same(std::wstring_view a,std::wstring_view b) {
    return a.size()==b.size() && std::equal(a.begin(),a.end(),b.begin(),
        [](wchar_t x,wchar_t y){return std::towlower(x)==std::towlower(y);});
}
inline bool StartsWith(std::wstring_view text,std::wstring_view prefix) {
    return text.size()>=prefix.size() && Same(text.substr(0,prefix.size()),prefix);
}
inline bool EndsWith(std::wstring_view text,std::wstring_view suffix) {
    return text.size()>=suffix.size() && Same(text.substr(text.size()-suffix.size()),suffix);
}
inline std::wstring Lower(std::wstring_view value) {
    std::wstring result(value);
    std::transform(result.begin(),result.end(),result.begin(),[](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    return result;
}

// The last three segments of a path, file name first; both separators count.
struct Tail {std::wstring_view file,parent,grandparent;};
inline Tail Split(std::wstring_view path) {
    auto next=[&path] {
        const auto separator=path.find_last_of(L"\\/");
        const auto part=separator==std::wstring_view::npos?path:path.substr(separator+1);
        path=separator==std::wstring_view::npos?std::wstring_view():path.substr(0,separator);
        return part;
    };
    Tail tail;tail.file=next();tail.parent=next();tail.grandparent=next();
    return tail;
}

// The folder an install is named by: the exe's parent, or the folder above bin\ for an IDE.
inline std::wstring_view InstallFolder(std::wstring_view path,const Rule& rule) {
    const auto tail=Split(path);return rule.ide?tail.grandparent:tail.parent;
}
inline bool FolderMatches(const Rule& rule,std::wstring_view folder) {
    return rule.ide?StartsWith(folder,rule.folder):Same(folder,rule.folder);
}

// The rule an executable belongs to, by its file name AND its install folder. A display
// name or shortcut stem alone never identifies a product.
inline const Rule* MatchRule(std::wstring_view path) {
    const auto tail=Split(path);
    for(const auto& rule:kRules) {
        if(!Same(tail.file,rule.exe))continue;
        if(rule.ide?Same(tail.parent,L"bin") && FolderMatches(rule,tail.grandparent):FolderMatches(rule,tail.parent))return &rule;
    }
    return nullptr;
}

// unins000.exe, uninst.exe and "Uninstall X.exe" ship beside the product, as do
// Squirrel's Update.exe and Inno's inno_updater.exe.
inline bool IsUninstallerOrUpdater(std::wstring_view file) {
    return StartsWith(file,L"unins") || Same(file,L"update.exe") || Same(file,L"inno_updater.exe");
}

// Updater staging folders: Inno's _\ and _bak\ (Cursor) and Squirrel's app-<version>\.
// Only the executable's own folder counts, so a profile or drive folder with such a
// name above a valid install never hides it.
inline bool HasStagingSegment(std::wstring_view path) {
    const auto separator=path.find_last_of(L"\\/");
    if(separator==std::wstring_view::npos)return false;
    path=path.substr(0,separator);
    const auto segment=path.substr(path.find_last_of(L"\\/")==std::wstring_view::npos?0:path.find_last_of(L"\\/")+1);
    return Same(segment,L"_") || Same(segment,L"_bak") ||
           (StartsWith(segment,L"app-") && segment.size()>4 && std::iswdigit(segment[4]));
}

// Accepts an existing executable by its path and PE subsystem: a GUI program that is
// not an uninstaller, an updater or a staged copy, and that matches a rule.
inline const Rule* MatchCandidate(std::wstring_view path,unsigned subsystem) {
    if(subsystem!=kGuiSubsystem || HasStagingSegment(path) || IsUninstallerOrUpdater(Split(path).file))return nullptr;
    return MatchRule(path);
}

// A shortcut stores its target's file name as ANSI and UTF-16LE text (link info, ID
// list, environment block). Loading a link through IShellLink costs about 1 ms, so
// discovery loads only links whose bytes name a rule's executable, ignoring ASCII case.
// A miss cannot point at a catalog executable; a false hit only costs one load.
inline bool MayNameRuleExe(std::string_view bytes) {
    // Lowercase names in both encodings; each ends with its encoding's ".exe" anchor.
    static const auto patterns=[] {
        std::array<std::vector<std::string>,2> result;
        for(const auto& rule:kRules) {
            std::string narrow,wide;
            for(const wchar_t c:rule.exe) {
                const char ascii=static_cast<char>(std::towlower(c));
                narrow+=ascii;wide+=ascii;wide+='\0';
            }
            result[0].push_back(std::move(narrow));result[1].push_back(std::move(wide));
        }
        return result;
    }();
    std::string folded(bytes);
    for(auto& c:folded)if(c>='A' && c<='Z')c=static_cast<char>(c-'A'+'a');
    const std::string_view text(folded),anchors[]={".exe",std::string_view(".\0e\0x\0e\0",8)};
    for(size_t encoding=0;encoding<2;++encoding) {
        const auto anchor=anchors[encoding];
        for(auto at=text.find(anchor);at!=std::string_view::npos;at=text.find(anchor,at+1)) {
            const auto end=at+anchor.size();
            for(const auto& pattern:patterns[encoding])
                if(pattern.size()<=end && text.substr(end-pattern.size(),pattern.size())==pattern)return true;
        }
    }
    return false;
}

// An Uninstall DisplayIcon is "path[,index]", optionally quoted. Returns the path when
// it names an executable; ZCode and others point it at an .ico instead.
inline std::wstring ExeFromDisplayIcon(std::wstring_view value) {
    auto trim=[](std::wstring_view text) {
        while(!text.empty() && std::iswspace(text.front()))text.remove_prefix(1);
        while(!text.empty() && std::iswspace(text.back()))text.remove_suffix(1);
        return text;
    };
    value=trim(value);
    if(value.starts_with(L'"')) {
        const auto close=value.find(L'"',1);
        if(close==std::wstring_view::npos)return {};
        value=value.substr(1,close-1);
    } else if(const auto comma=value.rfind(L',');comma!=std::wstring_view::npos) {
        auto index=trim(value.substr(comma+1));
        if(index.starts_with(L'-'))index.remove_prefix(1);
        if(!index.empty() && std::all_of(index.begin(),index.end(),[](wchar_t c){return c>=L'0' && c<=L'9';}))
            value=value.substr(0,comma);
    }
    value=trim(value);
    return EndsWith(value,L".exe")?std::wstring(value):std::wstring();
}
}
