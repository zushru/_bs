// ESP32 ATT diagnostic tool; AP: ATT-DIAG / diag12345.
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <esp_system.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define PIN_BUZ 23
#define PIN_SDA 21
#define PIN_SCL 22
#define PIN_RX2 16
#define PIN_TX2 17
#define PIN_TCH 4
WebServer srv(80);
Preferences prefs;
String L;
int job = 0;
int activeJob = 0;
bool busy = false;
bool apOff = false;
uint32_t lastSave = 0;

static void lg(const char* f, ...) {
  char b[200];
  va_list a;
  va_start(a, f);
  vsnprintf(b, sizeof b, f, a);
  va_end(a);
  uint32_t s = millis() / 1000;
  char t[16];
  snprintf(t, sizeof t, "[%lu:%02lu] ", (unsigned long)(s / 60), (unsigned long)(s % 60));
  L += t;
  L += b;
  L += '\n';
  if (L.length() > 7000) L.remove(0, 3000);
  File lf = LittleFS.open("/d.txt", "a");
  if (lf && lf.size() > 60000) { lf.close(); LittleFS.remove("/d.txt"); lf = LittleFS.open("/d.txt", "a"); }
  if (lf) { lf.print(t); lf.print(b); lf.print('\n'); lf.close(); }
}

static void nap(uint32_t ms) {
  uint32_t t = millis();
  while (millis() - t < ms) { if (!apOff) srv.handleClient(); delay(2); }
}

static void beep(int ms) {
  digitalWrite(PIN_BUZ, LOW);
  nap(ms);
  digitalWrite(PIN_BUZ, HIGH);
}

static void jobScan() {
  Wire.begin(PIN_SDA, PIN_SCL);
  int n = 0;
  String s;
  for (uint8_t a = 8; a < 120; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      char h[8];
      snprintf(h, sizeof h, "0x%02X ", a);
      s += h;
      n++;
    }
  }
  lg("I2C scan: %d found: %s", n, s.c_str());
}

static uint8_t bcd(uint8_t v) { return (v >> 4) * 10 + (v & 15); }

static void jobRtc() {
  Wire.begin(PIN_SDA, PIN_SCL);
  Wire.beginTransmission(0x68);
  Wire.write(0);
  if (Wire.endTransmission(false) != 0) { lg("RTC: no ack at 0x68"); return; }
  if (Wire.requestFrom(0x68, 7) != 7) { lg("RTC: short read"); return; }
  uint8_t r[7];
  for (int i = 0; i < 7; i++) r[i] = Wire.read();
  Wire.beginTransmission(0x68);
  Wire.write(0x0F);
  Wire.endTransmission(false);
  if (Wire.requestFrom(0x68, 1) != 1) { lg("RTC: status-register read failed"); return; }
  int stRead = Wire.read();
  if (stRead < 0) { lg("RTC: status-register read failed"); return; }
  uint8_t st = (uint8_t)stRead;
  char b[40];
  snprintf(b, sizeof b, "20%02d-%02d-%02d %02d:%02d:%02d", bcd(r[6]), bcd(r[5] & 0x1F),
           bcd(r[4]), bcd(r[2] & 0x3F), bcd(r[1]), bcd(r[0] & 0x7F));
  lg("RTC %s  OSF(lostPower)=%d", b, (st >> 7) & 1);
  prefs.putString("rtc", b);
}

static void jobTouch() {
  pinMode(PIN_TCH, INPUT);
  lg("GPIO4: sampling 8 s. Touch/release the sensor now.");
  uint32_t t0 = millis();
  int hi = 0, lo = 0, tr = 0, last = digitalRead(PIN_TCH);
  while (millis() - t0 < 8000) {
    int v = digitalRead(PIN_TCH);
    if (v) hi++; else lo++;
    if (v != last) { tr++; last = v; }
    nap(10);
  }
  lg("GPIO4: high=%d low=%d transitions=%d", hi, lo, tr);
}

static void afSend(uint8_t ins, const uint8_t* d, uint8_t n) {
  uint8_t len = n + 3;
  uint16_t sum = 1 + len + ins;
  while (Serial2.available()) Serial2.read();
  uint8_t h[9] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x00, len};
  Serial2.write(h, 9);
  Serial2.write(ins);
  for (uint8_t i = 0; i < n; i++) { Serial2.write(d[i]); sum += d[i]; }
  Serial2.write((uint8_t)(sum >> 8));
  Serial2.write((uint8_t)sum);
}

