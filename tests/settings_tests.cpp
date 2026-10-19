// Compile this test as a separate executable, without separately compiling
// settings.cpp. The include keeps parser details private to production while
// making the anonymous-namespace helpers available in this translation unit.
// No test calls ConfigPath, LoadTargets, SaveTargets, or any window API: the
// real user's configuration is never read or written.
#include "../src/settings.cpp"

#include <iostream>
#include <stdexcept>

namespace {
unsigned int checks = 0;

void Require(bool condition, const char* description) {
    ++checks;
    if (!condition) throw std::runtime_error(description);
}

std::wstring Replace(std::wstring source, std::wstring_view before, std::wstring_view after) {
    const auto position = source.find(before);
    if (position == std::wstring::npos) throw std::runtime_error("invalid test fixture replacement");
    source.replace(position, before.size(), after);
    return source;
}

const std::wstring kFixture = L"; Copilot Reel configuration\n[CopilotReel]\nVersion=1\nCount=3\n\n[App0]\nName=\u4e2d\u6587 Agent \u5de5\u4f5c\u53f0\nKind=shell\nTarget=C:\\\\Program Files\\\\\u4e2d\u6587\u5e94\u7528\\\\agent.exe\nArguments=--profile=\"\u5de5\u4f5c \u7a7a\u95f4\" --mode=a=b\nWorkingDirectory=C:\\\\\u7528\u6237\\\\\u793a\u4f8b \u6587\u4ef6\u5939\n\n[App1]\nName=Web query\nKind=shell\nTarget=https://example.test/chat?q=a=b&agent=\u4e2d\u6587%20\u52a9\u624b#main\nArguments=\nWorkingDirectory=\n\n[App2]\nName=Packaged app\nKind=aumid\nTarget=Example.Agent_123456789abcd!App\nArguments=conversation=123\nWorkingDirectory=\n";

void TestFixture() {
    std::vector<reel::AppTarget> apps;
    Require(reel::Parse(kFixture, apps), "valid Unicode configuration should parse");
    Require(apps.size() == 3, "entry count and order should be preserved");
    Require(apps[0].name == L"\u4e2d\u6587 Agent \u5de5\u4f5c\u53f0", "Chinese name and internal spaces must survive");
    Require(apps[0].target == L"C:\\Program Files\\\u4e2d\u6587\u5e94\u7528\\agent.exe", "Windows backslashes and path spaces must survive");
    Require(apps[0].arguments == L"--profile=\"\u5de5\u4f5c \u7a7a\u95f4\" --mode=a=b", "quotes and equals in arguments must survive");
    Require(apps[0].workingDirectory == L"C:\\\u7528\u6237\\\u793a\u4f8b \u6587\u4ef6\u5939", "working directory must survive");
    Require(apps[1].target == L"https://example.test/chat?q=a=b&agent=\u4e2d\u6587%20\u52a9\u624b#main", "URI query equals, ampersands, percent escapes, and fragment must survive");
    Require(apps[1].arguments.empty() && apps[1].workingDirectory.empty(), "optional empty values must stay empty");
    Require(apps[2].kind == reel::TargetKind::Aumid && apps[2].target == L"Example.Agent_123456789abcd!App", "AUMID must preserve package identity and entrypoint");

    std::wstring crlf;
    for (wchar_t c : kFixture) {
        if (c == L'\n') crlf += L'\r';
        crlf += c;
    }
    Require(reel::Parse(crlf, apps), "Windows CRLF lines should parse");
    Require(reel::Parse(kFixture.substr(0, kFixture.size() - 1), apps), "final newline is optional");
}

void TestEscaping() {
    const std::wstring original = L"  \u4e2d\u6587 \\server\\\u5171\u4eab\\agent.lnk\t--key=a=b&x=1  ";
    const auto encoded = reel::Encode(original);
    Require(encoded == L"  \u4e2d\u6587 \\\\server\\\\\u5171\u4eab\\\\agent.lnk\\t--key=a=b&x=1  ", "encoding must escape backslashes and tabs without trimming values");
    std::wstring decoded;
    Require(reel::Decode(encoded, decoded) && decoded == original, "escaped text should round-trip exactly");
    Require(reel::Decode(L"", decoded) && decoded.empty(), "empty optional field should round-trip");
    Require(!reel::Decode(L"dangling\\", decoded), "dangling escape must be rejected");
    Require(!reel::Decode(L"unsupported\\q", decoded), "unknown escape must be rejected instead of changing a path");
}

void TestCorruptConfigurations() {
    std::vector<reel::AppTarget> apps;
    const std::vector<std::pair<std::wstring, const char*>> invalid{
        {L"", "empty file should fail"},
        {Replace(kFixture, L"Version=1", L"Version=2"), "unsupported version should fail"},
        {Replace(kFixture, L"Count=3", L"Count=0"), "zero entries should fail"},
        {Replace(kFixture, L"Count=3", L"Count=9"), "more than eight entries should fail"},
        {Replace(kFixture, L"Count=3", L"Count=2"), "declared count mismatch should fail"},
        {Replace(kFixture, L"Count=3", L"Count=3\nCount=3"), "duplicate header key should fail"},
        {kFixture + L"\n[App0]\n", "duplicate section should fail"},
        {Replace(kFixture, L"[App1]", L"[App7]"), "noncontiguous indexes should fail"},
        {Replace(kFixture, L"Name=Web query", L"Name=Web query\nName=Second"), "duplicate application key should fail"},
        {Replace(kFixture, L"Kind=aumid", L"Kind=unknown"), "unknown launch kind should fail"},
        {Replace(kFixture, L"Name=Web query", L"Name= \t "), "whitespace-only name should fail"},
        {Replace(kFixture, L"Target=Example.Agent_123456789abcd!App", L"Target="), "empty launch target should fail"},
        {Replace(kFixture, L"Arguments=conversation=123\n", L""), "missing optional-valued field should fail"},
        {Replace(kFixture, L"Arguments=conversation=123", L"Arguments=bad\\q"), "invalid escape should fail"},
        {kFixture + L"\n[Unknown]\nKey=value\n", "unknown section should fail"},
        {L"Name=orphan\n" + kFixture, "key outside a section should fail"}
    };
    for (const auto& [fixture, description] : invalid) Require(!reel::Parse(fixture, apps), description);
}

void TestLimits() {
    const reel::AppTarget valid{L"Agent", reel::TargetKind::Shell, L"https://example.test", L"", L""};
    std::wstring error;
    Require(!reel::Validate({}, error) && !error.empty(), "empty application list should return an error");
    Require(reel::Validate({valid}, error), "one valid application is supported");
    Require(reel::Validate(std::vector<reel::AppTarget>(8, valid), error), "eight applications are supported");
    Require(!reel::Validate(std::vector<reel::AppTarget>(9, valid), error), "nine applications must be rejected");

    auto app = valid;
    app.name = std::wstring(128, L'\u540d');
    Require(reel::Validate({app}, error), "128 UTF-16 name units are supported");
    app.name += L'\u79f0';
    Require(!reel::Validate({app}, error), "overlong name must be rejected");
    app = valid;
    app.target = std::wstring(8192, L'x');
    Require(reel::Validate({app}, error), "8192-unit target is supported");
    app.target += L'x';
    Require(!reel::Validate({app}, error), "overlong target must be rejected");
    app = valid;
    app.arguments = std::wstring(8193, L'x');
    Require(!reel::Validate({app}, error), "overlong arguments must be rejected");
    app = valid;
    app.workingDirectory = std::wstring(8193, L'x');
    Require(!reel::Validate({app}, error), "overlong working directory must be rejected");

    for (wchar_t forbidden : {L'\0', L'\r', L'\n'}) {
        app = valid;
        app.target += forbidden;
        app.target += L"hidden suffix";
        Require(!reel::Validate({app}, error), "embedded NUL or newline must not truncate or inject configuration content");
    }
    app = valid;
    app.kind = static_cast<reel::TargetKind>(100);
    Require(!reel::Validate({app}, error), "invalid enum value must be rejected");

    auto defaults = reel::Defaults();
    Require(defaults.size() == 6 && reel::Validate(defaults, error), "first-run defaults must be valid and bounded");
    Require(defaults[0].target == L"https://chatgpt.com" && defaults[1].target == L"https://claude.ai", "default agents must not assume installed package identities");
}
} // namespace

int main() {
    try {
        TestFixture();
        TestEscaping();
        TestCorruptConfigurations();
        TestLimits();
        std::cout << "Settings tests passed: " << checks << " checks; no user files accessed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Settings test failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
