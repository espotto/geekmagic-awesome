#include "SysText.h"
#if defined(ARDUINO)
#include <pgmspace.h>
#else
#define PROGMEM
#define pgm_read_byte(p) (*reinterpret_cast<const uint8_t*>(p))
#endif

namespace sys {
namespace {

// sys::Word order, '|'-separated: the English words, then the Russian ones.
// In flash: on the ESP8266 a table of literals would sit in RAM.
const char kWords[] PROGMEM =
  "Wi-Fi setup|Scan to join|Open network|Password|Connected|Open in a browser|"
  "Crashed|Flash a fix at|Recovering|Updating|Update failed|Restarting|"
  "Starting setup|Looking for Wi-Fi|Joining %s|"
  "auth timed out|wrong password?|network not found|security mode?|router refused|"
  "signal lost|join failed|no IP (DHCP)|timed out|Calendar|Waiting for the time|"
  "INFO|WARNING|ALERT|TASK DONE|NEEDS YOU|"
  "Watch face|Waiting for free memory|Select a watch face|Watch face file not found|Watch face ID mismatch|Could not read the watch face|"
  "Настройка Wi-Fi|Отсканируйте код|Открытая сеть|Пароль|Подключено|Откройте в браузере|"
  "Сбой|Прошейте исправление|Восстановление|Обновление|Ошибка обновления|Перезагрузка|"
  "Запуск настройки|Поиск Wi-Fi|Подключение к %s|"
  "нет ответа|неверный пароль?|сеть не найдена|режим защиты?|роутер отказал|"
  "сигнал потерян|не подключилось|нет IP (DHCP)|нет ответа|Календарь|Ожидание времени|"
  "ИНФО|ВНИМАНИЕ|ТРЕВОГА|ГОТОВО|ЖДЁТ ВАС|"
  "Циферблат|Ожидание свободной памяти|Выберите циферблат|Файл циферблата не найден|ID циферблата не совпадает|Не удалось прочитать циферблат";

uint8_t g_lang = 0;

}  // namespace

void setLanguage(uint8_t lang) { g_lang = lang == 1 ? 1 : 0; }
uint8_t language() { return g_lang; }

const char* textIn(uint8_t lang, Word w, char* out, size_t cap) {
  int index = (lang == 1 ? WordCount : 0) + (w < WordCount ? w : 0);
  size_t n = 0;
  for (const char* p = kWords;; ++p) {
    const char c = static_cast<char>(pgm_read_byte(p));
    if (c == '|' || c == 0) {
      if (index-- == 0 || c == 0) break;
      continue;
    }
    if (index == 0 && n + 1 < cap) out[n++] = c;
  }
  if (cap) out[n] = 0;
  return out;
}

const char* text(Word w, char* out, size_t cap) { return textIn(g_lang, w, out, cap); }

}  // namespace sys
