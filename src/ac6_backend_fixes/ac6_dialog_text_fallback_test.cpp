#include "../ac6_dialog_text_fallback.h"

#include <cassert>

int main() {
  using ac6::dialog_text::ReplacementFor;

  assert(ReplacementFor("M70000_122") ==
         "No ACE COMBAT 6 Game Data.\nCreate new Game Data?");
  assert(ReplacementFor("M70000_222").empty());
  assert(ReplacementFor("LOAD_W_003").empty());

  const auto reference = ac6::dialog_text::LayoutForHeight(720.0f);
  assert(reference.font_size == 28.0f);
  assert(reference.first_line_y == 278.0f);
  assert(reference.line_spacing == 34.0f);

  const auto half = ac6::dialog_text::LayoutForHeight(360.0f);
  assert(half.font_size == 14.0f);
  assert(half.first_line_y == 139.0f);
  assert(half.line_spacing == 17.0f);
  return 0;
}
