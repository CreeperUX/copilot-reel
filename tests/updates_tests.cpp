#include "updates.h"
#include <winrt/base.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

// The update logic without the network: versions, the choice among GitHub releases, identities,
// and the signature check on the signed package in artifacts/ and on a tampered copy of it.
namespace {
unsigned checks = 0;
void Check(bool okay, const char* label) {
    ++checks;
    if (!okay) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
using reel::PackageVersion;

constexpr wchar_t kReleases[] = LR"([
  {"tag_name":"v0.4.0-beta","draft":true,"assets":[{"name":"CopilotReel_0.4.0.0_x64.msix","browser_download_url":"https://example.invalid/draft","size":9}]},
  {"tag_name":"v0.3.9-beta","draft":false,"prerelease":true,"assets":[
    {"name":"CopilotReel_0.3.9.0_arm64.msix","browser_download_url":"https://github.com/o/r/releases/download/v0.3.9-beta/CopilotReel_0.3.9.0_arm64.msix","size":1404648,"digest":"sha256:CCC06E1A2F4BE5FF373834F0ACF4BBCC7531DDA00009E30CDC009D23E93EAC6B"},
    {"name":"CopilotReel_0.3.9.0_x64.msix","browser_download_url":"https://github.com/o/r/releases/download/v0.3.9-beta/CopilotReel_0.3.9.0_x64.msix","size":1436701,"digest":null},
    {"name":"CopilotReel-Development.cer","browser_download_url":"https://github.com/o/r/releases/download/v0.3.9-beta/CopilotReel-Development.cer","size":771}]},
  {"tag_name":"v0.3.8.5-beta","draft":false,"assets":[
    {"name":"CopilotReel_0.3.8.4_x64.msix","browser_download_url":"https://github.com/o/r/a.msix","size":1}]},
  {"tag_name":"v0.3.8-beta","draft":false,"assets":[
    {"name":"CopilotReel_0.3.8.0_x64.msix","browser_download_url":"http://github.com/o/r/plain.msix","size":1},
    {"name":"CopilotReel_0.3.8_x64.msix","browser_download_url":"https://github.com/o/r/b.msix","size":2,"digest":"sha256:00"}]},
  {"tag_name":"nightly","assets":[]},
  "not a release"
])";

std::filesystem::path Artifacts() {
    wchar_t module[MAX_PATH]{};
    GetModuleFileNameW(nullptr, module, MAX_PATH);
    // build\Release\updates_tests.exe or build\arm64\Release\...: the repository holds artifacts\.
    for (auto folder = std::filesystem::path(module).parent_path(); !folder.empty() && folder != folder.parent_path(); folder = folder.parent_path())
        if (std::filesystem::exists(folder / L"packaging" / L"AppxManifest.xml")) return folder / L"artifacts";
    return {};
}
}

