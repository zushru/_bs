#pragma once
#include <stdint.h>
#include <stddef.h>
#include "time_rules.h"
namespace att {
const size_t CFG_SLOT_SIZE = 192;     // bytes per slot; 186 used, rest 0xFF
const size_t CFG_PAYLOAD_LEN = 173;
const uint8_t CFG_SCHEMA_VER = 1;

struct Config {
  Params params;
  Timetable tt;
  int32_t termStart, termEnd;         // local day numbers (days since 2020-01-01)
  uint8_t autoSync, soundMode;        // soundMode 0 Full, 1 Quiet, 2 Silent
  uint16_t minScore;                  // 0 = sensor security level decides
  uint8_t pinSet, pinHash[32], pinSalt[8];
  char wifiPass[64];                  // NUL-terminated, zero-padded, 8..63 printable ASCII
};

enum CfgDec { CD_OK, CD_BLANK, CD_BAD_MAGIC, CD_BAD_CRC, CD_BAD_VER, CD_BAD_RANGE };
enum CfgBoot { CB_UNPROVISIONED, CB_OK, CB_FAULT };
struct CfgPick { CfgBoot boot; int active; uint32_t seq; };   // active: 0 = A, 1 = B, -1 = none
enum CfgChange { CH_OK, CH_REFUSED_WINDOW_OPEN, CH_REFUSED_UNCLOSED };

Config defaultConfig();               // wizard starting point only. NOT valid, never a fallback.
bool validateConfig(const Config& c);
bool configEqual(const Config& a, const Config& b);
size_t encodeConfig(const Config& c, uint32_t seq, uint8_t out[CFG_SLOT_SIZE]);   // 0 = invalid, out untouched
CfgDec decodeConfig(const uint8_t in[CFG_SLOT_SIZE], Config& c, uint32_t& seq);   // c, seq untouched unless CD_OK
CfgPick pickSlot(const uint8_t a[CFG_SLOT_SIZE], const uint8_t b[CFG_SLOT_SIZE], Config& out);
CfgChange canApply(const Config& cur, const Config& next, bool windowOpen, bool unclosedPast);
}
