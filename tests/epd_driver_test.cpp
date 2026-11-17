// Host tests compile the actual live driver and interface, with no real credentials.
#include <cassert>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "epd2in9_V2.h"
#include "SPI.h"
FakeSPI SPI;
uint64_t clockUs=0, resetLowAt=0, resetPulseUs=0;
uint64_t resetReleasedAt=0;
uint32_t softwareResetAt=0;
unsigned int interruptStretchUs=0;
uint32_t softwareAssertionDelay=0;
bool initPulses=false, softwareResetSent=false;
int levels[32]={0}, modes[32]={0}, resetCount=0;
enum class BusyMode { Normal, Never, FirstAttemptNever, Stuck, InitStuck };
BusyMode busyMode=BusyMode::Normal;
bool refreshSent=false;
uint32_t refreshAt=0, assertionDelay=0;
unsigned char command=0;
struct Byte { unsigned char command, value; };
std::vector<Byte> data;
std::vector<unsigned char> commands;
void pinMode(int pin,int mode) {
    if (mode==INPUT_PULLUP) levels[pin]=HIGH;
    if (pin==RST_PIN && mode==OUTPUT) assert(levels[pin]==HIGH);
    modes[pin]=mode;
}
void digitalWrite(int pin,int value) {
    if (pin==RST_PIN && value==LOW) {
        resetLowAt=clockUs; refreshSent=false; softwareResetSent=false; resetCount++;
    }
    if (pin==RST_PIN && value==HIGH && levels[pin]==LOW && resetCount) {
        resetPulseUs=clockUs-resetLowAt;
        resetReleasedAt=clockUs;
    }
    levels[pin]=value;
}
int digitalRead(int pin) {
    assert(pin==BUSY_PIN);
    if (busyMode==BusyMode::InitStuck) return HIGH;
    if (initPulses && resetCount && clockUs-resetReleasedAt<8000) return HIGH;
    if (initPulses && softwareResetSent) {
        uint32_t sinceReset=uint32_t(millis()-softwareResetAt);
        if (sinceReset>=softwareAssertionDelay && sinceReset<softwareAssertionDelay+6) return HIGH;
    }
    if (!refreshSent || busyMode==BusyMode::Never ||
        (busyMode==BusyMode::FirstAttemptNever && SPI.begins<2)) return LOW;
    uint32_t elapsed=uint32_t(millis()-refreshAt);
    if (elapsed<assertionDelay) return LOW;
    return busyMode==BusyMode::Stuck || elapsed<assertionDelay+2200 ? HIGH : LOW;
}
void delay(unsigned long ms) { clockUs+=ms*1000ULL; }
void delayMicroseconds(unsigned int us) { clockUs+=us+interruptStretchUs; }
unsigned long millis() { return uint32_t(clockUs/1000); }
unsigned long micros() { return uint32_t(clockUs); }
uint8_t fakeTransfer(uint8_t value) {
    assert(SPI.active==1);
    assert(levels[CS_PIN]==LOW);
    if (initPulses && softwareResetSent && !refreshSent) assert(digitalRead(BUSY_PIN)==LOW);
    if (levels[DC_PIN]==LOW) {
        command=value; commands.push_back(value);
        if (value==0x20) { refreshSent=true; refreshAt=millis(); }
        if (value==0x12) { softwareResetSent=true; softwareResetAt=millis(); }
    } else data.push_back({command,value});
    return 0;
}
void assertInit(Epd& epd) {
    assert(epd.Init()==0);
    assert(!epd.HasError());
    assert(resetPulseUs==2000+interruptStretchUs);
    assert(epd.GetInitDiagnostics().resetLowUs==resetPulseUs);
    assert(SPI.sck==4 && SPI.miso==-1 && SPI.mosi==6 && SPI.cs==7);
    assert(SPI.clock==2000000);
    assert(modes[BUSY_PIN]==INPUT);
    size_t lutBytes=0;
    for (auto b:data) if (b.command==0x32) lutBytes++;
    assert(lutBytes>=153);
}
Epd epd;
#ifdef TEST_DASHBOARD_POLICY
struct FakeSerial {
    template<typename... Args> void printf(const char*,Args...) {}
    void println(const char*) {}
    void flush() {}
} Serial;
unsigned char image[EPD_WIDTH*EPD_HEIGHT/8]={0};
// Extracted from the real .ino by the runner; no duplicate retry implementation.
#include "dashboard_policy.inc"
#endif
int main(int argc,char** argv) {
    assert(argc==2); std::string scenario=argv[1];
#ifdef TEST_DASHBOARD_POLICY
    if (scenario.find("policy_")==0) {
        if (scenario=="policy_never") busyMode=BusyMode::Never;
        if (scenario=="policy_recovers") busyMode=BusyMode::FirstAttemptNever;
        if (scenario=="policy_late") assertionDelay=251;
        if (scenario=="policy_stuck") busyMode=BusyMode::Stuck;
        if (scenario=="policy_spi_failure") SPI.failBegin=true;
        tryDashboardFrame();
        bool twice=scenario=="policy_never" || scenario=="policy_recovers";
        bool spiFail=scenario=="policy_spi_failure";
        assert(resetCount==(spiFail ? 0 : twice ? 2 : 1));
        assert(SPI.begins==(twice ? 2 : 1));
        assert(SPI.active==0 && SPI.transactions==SPI.closes);
        assert(SPI.transactions==(spiFail ? 0 : twice ? 2 : 1));
        assert(SPI.ends==((twice || spiFail) ? 1 : 0));
        assert(std::count(commands.begin(),commands.end(),0x20)==(spiFail ? 0 : twice ? 2 : 1));
        bool ok=scenario=="policy_success" || scenario=="policy_recovers";
        assert(std::count(commands.begin(),commands.end(),0x10)==(ok ? 1 : 0));
        assert(epd.HasError()!=ok);
        EpdIf::IfEnd(); assert(SPI.active==0);
        return 0;
    }
#endif
    if (scenario=="spi_failure") {
        SPI.failBegin=true;
        assert(epd.Init()==-1);
        assert(std::strcmp(epd.GetLastError(),"SPI_INIT_FAILED")==0);
        assert(commands.empty() && SPI.active==0);
        assert(!epd.GetInitDiagnostics().reset.sampled);
        EpdIf::IfEnd(); assert(SPI.closes==0);
        SPI.failBegin=false; assertInit(epd);
        EpdIf::IfEnd(); assert(SPI.active==0 && SPI.begins==2);
    } else if (scenario=="init_busy_timeout") {
        busyMode=BusyMode::InitStuck;
        assert(epd.Init()==-1);
        assert(std::strcmp(epd.GetLastError(),"BUSY_TIMEOUT")==0);
        assert(millis()<11000);
        assert(commands.empty());
        EpdIf::IfEnd(); assert(SPI.active==0);
    } else {
        if (scenario=="rollover") clockUs=(uint64_t(UINT32_MAX)-200)*1000;
        if (scenario=="init_trace" || scenario=="init_delayed_trace") initPulses=true;
        if (scenario=="init_delayed_trace") softwareAssertionDelay=2;
        if (scenario=="reset_stretched") interruptStretchUs=5000;
        if (scenario=="micros_rollover") clockUs=uint64_t(UINT32_MAX)-21000;
        assertInit(epd);
        if (scenario=="init_trace" || scenario=="init_delayed_trace" || scenario=="reset_stretched" || scenario=="micros_rollover") {
            const auto& diag=epd.GetInitDiagnostics();
            assert(diag.reset.sampled && diag.softwareReset.sampled);
            assert(diag.reset.highSeen==initPulses && diag.softwareReset.highSeen==initPulses);
            assert(diag.reset.last==LOW && diag.softwareReset.last==LOW);
            if (initPulses) {
                assert(diag.reset.first==HIGH);
                assert(diag.softwareReset.first==(softwareAssertionDelay ? LOW : HIGH));
            }
            assert(!epd.CanRetryRefresh());
            EpdIf::IfEnd();
        } else if (scenario=="spi_restart_failure") {
            SPI.failBegin=true;
            assert(EpdIf::RestartSpi()==-1);
            assert(SPI.active==0 && SPI.begins==2 && SPI.ends==2);
            assert(resetCount==1); // failed host restart does not reset the panel
            SPI.failBegin=false; assertInit(epd);
            EpdIf::IfEnd(); assert(SPI.active==0 && SPI.begins==3);
        } else if (scenario=="begin_once") {
            EpdIf::IfEnd(); assertInit(epd);
            assert(SPI.begins==1 && SPI.transactions==2);
            EpdIf::IfEnd(); assert(SPI.active==0);
        } else if (scenario=="ram_planes") {
            data.clear(); epd.ClearFrameMemory(0xFF);
            size_t current=0, previous=0;
            for (auto b:data) {
                if (b.command==0x24) { current++; assert(b.value==0xFF); }
                if (b.command==0x26) { previous++; assert(b.value==0xFF); }
            }
            assert(current==4736 && previous==4736);
            EpdIf::IfEnd();
        } else {
            bool noAssertion=scenario=="never_asserts" || scenario=="recovery" || scenario=="spi_restart" || scenario=="guard_rollover";
            bool stuck=scenario=="stuck_high" || scenario=="restart_busy_guard";
            if (noAssertion) busyMode=BusyMode::Never;
            if (stuck) busyMode=BusyMode::Stuck;
            if (scenario=="delayed_assertion") assertionDelay=100;
            if (scenario=="late_assertion") assertionDelay=251;
            auto start=uint32_t(millis()); bool ok=epd.DisplayFrame();
            auto elapsed=uint32_t(millis()-start);
            assert(commands.size()>=2 && commands[commands.size()-2]==0x22 && commands.back()==0x20);
            bool updateByte=false;
            for (auto b:data) if (b.command==0x22 && b.value==0xC7) updateByte=true;
            assert(updateByte);
            if (noAssertion || scenario=="late_assertion") {
                assert(!ok && std::strcmp(epd.GetLastError(),"REFRESH_NOT_STARTED")==0);
                assert(elapsed>=250 && elapsed<300);
            } else if (stuck) {
                assert(!ok && std::strcmp(epd.GetLastError(),"BUSY_TIMEOUT")==0);
                assert(elapsed>=15000 && elapsed<15100);
            } else {
                assert(ok && !epd.HasError());
                assert(elapsed>=2200 && elapsed<2400);
            }
            size_t count=commands.size(); epd.Sleep();
            if (ok) assert(commands.back()==0x10 && data.back().value==0x01);
            else assert(commands.size()==count); // no sleep/commands after a fault
            if (scenario=="late_assertion" || scenario=="restart_busy_guard") {
                assert(!epd.CanRetryRefresh());
                assert(EpdIf::RestartSpi()==-2);
                assert(SPI.ends==0 && resetCount==1 && commands.size()==count);
            }
            if (scenario=="spi_restart" || scenario=="guard_rollover") {
                if (scenario=="guard_rollover") clockUs=(uint64_t(UINT32_MAX)-500)*1000;
                uint32_t guardStart=millis();
                assert(epd.CanRetryRefresh());
                assert(uint32_t(millis()-guardStart)==1000);
                assert(commands.size()==count && resetCount==1);
                assert(EpdIf::RestartSpi()==0);
                assert(SPI.active==0 && SPI.begins==2 && SPI.ends==1 && SPI.closes==1);
                busyMode=BusyMode::Normal; assertInit(epd);
                assert(epd.DisplayFrame()); epd.Sleep();
                assert(resetCount==2 && SPI.transactions==2);
            }
            EpdIf::IfEnd(); assert(SPI.active==0);
            if (scenario=="recovery") {
                busyMode=BusyMode::Normal; assertInit(epd);
                assert(epd.DisplayFrame()); epd.Sleep(); EpdIf::IfEnd();
                assert(SPI.begins==1 && SPI.active==0);
            }
        }
    }
    EpdIf::IfEnd(); assert(SPI.active==0); // cleanup is idempotent
}
