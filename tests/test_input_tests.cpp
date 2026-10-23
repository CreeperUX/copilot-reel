#include "test_input.h"
#include <appmodel.h>
#include <array>
#include <cstdlib>
#include <iostream>

void Check(bool condition, const char* label) {
    if (!condition) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}
int main() {
    using namespace reel;
    constexpr std::wstring_view id=L"run-0123456789abcdef",dir=L"C:\\QA\\seam";
    const auto refused=[](const TestInputGate& gate) {
        return gate.present && !gate.active && !gate.reason.empty() && gate.reason!=L"packaged" && gate.runId.empty();
    };

    // Missing variable: off, and nothing for wWinMain to refuse.
    for(const auto pointer:{L"",L"10,20",L"x,1"}) {
        const auto gate=ParseTestInputGate(L"",pointer,dir,false);
        Check(!gate.present && !gate.active && gate.reason.empty(),"a missing run id leaves the seam off without a reason");
    }
    Check(!ParseTestInputGate(L"",L"",L"",true).present,"a packaged process without the variable is untouched");

    // Packaged: off with the one reason wWinMain ignores, whatever else the environment holds.
    for(const auto runId:{id,std::wstring_view(L"bad id!")}) {
        const auto gate=ParseTestInputGate(runId,L"x,1",L"relative",true);
        Check(gate.present && !gate.active && gate.reason==L"packaged" && gate.runId.empty() && gate.pointer.x==0 && gate.pointer.y==0,
            "the installed app ignores the seam and never exits for it");
    }
    const auto packaged=ParseTestInputGate(id,L"10,20",dir,true);
    Check(packaged.present && !packaged.active && packaged.reason==L"packaged","a packaged process with every other condition valid stays off");

    // Run id: [A-Za-z0-9-]{8,40}.
    for(const auto runId:{L"short",L"1234567",L"bad id!",L"run_0123456789",L"run.0123456789",L"run-0123\t456789",
                          L"\x8fd0\x884c-0123456789",L"run-0123456789-0123456789-0123456789-0123"})
        Check(refused(ParseTestInputGate(runId,L"",dir,false)),"a malformed run id is refused with a reason");
    for(const auto runId:{L"12345678",L"ABCDEFGH-ijklmnop-0123",L"run-0123456789-0123456789-0123456789-012",
                          L"0f8fad5b-d9cb-469f-a165-70867728950e"}) {
        const auto gate=ParseTestInputGate(runId,L"",dir,false);
        Check(gate.present && gate.active && gate.reason.empty() && gate.runId==runId,"a run id of 8 to 40 safe characters is accepted");
    }

    // Data directory: an absolute drive or UNC path, never a relative, rooted or device path.
    for(const auto data:{L"",L"relative\\dir",L"C:",L"C:relative",L"\\QA\\seam",L"/QA/seam",L"\\\\?\\C:\\QA",
                         L"\\\\.\\pipe\\reel",L"\\\\server",L"\\\\server\\",L"\\\\server\\\\share"})
        Check(refused(ParseTestInputGate(id,L"",data,false)),"an empty, relative or device data directory is refused");
    for(const auto data:{L"C:\\QA",L"d:/qa/seam",L"\\\\server\\share",L"\\\\server\\share\\qa"})
        Check(ParseTestInputGate(id,L"",data,false).active,"an absolute drive or UNC data directory is accepted");

    // Pointer: x,y in physical pixels; negative monitor origins are valid.
    for(const auto pointer:{L"x,1",L"1",L"1,",L",1",L"1,2,3",L" 1,2",L"1, 2",L"1,2 ",L"+1,2",L"--1,2",L"-,2",L"1.5,2",
                            L"99999999999,1",L"2147483648,0",L"0,-2147483648",L"12345678901,0"})
        Check(refused(ParseTestInputGate(id,pointer,dir,false)),"a malformed or overflowing pointer is refused");
    struct Expected { const wchar_t* text; LONG x, y; };
    for(const auto& expected:{Expected{L"0,0",0,0},Expected{L"-1920,-200",-1920,-200},Expected{L"3839,2159",3839,2159},
                              Expected{L"2147483647,-2147483647",2147483647,-2147483647},Expected{L"0007,-0",7,0}}) {
        const auto gate=ParseTestInputGate(id,expected.text,dir,false);
        Check(gate.active && gate.pointer.x==expected.x && gate.pointer.y==expected.y,"a valid pointer becomes the initial synthetic cursor");
    }
    const auto valid=ParseTestInputGate(id,L"",dir,false);
    Check(valid.present && valid.active && valid.reason.empty() && valid.runId==id && valid.pointer.x==0 && valid.pointer.y==0,
        "valid input enables the seam; an empty pointer leaves the default to CurrentTestInput");

    // Decoder: only an active seam accepts a complete, current, in-range packet.
    TestPacket packet;packet.action=TestAction::Key;packet.sequence=7;packet.screen={-5,9};packet.vk=VK_ESCAPE;
    Check(!DecodeTestPacket(kTestIpcMagic,sizeof(packet),&packet,false),"an inactive seam rejects every packet");
    Check(!DecodeTestPacket(0x5245454c,sizeof(packet),&packet,true),"the command magic is not test input");
    Check(!DecodeTestPacket(0,sizeof(packet),&packet,true),"an unknown tag is rejected");
    std::array<unsigned char,40> bytes{};std::memcpy(bytes.data(),&packet,sizeof(packet));
    for(const DWORD size:{0u,24u,31u,33u,40u})
        Check(!DecodeTestPacket(kTestIpcMagic,size,bytes.data(),true),"a truncated or oversized packet is rejected");
    Check(!DecodeTestPacket(kTestIpcMagic,sizeof(packet),nullptr,true),"a packet without data is rejected");
    auto changed=packet;changed.version=2;
    Check(!DecodeTestPacket(kTestIpcMagic,sizeof(changed),&changed,true),"a future packet version is rejected");
    changed=packet;changed.version=0;
    Check(!DecodeTestPacket(kTestIpcMagic,sizeof(changed),&changed,true),"version zero is rejected");
    for(const unsigned action:{static_cast<unsigned>(TestAction::Count),99u,0xffffffffu}) {
        changed=packet;std::memcpy(&changed.action,&action,sizeof(action));
        Check(!DecodeTestPacket(kTestIpcMagic,sizeof(changed),&changed,true),"an unknown action is rejected");
    }
    for(const unsigned vk:{0xFFu,0x100u,0xffffffffu}) {
        changed=packet;changed.vk=vk;
        Check(!DecodeTestPacket(kTestIpcMagic,sizeof(changed),&changed,true),"a virtual-key code above 0xFE is rejected");
    }
    changed=packet;changed.reserved=1;
    Check(!DecodeTestPacket(kTestIpcMagic,sizeof(changed),&changed,true),"a nonzero reserved field is rejected");
    for(unsigned action=0;action<static_cast<unsigned>(TestAction::Count);++action) {
        for(const unsigned vk:{0u,0x41u,0xFEu}) {
            changed=packet;changed.action=static_cast<TestAction>(action);changed.vk=vk;changed.sequence=1000+action;
            const auto decoded=DecodeTestPacket(kTestIpcMagic,sizeof(changed),&changed,true);
            Check(decoded && decoded->action==changed.action && decoded->sequence==changed.sequence && decoded->vk==vk &&
                decoded->screen.x==-5 && decoded->screen.y==9,"an active seam accepts every action unchanged");
        }
    }
    // The drag halves extend the wire format; the earlier actions keep their numbers.
    Check(static_cast<unsigned>(TestAction::Key)==3 && static_cast<unsigned>(TestAction::Press)==4 &&
        static_cast<unsigned>(TestAction::Release)==5 && static_cast<unsigned>(TestAction::Count)==6,
        "tests/reel_seam.py mirrors the action numbers");

    // This process: the gate is read once from the environment and the package identity.
    UINT32 length=0;
    const bool packagedProcess=GetCurrentPackageFullName(&length,nullptr)!=APPMODEL_ERROR_NO_PACKAGE;
    Check(SetEnvironmentVariableW(L"COPILOT_REEL_TEST_INPUT",L"unit-test-0001") &&
        SetEnvironmentVariableW(L"COPILOT_REEL_DATA_DIR",L"C:\\CopilotReel-unit-test") &&
        SetEnvironmentVariableW(L"COPILOT_REEL_TEST_POINTER",L"-1234,567"),"test environment");
    const auto& gate=CurrentTestInput();
    if(packagedProcess) {
        Check(gate.present && !gate.active && gate.reason==L"packaged" && TestInstanceSuffix().empty(),
            "a packaged process keeps the production namespace");
    } else {
        Check(gate.active && gate.runId==L"unit-test-0001","an unpackaged process with valid variables enables the seam");
        Check(TestInstanceSuffix()==L".Test.unit-test-0001","a test instance owns its own mutex and ready namespace");
        Check(CursorPosition().x==-1234 && CursorPosition().y==567,"the cursor starts at COPILOT_REEL_TEST_POINTER");
        SetSyntheticCursor({40,-30});
        Check(CursorPosition().x==40 && CursorPosition().y==-30,"test input moves only the synthetic cursor");
    }
    SetEnvironmentVariableW(L"COPILOT_REEL_TEST_INPUT",nullptr);
    Check(&CurrentTestInput()==&gate && CurrentTestInput().active==!packagedProcess,"the gate is parsed once per process");
    std::cout << "Test input seam tests passed: gate truth table (missing, packaged, run id, data directory, pointer), "
                 "packet decoding (tag, size, version, action, key, reserved), and the cached "
              << (packagedProcess?"packaged":"unpackaged") << " process gate with its synthetic cursor and namespace.\n";
}