static int afRecv(uint8_t* buf, int cap, uint32_t to) {
  uint32_t t0 = millis();
  int n = 0, need = 9;
  while (millis() - t0 < to) {
    if (!Serial2.available()) { delay(1); continue; }
    uint8_t c = Serial2.read();
    if (n == 0 && c != 0xEF) continue;
    if (n < cap) buf[n] = c;
    n++;
    if (n == 9) {
      need = 9 + ((buf[7] << 8) | buf[8]);
      if (need < 12 || need > cap) return -2;
    }
    if (n >= 9 && n == need) break;
  }
  if (n < 9 || n != need) return -1;
  if (buf[1] != 0x01) return -2;
  uint16_t sum = 0;
  for (int i = 6; i < need - 2; i++) sum += buf[i];
  if (sum != (uint16_t)((buf[need - 2] << 8) | buf[need - 1])) return -3;
  return need;
}

static int afCmd(uint8_t ins, const uint8_t* d, uint8_t n, uint8_t* r, int cap, uint32_t to) {
  afSend(ins, d, n);
  return afRecv(r, cap, to);
}

static int cc(int n, const uint8_t* r) {
  if (n < 0) return n;
  if (n < 12) return -2;
  if (r[6] != 0x07) return -4;
  return r[9];
}

static const char* afCodeName(int c) {
  switch (c) {
    case 0x00: return "OK";
    case 0x01: return "packet receive error";
    case 0x02: return "no finger";
    case 0x03: return "image capture failed";
    case 0x06: return "image too messy";
    case 0x07: return "too few feature points";
    case 0x09: return "finger not found";
    case 0x0A: return "fingerprints differ";
    case 0x0B: return "invalid template slot";
    case 0x13: return "password verification failed";
    case 0x15: return "invalid image";
    case 0x18: return "flash write error";
    case -1: return "UART timeout";
    case -2: return "invalid packet";
    case -3: return "packet checksum error";
    case -4: return "unexpected packet type";
    default: return "sensor error";
  }
}

static bool afWaitFinger(bool present, uint32_t timeoutMs) {
  uint32_t t0 = millis();
  int consecutive = 0;
  int lastError = -99;
  uint8_t r[16];
  while (millis() - t0 < timeoutMs) {
    int n = afCmd(0x01, nullptr, 0, r, sizeof r, 400);
    int c = cc(n, r);
    bool target = present ? (c == 0x00) : (c == 0x02);
    if (target) {
      if (++consecutive >= 2) {
        lg(present ? "finger detected (stable)" : "finger removal detected");
        return true;
      }
    } else {
      consecutive = 0;
      if (present ? (c != 0x02) : (c != 0x00)) lastError = c;
    }
    nap(50);
  }
  if (lastError == -99)
    lg("finger wait timeout: %s", present ? "no stable finger detected" : "finger was not lifted");
  else
    lg("finger wait timeout: %s; last sensor result=%d (%s)",
       present ? "no stable finger detected" : "finger was not lifted",
       lastError, afCodeName(lastError));
  return false;
}
#ifndef FP_PASSWORD
#define FP_PASSWORD 0x00000000UL
#endif
static bool afAuthenticate() {
  uint8_t password[4] = {
    (uint8_t)(FP_PASSWORD >> 24), (uint8_t)(FP_PASSWORD >> 16),
    (uint8_t)(FP_PASSWORD >> 8), (uint8_t)FP_PASSWORD
  };
  uint8_t r[40];
  int n = afCmd(0x13, password, sizeof password, r, sizeof r, 1200);
  int c = cc(n, r);
  if (c != 0) {
    lg("AS608 password check failed: code=%d (%s). Check sensor password, UART wiring and baud rate.",
       c, afCodeName(c));
    return false;
  }
  n = afCmd(0x0F, nullptr, 0, r, sizeof r, 1200);
  c = cc(n, r);
  if (c != 0 || n < 28) {
    lg("AS608 system-parameter read failed: code=%d (%s)", c, afCodeName(c));
    return false;
  }
  uint16_t capacity = (uint16_t)((r[14] << 8) | r[15]);
  uint16_t packetSizeCode = (uint16_t)((r[22] << 8) | r[23]);
  uint16_t baudCode = (uint16_t)((r[24] << 8) | r[25]);
  lg("AS608 ready: capacity=%u packet-size-code=%u baud=%lu",
     capacity, packetSizeCode, (unsigned long)baudCode * 9600UL);
  if (capacity <= 1) {
    lg("AS608 enrollment stopped: sensor reports no usable slot 1");
    return false;
  }
  return true;
}