int main() {
    winrt::init_apartment(winrt::apartment_type::multi_threaded);
    using reel::ParseVersion;
    Check(ParseVersion(L"v0.3.8-beta") == PackageVersion{0, 3, 8, 0}, "a release tag parses");
    Check(ParseVersion(L"0.3.8.1") == PackageVersion{0, 3, 8, 1} && ParseVersion(L"1.2") == PackageVersion{1, 2, 0, 0},
        "two to four parts parse");
    for (const wchar_t* bad : {L"", L"v", L"nightly", L"1", L"1..2", L"1.2.3.4.5", L"1.2.x", L"1.70000", L"-1.2", L"1.2."})
        Check(!ParseVersion(bad), "anything else is not a version");
    Check(PackageVersion{0, 3, 10, 0} > PackageVersion{0, 3, 9, 9} && PackageVersion{0, 3, 8, 1} > PackageVersion{0, 3, 8, 0},
        "versions compare part by part");
    Check(reel::VersionText({0, 3, 8, 0}) == L"0.3.8" && reel::VersionText({0, 3, 8, 1}) == L"0.3.8.1", "versions show without a zero revision");

    const auto x64 = reel::NewestUpdate(kReleases, {0, 3, 7, 0}, L"x64");
    Check(x64 && x64->version == PackageVersion{0, 3, 9, 0} && x64->asset == L"CopilotReel_0.3.9.0_x64.msix" &&
        x64->tag == L"v0.3.9-beta" && x64->size == 1436701 && x64->sha256.empty(),
        "the newest prerelease wins, a draft never does, and a null digest leaves the hash empty");
    const auto arm = reel::NewestUpdate(kReleases, {0, 3, 7, 0}, L"arm64");
    Check(arm && arm->asset == L"CopilotReel_0.3.9.0_arm64.msix" &&
        arm->sha256 == L"ccc06e1a2f4be5ff373834f0acf4bbcc7531dda00009e30cdc009d23e93eac6b",
        "the package for the running architecture is chosen, with its digest");
    Check(!reel::NewestUpdate(kReleases, {0, 3, 9, 0}, L"x64"), "nothing newer than the running version is offered");
    Check(!reel::NewestUpdate(kReleases, {0, 3, 7, 0}, L"x86"), "no package for the architecture, no update");
    const auto skip = reel::NewestUpdate(LR"([{"tag_name":"v0.3.8.5-beta","assets":[{"name":"CopilotReel_0.3.8.4_x64.msix","browser_download_url":"https://a/b.msix"}]},
        {"tag_name":"v0.3.8-beta","assets":[{"name":"CopilotReel_0.3.8.0_x64.msix","browser_download_url":"http://a/b.msix"},
        {"name":"CopilotReel_0.3.8_x64.msix","browser_download_url":"https://a/c.msix","digest":"sha256:00"}]}])", {0, 3, 7, 0}, L"x64");
    Check(skip && skip->version == PackageVersion{0, 3, 8, 0} && skip->url == L"https://a/c.msix" && skip->sha256.empty(),
        "an asset of another version, a plain-HTTP link and a malformed digest are ignored");
    Check(!reel::NewestUpdate(L"[]", {0, 3, 7, 0}, L"x64"), "no releases, no update");
    Check(!reel::NewestUpdate(LR"([{"tag_name":"v0.3.9","assets":[{"name":"CopilotReel_0.3.9-x\\..\\..\\evil_x64.msix","browser_download_url":"https://a/b.msix"},
        {"name":"CopilotReel_0.3.9_x64 .msix","browser_download_url":"https://a/c.msix"},{"name":"CopilotReel_0.3.9-é_x64.msix","browser_download_url":"https://a/d.msix"}]}])",
        {0, 3, 7, 0}, L"x64"), "an asset name with a path, a space or a non-ASCII letter is never offered");
    constexpr wchar_t local[] = LR"([{"tag_name":"v0.3.8","assets":[{"name":"CopilotReel_0.3.8.0_x64.msix","browser_download_url":"http://127.0.0.1:8080/a.msix"}]}])";
    Check(!reel::NewestUpdate(local, {0, 3, 7, 0}, L"x64") && reel::NewestUpdate(local, {0, 3, 7, 0}, L"x64", true),
        "plain HTTP from 127.0.0.1 serves only a test feed");
    bool threw = false;
    try { reel::NewestUpdate(L"{\"message\":\"API rate limit exceeded\"", {0, 3, 7, 0}, L"x64"); } catch (...) { threw = true; }
    Check(threw, "malformed JSON fails the check");

    const reel::PackageIdentity running{L"CopilotReel.Desktop", L"CN=CopilotReel Development", L"x64", {0, 3, 8, 0}};
    auto candidate = running;
    candidate.version = {0, 3, 9, 0};
    Check(reel::IdentityMismatch(candidate, running, {0, 3, 9, 0}).empty(), "the same package, newer and as offered, may update");
    Check(!reel::IdentityMismatch(candidate, running, {0, 3, 9, 1}).empty(), "a package other than the offered version is refused");
    for (auto field : {&reel::PackageIdentity::name, &reel::PackageIdentity::publisher, &reel::PackageIdentity::architecture}) {
        auto other = candidate;
        other.*field += L"X";
        Check(!reel::IdentityMismatch(other, running, {0, 3, 9, 0}).empty(), "another name, publisher or architecture is refused");
    }
    candidate.version = running.version;
    Check(!reel::IdentityMismatch(candidate, running, running.version).empty(), "the running version is not an update");

    // The signed package in artifacts\, if one was packaged, and a copy with one byte changed.
    std::filesystem::path signedPackage;
    if (const auto folder = Artifacts(); !folder.empty() && std::filesystem::exists(folder))
        for (const auto& entry : std::filesystem::directory_iterator(folder))
            if (entry.path().filename().wstring().starts_with(L"CopilotReel_") && entry.path().wstring().ends_with(L"_x64.msix"))
                signedPackage = entry.path();
    if (signedPackage.empty()) {
        std::cout << "Note: no signed x64 package in artifacts\\; the signature checks were skipped.\n";
    } else {
        const auto check = reel::CheckPackage(signedPackage.wstring());
        Check(check.error.empty() && check.signer == reel::kTrustedSigners[0], "the signed package verifies and names the trusted signer");
        Check(check.identity.name == L"CopilotReel.Desktop" && check.identity.publisher == L"CN=CopilotReel Development" &&
            check.identity.architecture == L"x64" && check.identity.version > PackageVersion{0, 3, 6, 0}, "its identity is read from its manifest");
        const auto hash = reel::FileSha256(signedPackage.wstring());
        Check(hash.size() == 64 && hash == reel::FileSha256(signedPackage.wstring()), "the file hash is stable lowercase hex");
        const auto tampered = std::filesystem::temp_directory_path() / L"CopilotReel-tampered-test.msix";
        std::filesystem::copy_file(signedPackage, tampered, std::filesystem::copy_options::overwrite_existing);
        {
            std::fstream file(tampered, std::ios::in | std::ios::out | std::ios::binary);
            file.seekg(static_cast<std::streamoff>(std::filesystem::file_size(tampered) / 2));
            char byte = 0;
            file.read(&byte, 1);
            file.seekp(static_cast<std::streamoff>(std::filesystem::file_size(tampered) / 2));
            byte = static_cast<char>(byte ^ 0x5A);
            file.write(&byte, 1);
        }
        Check(!reel::CheckPackage(tampered.wstring()).error.empty(), "a changed byte breaks the signature");
        Check(reel::FileSha256(tampered.wstring()) != hash, "and the file hash");
        std::filesystem::remove(tampered);
        Check(!reel::CheckPackage((std::filesystem::temp_directory_path() / L"CopilotReel-missing.msix").wstring()).error.empty(),
            "a missing file does not verify");
    }
    std::cout << "Update tests passed: " << checks << " checks" << (signedPackage.empty() ? "" : "; signature checks on ")
              << (signedPackage.empty() ? std::string() : signedPackage.filename().string()) << ".\n";
    return 0;
}
