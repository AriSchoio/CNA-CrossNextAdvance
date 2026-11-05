# Installed WASM Apps

CrossNext can run sandboxed WASM modules installed on the SD card. The runtime is WAMR and exposes only a small drawing/input API from `goodies/goody.h`.

## Package layout

Create one folder per app under `/.crossnext/apps`:

```
/.crossnext/apps/
  my-app/
    manifest.json
    app.wasm
```

`manifest.json` is optional, but recommended:

```json
{
  "id": "my-app",
  "name": "My App",
  "version": "1.0.0"
}
```

The package is discovered automatically under **Apps > Installed Apps**. No firmware rebuild is required after installing or updating a package.

## Legacy modules

The CrossDink-compatible folder `/debug/goodies/*.wasm` is also discovered. Those files need no package directory and appear as legacy modules.

## WASM ABI

Use the declarations in `goodies/goody.h`.

Every module must export:

```c
void gd_render(void);
int gd_input(int code, int x, int y);
```

`gd_render` draws the active page. `gd_input` receives button or touch input and returns non-zero to request a redraw. The supported button codes are:

```text
GD_UP, GD_DOWN, GD_LEFT, GD_RIGHT, GD_CONFIRM, GD_TAP
```

Host imports are limited to:

```text
gd_width()
gd_height()
gd_line_height()
gd_text(x, y, text, bold)
gd_rect(x, y, width, height, fill)
gd_log(text)
```

## Security and limits

- Modules are interpreter-only WASM.
- The maximum module size is 64 KiB.
- Modules cannot access the panel controller, flash registers, device secrets, or arbitrary memory APIs.
- File/network access is not exposed by the current host ABI.
- Modules run under a 8 KiB WASM stack and have PSRAM-backed linear memory.
- A module trap is contained by the host and shown as an error page.