static bool afWait(uint8_t want, uint32_t to) {
  uint32_t t0 = millis();
  uint8_t r[16];
  while (millis() - t0 < to) {
    int n = afCmd(0x01, nullptr, 0, r, sizeof r, 300);
    if (n > 0 && r[9] == want) return true;
    nap(60);
  }
  return false;
}

static void afSendData(uint8_t pid, const uint8_t* d, int n) {
  uint16_t len = n + 2, sum = pid + (len >> 8) + (len & 0xFF);
  uint8_t h[9] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF, pid, (uint8_t)(len >> 8), (uint8_t)len};
  Serial2.write(h, 9);
  for (int i = 0; i < n; i++) { Serial2.write(d[i]); sum += d[i]; }
  Serial2.write((uint8_t)(sum >> 8));
  Serial2.write((uint8_t)sum);
}

static void jobParams() {
  uint8_t r[40];
  int n = afCmd(0x0F, nullptr, 0, r, sizeof r, 1000);
  if (n < 0) { lg("AS608 params: transport err %d", n); return; }
  String h;
  for (int i = 0; i < n; i++) { char x[4]; snprintf(x, sizeof x, "%02X", r[i]); h += x; }
  lg("AS608 raw(%d): %s", n, h.c_str());
  if (n >= 28 && r[9] == 0)
    lg("AS608 cap=%u security=%u pktcode=%u baud=%u", (r[14] << 8) | r[15],
       (r[16] << 8) | r[17], (r[22] << 8) | r[23], ((r[24] << 8) | r[25]) * 9600);
}

static int afSearch(int& page, int& score) {
  uint8_t r[20], d[1] = {1};
  int n = afCmd(0x02, d, 1, r, sizeof r, 1000);
  int c = cc(n, r);
  if (c) return c;
  uint8_t s[5] = {1, 0, 0, 0x01, 0x2C};
  n = afCmd(0x04, s, 5, r, sizeof r, 2000);
  c = cc(n, r);
  if (c == 0 && n >= 16) { page = (r[10] << 8) | r[11]; score = (r[12] << 8) | r[13]; }
  return c;
}

static void jobEnroll() {
  uint8_t r[40], d[3];
  lg("ENROLL slot 1: checking sensor");
  if (!afAuthenticate()) return;
  lg("Place finger flat on the sensor and hold still (10 s)");
  if (!afWaitFinger(true, 10000)) {
    lg("enroll stopped: first finger image was not acquired");
    return;
  }
  d[0] = 1;
  int c = cc(afCmd(0x02, d, 1, r, sizeof r, 1200), r);
  if (c != 0) {
    lg("enroll: Img2Tz #1 failed: code=%d (%s)", c, afCodeName(c));
    return;
  }
  beep(40);
  lg("Lift finger completely; wait for the sensor to detect removal (10 s)");
  if (!afWaitFinger(false, 10000)) {
    lg("enroll stopped: remove finger before the second scan");
    return;
  }
  lg("Place the SAME finger again, in a similar position (10 s)");
  if (!afWaitFinger(true, 10000)) {
    lg("enroll stopped: second finger image was not acquired");
    return;
  }
  d[0] = 2;
  c = cc(afCmd(0x02, d, 1, r, sizeof r, 1200), r);
  if (c != 0) {
    lg("enroll: Img2Tz #2 failed: code=%d (%s)", c, afCodeName(c));
    return;
  }
  c = cc(afCmd(0x05, nullptr, 0, r, sizeof r, 1500), r);
  if (c != 0) {
    lg("enroll: RegModel failed: code=%d (%s)", c, afCodeName(c));
    return;
  }
  d[0] = 1; d[1] = 0; d[2] = 1;
  c = cc(afCmd(0x06, d, 3, r, sizeof r, 2000), r);
  if (c != 0) {
    lg("enroll: Store slot 1 failed: code=%d (%s)", c, afCodeName(c));
    return;
  }
  lg("ENROLL SUCCESS: fingerprint stored in slot 1");
  beep(200);
}

