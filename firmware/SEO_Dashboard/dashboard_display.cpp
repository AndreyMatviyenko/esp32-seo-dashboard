#include "dashboard_display.h"
#include "dashboard_pages.h"
#include "cwv_quality.h"
#include "epd2in9_V2.h"
#include "epdpaint.h"

namespace dashboard {
namespace display {
namespace {
// All entry points are Arduino-task only. HTTP never accesses the framebuffer
// or SPI, preserving the SSD1680 reset timing on the single-core ESP32-C3.
Epd epd;
unsigned char image[EPD_WIDTH * EPD_HEIGHT / 8];
Paint paint(image, EPD_WIDTH, EPD_HEIGHT);

String localNow(const Settings& settings) {
  const time_t now = time(nullptr);
  if (now < 1700000000) return "--:-- --.--";
  const TimeZoneOption* zone = findTimeZone(settings.timeZone);
  setenv("TZ", zone ? zone->rule : kTimeZones[0].rule, 1); tzset();
  struct tm local; localtime_r(&now, &local);
  char buffer[20]; strftime(buffer, sizeof(buffer), "%H:%M %d.%m", &local);
  setenv("TZ", "UTC0", 1); tzset();
  return buffer;
}

String displayedValue(const String& value, const ProviderState& state) {
  // Preserve last-known values on transient failures and mark them with '*'.
  // Never substitute zero for a service that has not returned valid data yet.
  return state.updated && value != "--" ? value + (state.status == Status::Live ? "" : "*") : "--";
}
void drawScaledText(int x, int y, const String& value, sFONT* font, int scale) {
  // Waveshare fonts store MSB-first rows padded to whole bytes. Integer pixel
  // scaling preserves hard black/white edges on the monochrome framebuffer.
  const int rowBytes = (font->Width + 7) / 8;
  for (size_t character = 0; character < value.length(); ++character) {
    unsigned char ch = value[character];
    if (ch < ' ' || ch > '~') ch = '?';
    const unsigned char* glyph = font->table + (ch - ' ') * font->Height * rowBytes;
    for (int row = 0; row < font->Height; ++row) {
      for (int column = 0; column < font->Width; ++column) {
        if (!(pgm_read_byte(glyph + row * rowBytes + column / 8) & (0x80 >> (column % 8)))) continue;
        for (int dy = 0; dy < scale; ++dy)
          for (int dx = 0; dx < scale; ++dx)
            paint.DrawPixel(x + (character * font->Width + column) * scale + dx, y + row * scale + dy, 0);
      }
    }
  }
}
void drawMetric(int x, int y, int width, int height, const String& label, const String& value) {
  paint.DrawStringAt(x + 8, y + 2, label.c_str(), &Font8, 0);
  sFONT* chosen = nullptr;
  int chosenScale = 1, chosenHeight = 0;
  sFONT* fonts[] = {&Font24, &Font20, &Font12, &Font8};
  for (sFONT* font : fonts) {
    for (int scale = 1; scale <= 3; ++scale) {
      const int pixels = font->Height * scale;
      if (int(value.length()) * font->Width * scale <= width - 16 &&
          pixels <= height - 12 && pixels > chosenHeight) {
        chosen = font;
        chosenScale = scale;
        chosenHeight = pixels;
      }
    }
  }
  if (chosen) {
    drawScaledText(x + 8, y + 12 + (height - 12 - chosenHeight) / 2, value, chosen, chosenScale);
    return;
  }
  // Extremely long counts remain bounded to the cell instead of overwriting a neighbour.
  const String clipped = value.substring(0, (width - 16) / Font8.Width - 1) + "~";
  paint.DrawStringAt(x + 8, y + 12, clipped.c_str(), &Font8, 0);
}
void drawHeader(const Settings& settings, const String& title, const String& status) {
  paint.SetRotate(ROTATE_90);
  paint.Clear(1);
  const String header = status + " " + localNow(settings);
  const int headerX = 296 - 6 - header.length() * Font12.Width;
  const int capacity = max(0, (headerX - 16) / int(Font12.Width));
  paint.DrawStringAt(8, 3, title.substring(0, capacity).c_str(), &Font12, 0);
  paint.DrawStringAt(headerX, 3, header.c_str(), &Font12, 0);
  paint.DrawHorizontalLine(8, 19, 280, 0);
}
String money(const String& amount, const String& currency) {
  if (amount == "--") return amount;
  return (currency == "USD" ? "$" : currency.length() ? currency + " " : "") + amount;
}
String shortDate(const String& date) {
  return date.length() == 10 ? date.substring(8, 10) + "." + date.substring(5, 7) + "." + date.substring(0, 4) : "--";
}
ProviderState detailState(const ProviderState& provider, const ProviderState& group) {
  ProviderState state = group;
  if (provider.status != Status::Live) state.status = provider.status;
  return state;
}
// Geometry is shared by overview and detail pages. Detail pages reserve the
// bottom lines for the report period; values retain each provider's stale mark.
void drawGrid(const String* labels, const String* values, int count, int contentHeight = 108,
              const ProviderState* state = nullptr) {
  if (count <= 0) return;
  const int columns = count <= 3 ? 1 : 2;
  const int rows = (count + columns - 1) / columns;
  const int height = contentHeight / rows;
  for (int i = 0; i < count; ++i) {
    const bool lastFull = columns == 2 && count % 2 && i == count - 1;
    drawMetric((i % columns) * (296 / columns), 20 + (i / columns) * height,
      lastFull ? 296 : 296 / columns, height, labels[i], state ? displayedValue(values[i], *state) : values[i]);
  }
}

String cwvValue(double value, CwvThresholds thresholds, double divisor, unsigned int decimals, const char* unit) {
  const CwvQuality quality = cwvQuality(value, thresholds);
  if (quality == CwvQuality::NoData) return "--";
  String number(value / divisor, decimals);
  const unsigned int defaultDecimals = decimals;
  // Add precision only if rounding would visibly put the value in another band:
  // 2501 ms becomes 2.501 s, instead of 2.50 s / NEEDS WORK.
  while (decimals < 6 && cwvQuality(number.toDouble() * divisor, thresholds) != quality)
    number = String(value / divisor, ++decimals);
  if (cwvQuality(number.toDouble() * divisor, thresholds) != quality) {
    const double boundary = quality == CwvQuality::Poor ? thresholds.needsWork : thresholds.good;
    number = ">" + String(boundary / divisor, defaultDecimals);
  }
  return number + unit;
}

void drawOverview(const DeviceConfig& config, const DashboardData& data, const String& ipAddress) {
  const auto serviceEnabled = [&](Service service) { return config.services[service].ready(service); };
  const auto value = [&](Service service, const String& metric) {
    return displayedValue(metric, data.providers[service]);
  };
  drawHeader(config.network, config.title, statusLabel(data.overall));
  String labels[6], values[6];
  int count = 0;
  // Geometry depends on configuration, not request success. A temporary API
  // error keeps its tile in place while unconfigured services use no space.
  const auto metric = [&](const String& label, const String& displayed) {
    labels[count] = label;
    values[count++] = displayed;
  };
  if (serviceEnabled(GA4)) {
    metric("LIVE USERS", value(GA4, data.ga4.realtimeUsers));
    metric("NEW USERS / 28D", value(GA4, data.ga4.newUsers28));
  }
  if (serviceEnabled(AdSense)) {
    metric("ADS / TODAY", value(AdSense, money(data.adsense.today, data.adsense.currency)));
    metric("ADS / MONTH", value(AdSense, money(data.adsense.month, data.adsense.currency)));
  }
  if (serviceEnabled(GSC)) metric("GSC CLICKS / 28D ALL", value(GSC, data.gsc.clicks));
  if (serviceEnabled(CrUX)) {
    const String date = data.crux.lastDate.length() == 10 ?
      data.crux.lastDate.substring(8, 10) + "." + data.crux.lastDate.substring(5, 7) : "--.--";
    metric("CWV MOBILE / " + date, value(CrUX, data.crux.assessment));
  }
  if (!count) {
    paint.DrawStringAt(8,38,"Enable a service in Settings",&Font12,0);
    paint.DrawStringAt(8,63,"http://seo-dashboard.local",&Font8,0);
    String ip="http://"+ipAddress;
    paint.DrawStringAt(8,80,ip.c_str(),&Font8,0);
    paint.DrawStringAt(8,108,"USB command: setup (shows password)",&Font8,0);
  } else {
    drawGrid(labels, values, count);
  }
}

void drawAdSense(const DashboardData& data) {
  const ProviderState& state = data.providers[AdSense];
  const auto& ads = data.adsense;
  const ProviderState history = detailState(state, ads.history);
  const ProviderState payments = detailState(state, ads.payments);
  const String labels[] = {"TODAY", "YESTERDAY", "LAST 7D / EXCL TODAY", "MONTH TO DATE",
    "UNPAID BALANCE", "PAID / " + shortDate(ads.lastPaymentDate)};
  const String values[] = {
    displayedValue(money(ads.today, ads.currency), state),
    displayedValue(money(ads.yesterday, ads.historyCurrency), history),
    displayedValue(money(ads.last7, ads.historyCurrency), history),
    displayedValue(money(ads.month, ads.currency), state),
    displayedValue(ads.balance, payments), displayedValue(ads.lastPayment, payments)
  };
  drawGrid(labels, values, 6, 96);
  String note = "AS OF " + shortDate(ads.day) + " / ACCOUNT TIME ZONE";
  if (history.status != Status::Live || payments.status != Status::Live)
    note = String("HISTORY: ") + statusLabel(history.status) + " / PAY: " + statusLabel(payments.status);
  // Long diagnostics stay available in Serial; keep the footer inside 280px.
  paint.DrawStringAt(8, 119, note.substring(0, 56).c_str(), &Font8, 0);
}

void drawGa4(const DashboardData& data) {
  const ProviderState& state = data.providers[GA4];
  const String labels[] = {"LIVE USERS / 30 MIN", "NEW USERS / 28D", "TOTAL USERS / 28D",
    "SESSIONS / 28D", "VIEWS / 28D", "ENGAGEMENT / 28D"};
  const String values[] = {data.ga4.realtimeUsers, data.ga4.newUsers28, data.ga4.totalUsers28,
    data.ga4.sessions28, data.ga4.views28, data.ga4.engagementRate28};
  drawGrid(labels, values, 6, 96, &state);
  paint.DrawStringAt(8, 119, "28 COMPLETE PROPERTY DAYS / ENDS YESTERDAY", &Font8, 0);
}

void drawGsc(const DashboardData& data) {
  const ProviderState& state = data.providers[GSC];
  const String labels[] = {"CLICKS / 28D", "IMPRESSIONS / 28D", "CTR", "AVG POSITION"};
  const String values[] = {data.gsc.clicks, data.gsc.impressions, data.gsc.ctr, data.gsc.position};
  drawGrid(labels, values, 4, 84, &state);
  const String period = shortDate(data.gsc.start) + " - " + shortDate(data.gsc.end);
  paint.DrawStringAt(8, 106, period.c_str(), &Font8, 0);
  paint.DrawStringAt(8, 119, "WEB / ALL, INCLUDING PRELIMINARY / PT", &Font8, 0);
}

void drawCwv(const DashboardData& data) {
  const ProviderState& state = data.providers[CrUX];
  const auto& cwv = data.crux;
  const auto label = [&](const char* name, double value, CwvThresholds thresholds) {
    const CwvQuality quality = cwvQuality(state.updated ? value : NAN, thresholds);
    String text = String(name) + " / p75 / " + qualityLabel(quality);
    // The quality describes the same cached p75 as the numeric value below.
    if (quality != CwvQuality::NoData && state.status != Status::Live) text += "*";
    return text;
  };
  const String labels[] = {"ASSESSMENT", label("LCP", cwv.lcp, kLcpThresholds),
    label("INP", cwv.inp, kInpThresholds), label("CLS", cwv.cls, kClsThresholds)};
  const String values[] = {cwv.assessment, cwvValue(cwv.lcp, kLcpThresholds, 1000, 2, " s"),
    cwvValue(cwv.inp, kInpThresholds, 1, 0, " ms"), cwvValue(cwv.cls, kClsThresholds, 1, 3, "")};
  drawGrid(labels, values, 4, 84, &state);
  const String period = shortDate(cwv.firstDate) + " - " + shortDate(cwv.lastDate);
  paint.DrawStringAt(8, 106, period.c_str(), &Font8, 0);
  paint.DrawStringAt(8, 119, "PHONE / ORIGIN / 75TH PERCENTILE", &Font8, 0);
}

void drawSetup(const char* setupSsid, const char* setupPassword) {
  // Show the public setup password locally; private service credentials stay hidden.
  paint.SetRotate(ROTATE_90); paint.Clear(1);
  paint.DrawStringAt(8,5,"SEO DASHBOARD / SETUP",&Font12,0);
  paint.DrawHorizontalLine(8,23,280,0);
  paint.DrawStringAt(8,33,setupSsid,&Font12,0);
  paint.DrawStringAt(8,53,"http://192.168.4.1",&Font12,0);
  paint.DrawStringAt(8,73,"Password:",&Font12,0);
  paint.DrawStringAt(80,73,setupPassword,&Font12,0);
  paint.DrawStringAt(8,98,"Same password for Wi-Fi and browser",&Font8,0);
  paint.DrawStringAt(8,112,"Browser login: admin",&Font8,0);
}

void printDisplayBusyTrace(const char* stage,const EpdBusyTrace& trace) {
  if (!trace.sampled) {
    Serial.printf("DISPLAY %s BUSY: not sampled\n",stage);
    return;
  }
  Serial.printf("DISPLAY %s BUSY: first=%d; HIGH seen=%s; last=%d\n",
    stage,trace.first,trace.highSeen ? "YES" : "NO",trace.last);
}
bool transferDashboardFrame(unsigned int attempt) {
  // Always release the SPI transaction on success and every error return.
  struct DisplaySessionGuard {
    ~DisplaySessionGuard() { EpdIf::IfEnd(); }
  } displaySession;
  Serial.printf("DISPLAY ATTEMPT %u/2; INIT START; BUSY=%d\n",attempt,digitalRead(BUSY_PIN));
  Serial.flush();
  uint32_t displayStart=millis();
  int initResult=epd.Init();
  Serial.printf("DISPLAY INIT RETURNED; result=%d; elapsed=%lu ms; BUSY=%d\n",
    initResult,(unsigned long)(millis()-displayStart),digitalRead(BUSY_PIN));
  const EpdInitDiagnostics& diag=epd.GetInitDiagnostics();
  Serial.printf("DISPLAY RST LOW software interval=%lu us (not a pin measurement)\n",(unsigned long)diag.resetLowUs);
  printDisplayBusyTrace("RST release",diag.reset);
  printDisplayBusyTrace("SWRESET",diag.softwareReset);
  Serial.flush();
  if (initResult!=0) {
    Serial.printf("DISPLAY ERROR: %s (init)\n",epd.GetLastError());
    return false;
  }
  epd.ClearFrameMemory(0xFF);
  if (epd.HasError()) {
    Serial.printf("DISPLAY ERROR: %s (RAM clear)\n",epd.GetLastError());
    return false;
  }
  Serial.println("DISPLAY RAM CLEAR RETURNED"); Serial.flush();
  epd.SetFrameMemory(image,0,0,EPD_WIDTH,EPD_HEIGHT);
  if (epd.HasError()) {
    Serial.printf("DISPLAY ERROR: %s (image write)\n",epd.GetLastError());
    return false;
  }
  Serial.printf("DISPLAY IMAGE WRITTEN; BUSY=%d\n",digitalRead(BUSY_PIN)); Serial.flush();
  uint32_t refreshStart=millis();
  bool refreshOK=epd.DisplayFrame();
  Serial.printf("DISPLAY REFRESH %s; elapsed=%lu ms; BUSY=%d\n",refreshOK ? "CONFIRMED" : "FAILED",
    (unsigned long)(millis()-refreshStart),digitalRead(BUSY_PIN)); Serial.flush();
  if (!refreshOK) {
    Serial.printf("DISPLAY ERROR: %s; sleep skipped\n",epd.GetLastError());
    return false;
  }
  // An SPI write has no controller readback. Sleep only after the driver has
  // observed a complete BUSY HIGH -> LOW cycle for this refresh.
  epd.Sleep();
  Serial.println("DISPLAY SLEEP SENT after confirmed BUSY HIGH -> LOW");
  return true;
}
}

bool prepare() { return EpdIf::Prepare() == 0; }

static uint8_t reversePixelBits(uint8_t value) {
  value = (value >> 4) | (value << 4);
  value = ((value & 0xCC) >> 2) | ((value & 0x33) << 2);
  return ((value & 0xAA) >> 1) | ((value & 0x55) << 1);
}

static void rotateFrame180() {
  // Native rows are byte-aligned and MSB-first. Reverse pixel order across the
  // entire buffer: a true 180-degree turn, with no second framebuffer or changes
  // to Waveshare's reset/LUT/refresh sequence. Setup and error screens use it too.
  static_assert(EPD_WIDTH % 8 == 0 && sizeof(image) % 2 == 0, "Expected byte-aligned e-paper frame");
  for (size_t i = 0; i < sizeof(image) / 2; ++i) {
    const size_t opposite = sizeof(image) - 1 - i;
    const uint8_t first = image[i];
    image[i] = reversePixelBits(image[opposite]);
    image[opposite] = reversePixelBits(first);
  }
}

static RefreshResult presentFrame(const DeviceConfig& config) {
  // Clear() recreates the frame every time; theme inversion never accumulates.
  // Prepare orientation once; the recovery attempt reuses the same buffer.
  if (config.network.rotate180) rotateFrame180();
  Serial.printf("DISPLAY ORIENTATION: %u degrees\n", config.network.rotate180 ? 180U : 0U);
  size_t blackPixels=0;
  for (size_t i=0;i<sizeof(image);i++) {
    if (config.network.dark) image[i]^=0xFF;
    blackPixels+=8-__builtin_popcount((unsigned int)image[i]);
  }
  Serial.printf("DISPLAY THEME: %s; black pixels=%u / %u\n",config.network.dark ? "DARK" : "LIGHT",(unsigned int)blackPixels,(unsigned int)(sizeof(image)*8));
  if (transferDashboardFrame(1)) return {true, ""};
  if (epd.GetError()!=EpdError::RefreshNotStarted) {
    Serial.println("DISPLAY RETRY SKIPPED: not REFRESH_NOT_STARTED; next scheduled update");
    return {false, epd.GetLastError()};
  }
  Serial.println("DISPLAY RECOVERY: watching BUSY for 1000 ms before one SPI restart");
  if (!epd.CanRetryRefresh()) {
    Serial.println("DISPLAY RETRY SKIPPED: BUSY HIGH observed; next scheduled update");
    return {false, epd.GetLastError()};
  }
  int restartResult=EpdIf::RestartSpi();
  Serial.printf("DISPLAY SPI RESTART: result=%d (0=OK, -1=init failure, -2=BUSY HIGH)\n",restartResult);
  if (restartResult!=0) return {false, restartResult==-2 ? "BUSY_HIGH" : "SPI_INIT_FAILED"};
  // Reuse the same prepared frame. Do not invert again or repeat API requests.
  if (!transferDashboardFrame(2)) {
    Serial.println("DISPLAY RECOVERY FAILED: no more retries; next scheduled update");
    return {false, epd.GetLastError()};
  }
  return {true, ""};
}

RefreshResult showDashboard(const DeviceConfig& config, const DashboardData& data, const String& ipAddress) {
  drawOverview(config, data, ipAddress);
  return presentFrame(config);
}

RefreshResult showPage(const DeviceConfig& config, const DashboardData& data, DashboardPage page, const String& ipAddress) {
  if (page == DashboardPage::Overview || !pageEnabled(config, page)) return showDashboard(config, data, ipAddress);
  const Service service = pageService(page);
  String status = statusLabel(data.providers[service].status);
  if (service == AdSense && data.providers[service].status == Status::Live &&
      (data.adsense.history.status != Status::Live || data.adsense.payments.status != Status::Live)) status = "PARTIAL";
  drawHeader(config.network, pageLabel(page), status);
  switch (page) {
    case DashboardPage::AdSense: drawAdSense(data); break;
    case DashboardPage::GA4: drawGa4(data); break;
    case DashboardPage::GSC: drawGsc(data); break;
    case DashboardPage::CWV: drawCwv(data); break;
    case DashboardPage::Overview: break;
  }
  return presentFrame(config);
}

RefreshResult showSetup(const DeviceConfig& config, const char* setupSsid, const char* setupPassword) {
  drawSetup(setupSsid, setupPassword);
  return presentFrame(config);
}
}
}
