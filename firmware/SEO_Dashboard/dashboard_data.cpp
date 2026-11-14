#include "dashboard_data.h"

namespace dashboard {
const char* statusLabel(Status status) {
  switch (status) {
    case Status::Disabled: return "DISABLED";
    case Status::Waiting: return "WAITING";
    case Status::Live: return "LIVE";
    case Status::WifiError: return "WIFI ERROR";
    case Status::ClockError: return "CLOCK ERROR";
    case Status::HttpsError: return "HTTPS ERROR";
    case Status::AuthRequired: return "AUTH REQUIRED";
    case Status::AccessDenied: return "ACCESS DENIED";
    case Status::ApiError: return "API ERROR";
    case Status::JsonError: return "JSON ERROR";
    case Status::AuthError: return "AUTH ERROR";
    case Status::AccountError: return "ACCOUNT ERROR";
    case Status::ApiWarning: return "API WARNING";
    case Status::NoData: return "NO DATA";
    case Status::DateMismatch: return "DATE MISMATCH";
    case Status::MemoryError: return "MEMORY ERROR";
    case Status::CwvDateError: return "CWV DATE ERROR";
    case Status::TransferError: return "TRANSFER ERROR";
    case Status::ResponseTooLarge: return "RESPONSE TOO LARGE";
  }
  return "API ERROR";
}
const char* statusLabel(OverallStatus status) {
  switch (status) {
    case OverallStatus::Setup: return "SETUP";
    case OverallStatus::Live: return "LIVE";
    case OverallStatus::Partial: return "PARTIAL";
    case OverallStatus::Error: return "ERROR";
  }
  return "ERROR";
}
const char* statusLabel(DisplayState state) {
  switch (state) {
    case DisplayState::Waiting: return "WAITING";
    case DisplayState::Updating: return "UPDATING";
    case DisplayState::Ready: return "READY";
    case DisplayState::Error: return "ERROR";
  }
  return "ERROR";
}
const char* frameLabel(DisplayFrame frame) {
  switch (frame) {
    case DisplayFrame::None: return "none";
    case DisplayFrame::Dashboard: return "dashboard";
    case DisplayFrame::Setup: return "setup";
  }
  return "none";
}
}