static uint8_t tpl[1536], tpl2[1536];

// UpChar: command ack, then data packets until the end packet. 0 = ok, <0 = transport error, >0 = module code.
static int afUpChar(uint8_t bufId, uint8_t* dst, int cap, int& len) {
  uint8_t r[160], d[1];
  d[0] = bufId;
  int c = cc(afCmd(0x08, d, 1, r, sizeof r, 1000), r);
  if (c) return c;
  len = 0;
  for (int pk = 0; pk < 12; pk++) {
    int n = afRecv(r, sizeof r, 1500);
    if (n < 0) return n;
    int dl = n - 11;
    if (dl < 0 || len + dl > cap) return -5;
    memcpy(dst + len, r + 9, dl);
    len += dl;
    if (r[6] == 0x08) return 0;
  }
  return -5;
}
// DownChar into buffer 1. The module's reply after the data is NOT trusted; verification is done by reading back.
static bool afDownChar(const uint8_t* src, int len, int pkt) {
  uint8_t r[160], d[1];
  d[0] = 1;
  if (cc(afCmd(0x09, d, 1, r, sizeof r, 1000), r) != 0) return false;
  for (int o = 0; o < len; o += pkt) {
    int k = len - o < pkt ? len - o : pkt;
    afSendData(o + k >= len ? 0x08 : 0x02, src + o, k);
    nap(40);
  }
  nap(300);
  while (Serial2.available()) Serial2.read();
  return true;
}

// Test 8: slot 1 -> RAM -> overwrite buffer -> upload back -> store slot 2 -> reload -> compare bytes.
static void jobTemplate() {
  uint8_t r[160], d[4];
  d[0] = 1; d[1] = 0; d[2] = 1;
  int c = cc(afCmd(0x07, d, 3, r, sizeof r, 1000), r);   // LoadChar slot 1 -> buffer 1
  if (c) { lg("tpl: loadchar code=%d (slot 1 empty? run Enroll)", c); return; }
  int len = 0;
  c = afUpChar(1, tpl, sizeof tpl, len);
  if (c) { lg("tpl: upchar err=%d", c); return; }
  uint32_t h = 2166136261u;
  for (int i = 0; i < len; i++) { h ^= tpl[i]; h *= 16777619u; }
  lg("tpl: downloaded %d bytes, fnv=%08lX", len, (unsigned long)h);
  lg("tpl: place ANY finger once to overwrite buffer 1 (10 s)");
  if (!afWait(0x00, 10000)) { lg("tpl: no finger"); return; }
  d[0] = 1;
  c = cc(afCmd(0x02, d, 1, r, sizeof r, 1000), r);       // Img2Tz -> buffer 1: now differs from the template
  if (c) { lg("tpl: overwrite code=%d", c); return; }
  beep(40);
  lg("tpl: lift finger");
  afWait(0x02, 8000);
  const int sizes[3] = {128, 64, 32};
  for (int s = 0; s < 3; s++) {
    if (!afDownChar(tpl, len, sizes[s])) { lg("tpl: downchar refused (pkt %d)", sizes[s]); continue; }
    d[0] = 1; d[1] = 0; d[2] = 2;
    c = cc(afCmd(0x06, d, 3, r, sizeof r, 1500), r);     // Store buffer 1 -> page 2
    if (c) { lg("tpl: store code=%d (pkt %d)", c, sizes[s]); continue; }
    d[0] = 2; d[1] = 0; d[2] = 2;
    c = cc(afCmd(0x07, d, 3, r, sizeof r, 1000), r);     // LoadChar page 2 -> buffer 2
    if (c) { lg("tpl: reload code=%d (pkt %d)", c, sizes[s]); continue; }
    int len2 = 0;
    c = afUpChar(2, tpl2, sizeof tpl2, len2);
    if (c) { lg("tpl: reupload err=%d (pkt %d)", c, sizes[s]); continue; }
    bool same = len2 == len && memcmp(tpl, tpl2, len) == 0;
    lg("tpl: pkt %d -> slot 2 is %d bytes: %s", sizes[s], len2, same ? "RESTORE OK (byte-exact)" : "differs");
    if (same) { beep(200); return; }
  }
  lg("tpl: RESTORE FAILED for every packet size");
}

