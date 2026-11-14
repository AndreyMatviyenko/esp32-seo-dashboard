#include "dashboard_pages.h"

namespace dashboard {
namespace {
struct Page { DashboardPage id; Service service; const char* title; };
// Navigation, labels and provider lookup share this explicit order. Enum values
// may change without changing which service a page displays.
constexpr Page pages[] = {
  {DashboardPage::Overview, ServiceCount, "OVERVIEW"},
  {DashboardPage::AdSense, AdSense, "ADSENSE"},
  {DashboardPage::GA4, GA4, "GA4"},
  {DashboardPage::GSC, GSC, "GSC"},
  {DashboardPage::CWV, CrUX, "CWV MOBILE"}
};
constexpr size_t kPageCount = sizeof(pages) / sizeof(pages[0]);
}

const char* pageLabel(DashboardPage page) {
  for (const Page& item : pages) if (item.id == page) return item.title;
  return "OVERVIEW";
}
Service pageService(DashboardPage page) {
  for (const Page& item : pages) if (item.id == page) return item.service;
  return ServiceCount;
}
bool pageEnabled(const DeviceConfig& config, DashboardPage page) {
  if (page == DashboardPage::Overview) return true;
  const Service service = pageService(page);
  return service < ServiceCount && config.services[service].ready(service);
}
size_t enabledPageCount(const DeviceConfig& config) {
  size_t count = 0;
  for (const Page& item : pages) if (pageEnabled(config, item.id)) ++count;
  return count;
}
DashboardPage nextDashboardPage(const DeviceConfig& config, DashboardPage page) {
  size_t index = 0;
  for (size_t i = 0; i < kPageCount; ++i) if (pages[i].id == page) index = i;
  for (size_t i = 0; i < kPageCount; ++i) {
    index = (index + 1) % kPageCount;
    if (pageEnabled(config, pages[index].id)) return pages[index].id;
  }
  return DashboardPage::Overview;
}
}
