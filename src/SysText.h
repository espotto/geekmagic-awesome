// SysText.h — the words of the device's own screens (start-up, Wi-Fi setup,
// crash, Wi-Fi failures, a few messages) in the device language: English or
// Russian (settings clock.lang, gm::languageCode order). UTF-8, in flash;
// text() copies one out. Builds without Arduino (tests/screens).
#pragma once
#include <stddef.h>
#include <stdint.h>

namespace sys {

enum Word : uint8_t {
  WifiSetup, ScanToJoin, OpenNetwork, Password, Connected, OpenInBrowser,
  Crashed, FlashFixAt, Recovering, Updating, UpdateFailed, Restarting,
  StartingSetup, LookingForWifi, Joining,   // Joining: a format with one %s
  AuthTimedOut, WrongPassword, NetworkNotFound, SecurityMode, RouterRefused,
  SignalLost, JoinFailed, NoIp, TimedOut, Calendar, WaitingForTime,
  // A notification with no text of its own: its type, in notify_types order.
  NotifyInfo, NotifyWarning, NotifyAlert, NotifyDone, NotifyWaiting,
  // A watch face that cannot be shown (ThemeMode): the title, then why.
  WatchFace, FaceWaitingMemory, FaceSelect, FaceFileMissing, FaceIdMismatch, FaceAssetError,
  WordCount
};

// The language the words come in (0 English, 1 Russian; anything else English).
void setLanguage(uint8_t lang);
uint8_t language();
// Word `w` in the current language, UTF-8, into `out`; returns `out`.
const char* text(Word w, char* out, size_t cap);
// The same in a given language (tests).
const char* textIn(uint8_t lang, Word w, char* out, size_t cap);

}  // namespace sys