static void apStart() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("ATT-DIAG", "diag12345", 1, 0, 2);
}

static void jobBurst(bool quiet) {
  int ok = 0, nf = 0, bad = 0;
  if (quiet) {
    lg("burst AP-OFF: Wi-Fi stops now; 3 long beeps = start, 3 = done, then reconnect");
    nap(300);
    apOff = true;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    for (int k = 0; k < 3; k++) { beep(300); nap(200); }
    nap(2000);
  }
  for (int i = 1; i <= 10; i++) {
    if (quiet) { beep(60); nap(80); beep(60); }
    lg("scan %d: place finger (8 s)", i);
    if (!afWait(0x00, 8000)) { lg("scan %d: timeout", i); bad++; continue; }
    beep(30);
    uint32_t t0 = millis();
    int page = -1, score = -1;
    int c = afSearch(page, score);
    lg("scan %d: code=%d page=%d score=%d ms=%lu", i, c, page, score, (unsigned long)(millis() - t0));
    if (c == 0) ok++; else if (c == 9) nf++; else bad++;
    afWait(0x02, 5000);
  }
  if (quiet) {
    apStart();
    srv.close();
    srv.begin();
    apOff = false;
    for (int k = 0; k < 3; k++) { beep(300); nap(200); }
  }
  lg("burst%s: match=%d nomatch=%d err/timeout=%d", quiet ? " AP-OFF" : "", ok, nf, bad);
}

static const char* jobName(int j) {
  switch (j) {
    case 1: return "I2C scan";
    case 2: return "RTC read";
    case 3: return "GPIO4 sample";
    case 4: return "AS608 params";
    case 5: return "Enroll slot 1";
    case 6: return "10 scans";
    case 7: return "Beep 50";
    case 8: return "Beep 100";
    case 9: return "Template round-trip";
    case 10: return "10 scans AP OFF";
    default: return "Unknown job";
  }
}

static void runJob(int j) {
  switch (j) {
    case 1: jobScan(); break;
    case 2: jobRtc(); break;
    case 3: jobTouch(); break;
    case 4: jobParams(); break;
    case 5: jobEnroll(); break;
    case 6: jobBurst(false); break;
    case 10: jobBurst(true); break;
    case 7: lg("beep 50 ms"); beep(50); break;
    case 8: lg("beep 100 ms"); beep(100); break;
    case 9: jobTemplate(); break;
  }
  lg("done job %d", j);
}

static const char PAGE[] PROGMEM = R"HTML(<!doctype html>
<meta name=viewport content="width=device-width,initial-scale=1">
<style>body{font:14px sans-serif;margin:8px}#s{padding:6px;margin-bottom:6px;background:#eee}#b{display:grid;grid-template-columns:repeat(auto-fit,minmax(105px,1fr));gap:4px}button{padding:7px;font-size:13px}button:disabled{opacity:.45}.actions{display:flex;gap:4px;margin:6px 0}.actions input{min-width:0;flex:1}textarea{box-sizing:border-box;width:100%;height:38vh;font:12px monospace}</style>
<div id=s>Connecting…</div><div id=b></div>
<div class=actions><input id=n placeholder="Note (e.g. VCC=3V3)"><button onclick="note()">Add</button></div>
<textarea id=t readonly></textarea><div class=actions><button onclick="cp()">Copy</button><a href=/dl download=diag.txt>Download log</a><button onclick="fetch('/clr')">Clear</button></div>
<script>
const J=[[1,'I2C scan'],[2,'RTC read'],[3,'GPIO4 8s'],[4,'AS608 params'],[5,'Enroll slot 1'],[6,'10 scans'],[7,'Beep 50'],[8,'Beep 100'],[9,'Template round-trip'],[10,'10 scans AP OFF']];
const b=document.getElementById('b'),t=document.getElementById('t'),s=document.getElementById('s');
const controls=[];
J.forEach(function(x){
  var e=document.createElement('button');
  e.textContent=x[1];
  e.onclick=function(){
    s.textContent='Requesting: '+x[1]+'…';
    fetch('/run?j='+x[0]).then(function(r){return r.text()}).then(function(msg){
      if(msg!=='ok') s.textContent='Not started: '+msg;
      poll();
    }).catch(function(){s.textContent='Connection error while starting '+x[1]});
  };
  controls.push(e); b.appendChild(e);
});
function note(){fetch('/note?t='+encodeURIComponent(document.getElementById('n').value))}
function cp(){t.select();document.execCommand('copy')}
function poll(){
  fetch('/log',{cache:'no-store'}).then(function(r){return r.text()}).then(function(x){
    var p=x.indexOf('\n');
    var head=p>=0?x.slice(0,p):x;
    s.textContent=head;
    t.value=p>=0?x.slice(p+1):'';
    t.scrollTop=t.scrollHeight;
    var active=head.indexOf('RUNNING')>=0 || head.indexOf('QUEUED')>=0;
    controls.forEach(function(e){e.disabled=active});
  }).catch(function(){s.textContent='Live connection interrupted; retrying…'});
}
setInterval(poll,250);poll();
</script>)HTML";

