#pragma once
#include <cstdint>
#include <cassert>
#define MSBFIRST 1
#define SPI_MODE0 0
uint8_t fakeTransfer(uint8_t);
struct SPISettings {
    uint32_t clock; int order, mode;
    SPISettings(uint32_t c, int o, int m) : clock(c), order(o), mode(m) {}
};
struct FakeSPI {
    bool failBegin=false;
    int begins=0, ends=0, transactions=0, closes=0, active=0;
    int sck=-1, miso=-2, mosi=-1, cs=-1;
    uint32_t clock=0;
    bool begin(int s,int i,int o,int c) {
        begins++; sck=s; miso=i; mosi=o; cs=c;
        return !failBegin;
    }
    void end() { assert(active==0); ends++; }
    void beginTransaction(SPISettings s) {
        transactions++; active++; clock=s.clock;
    }
    void endTransaction() { closes++; active--; }
    uint8_t transfer(uint8_t v) { return fakeTransfer(v); }
};
extern FakeSPI SPI;
