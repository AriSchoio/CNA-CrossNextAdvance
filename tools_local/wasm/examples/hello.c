#include "../../../goodies/goody.h"

GD_EXPORT(gd_render)
void gd_render(void) {
  gd_text(12, 12, "Hello from an installed WASM app", 1);
  gd_rect(12, 48, gd_width() - 24, gd_line_height() + 20, 0);
  gd_text(24, 62, "Press Confirm or tap the screen", 0);
}

GD_EXPORT(gd_input)
int gd_input(int code, int x, int y) {
  gd_log("WASM app received input");
  (void)code;
  (void)x;
  (void)y;
  return 1;
}
