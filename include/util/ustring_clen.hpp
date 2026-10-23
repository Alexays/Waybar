#pragma once
#include <glibmm/ustring.h>

// calculate column width of ustring
static inline int ustring_clen(const Glib::ustring& str) {
  int total = 0;
  for (unsigned int i : str) {
    total += g_unichar_iswide(i) + 1;
  }
  return total;
}