static const char* rstName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "POWERON";
    case ESP_RST_SW: return "SW";
    case ESP_RST_PANIC: return "PANIC";
    case ESP_RST_BROWNOUT: return "BROWNOUT";
    case ESP_RST_EXT: return "EXT";
    default: return "OTHER";
  }
}

void setup() {
  pinMode(PIN_BUZ, OUTPUT);
  digitalWrite(PIN_BUZ, HIGH);
  LittleFS.begin(true);
  Serial.begin(115200);
  Serial2.setRxBufferSize(2048);   // must come before begin()
  Serial2.begin(57600, SERIAL_8N1, PIN_RX2, PIN_TX2);
  prefs.begin("diag", false);
  uint32_t boots = prefs.getUInt("boots", 0) + 1;
  prefs.putUInt("boots", boots);
  lg("BOOT #%u reset=%s", (unsigned)boots, rstName(esp_reset_reason()));
  lg("prev run lasted >= %u min; last RTC read: %s", (unsigned)prefs.getUInt("up", 0),
     prefs.getString("rtc", "none").c_str());
  prefs.putUInt("up", 0);
  apStart();
  srv.on("/", []() { srv.send(200, "text/html", PAGE); });
  srv.on("/log", []() {
    char h[180];
    int shownJob = busy ? activeJob : job;
    const char* state = busy ? "RUNNING" : (job ? "QUEUED" : "IDLE");
    snprintf(h, sizeof h, "%s: %s | clients=%d cpu=%uMHz up=%lus heap=%u\n",
             state, shownJob ? jobName(shownJob) : "ready", WiFi.softAPgetStationNum(),
             (unsigned)getCpuFrequencyMhz(), (unsigned long)(millis() / 1000), (unsigned)ESP.getFreeHeap());
    srv.send(200, "text/plain", String(h) + L);
  });
  srv.on("/dl", []() {
    File f = LittleFS.open("/d.txt", "r");
    if (!f) { srv.send(200, "text/plain", "empty"); return; }
    srv.sendHeader("Content-Disposition", "attachment; filename=diag.txt");
    srv.streamFile(f, "text/plain");
    f.close();
  });
  srv.on("/clr", []() { LittleFS.remove("/d.txt"); L = ""; srv.send(200, "text/plain", "ok"); });
  srv.on("/run", []() {
    if (busy || job) { srv.send(200, "text/plain", "busy"); return; }
    job = srv.arg("j").toInt();
    srv.send(200, "text/plain", "ok");
  });
  srv.on("/note", []() {
    lg("NOTE: %s", srv.arg("t").c_str());
    srv.send(200, "text/plain", "ok");
  });
  srv.begin();
}

void loop() {
  srv.handleClient();
  if (job && !busy) {
    int j = job;
    job = 0;
    busy = true;
    activeJob = j;
    runJob(j);
    activeJob = 0;
    busy = false;
  }
  if (millis() - lastSave > 60000) {
    lastSave = millis();
    prefs.putUInt("up", millis() / 60000);
  }
}