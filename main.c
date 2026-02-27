#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct ImageRGBA
{
  int w;
  int h;
  uint32_t *pixels;
} ImageRGBA;

typedef struct ImageGray
{
  int w;
  int h;
  uint8_t *pixels;
} ImageGray;

static IWICImagingFactory *g_wic = NULL;

static void PrintText(const char *s)
{
  OutputDebugStringA(s);
  HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
  if (h && h != INVALID_HANDLE_VALUE)
  {
    DWORD written = 0;
    WriteFile(h, s, (DWORD)lstrlenA(s), &written, NULL);
  }
}

static void PrintFPS(int fps)
{
  char buf[64];
  wsprintfA(buf, "fps: %d\n", fps);
  PrintText(buf);
}

static bool InitWIC(void)
{
  if (g_wic) return true;
  HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
  if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return false;
  hr = CoCreateInstance(
    &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
    &IID_IWICImagingFactory, (void **)&g_wic);
  return SUCCEEDED(hr);
}

static wchar_t *ToWide(const char *s)
{
  if (!s || !s[0]) return NULL;
  int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  if (len <= 0) return NULL;
  wchar_t *out = (wchar_t *)malloc((size_t)len * sizeof(wchar_t));
  if (!out) return NULL;
  MultiByteToWideChar(CP_UTF8, 0, s, -1, out, len);
  return out;
}

static bool LoadImageRGBA(const char *path, ImageRGBA *out)
{
  if (!InitWIC()) return false;

  IWICBitmapDecoder *decoder = NULL;
  IWICBitmapFrameDecode *frame = NULL;
  IWICFormatConverter *conv = NULL;
  wchar_t *wpath = NULL;

  wpath = ToWide(path);
  if (!wpath) goto Fail;

  HRESULT hr = g_wic->lpVtbl->CreateDecoderFromFilename(
    g_wic, wpath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
  if (FAILED(hr)) goto Fail;

  hr = decoder->lpVtbl->GetFrame(decoder, 0, &frame);
  if (FAILED(hr)) goto Fail;

  hr = g_wic->lpVtbl->CreateFormatConverter(g_wic, &conv);
  if (FAILED(hr)) goto Fail;

  hr = conv->lpVtbl->Initialize(
    conv, (IWICBitmapSource *)frame, &GUID_WICPixelFormat32bppRGBA,
    WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
  if (FAILED(hr)) goto Fail;

  UINT w = 0, h = 0;
  hr = conv->lpVtbl->GetSize(conv, &w, &h);
  if (FAILED(hr)) goto Fail;

  out->w = (int)w;
  out->h = (int)h;
  out->pixels = (uint32_t *)malloc((size_t)w * (size_t)h * sizeof(uint32_t));
  if (!out->pixels) goto Fail;

  hr = conv->lpVtbl->CopyPixels(
    conv, NULL, w * 4, (UINT)(w * h * 4), (BYTE *)out->pixels);
  if (FAILED(hr)) goto Fail;

  if (conv) conv->lpVtbl->Release(conv);
  if (frame) frame->lpVtbl->Release(frame);
  if (decoder) decoder->lpVtbl->Release(decoder);
  free(wpath);
  return true;

Fail:
  if (conv) conv->lpVtbl->Release(conv);
  if (frame) frame->lpVtbl->Release(frame);
  if (decoder) decoder->lpVtbl->Release(decoder);
  free(out->pixels);
  out->pixels = NULL;
  out->w = 0;
  out->h = 0;
  free(wpath);
  return false;
}

typedef struct MapData
{
  int w;
  int h;
  uint32_t *color;
  uint8_t *depth;
} MapData;

static void MapData_Clear(MapData *map)
{
  free(map->color);
  free(map->depth);
  map->color = NULL;
  map->depth = NULL;
  map->w = 0;
  map->h = 0;
}

static bool MapData_Load(MapData *map, const char *name)
{
  char base[260];
  if (!name) return false;
  wsprintfA(base, "maps/%s/", name);

  char colorPath[300];
  char depthPath[300];
  wsprintfA(colorPath, "%sColor.png", base);
  wsprintfA(depthPath, "%sDepth.png", base);

  ImageRGBA colorImg = {0};
  ImageRGBA depthImg = {0};

  if (!LoadImageRGBA(colorPath, &colorImg)) return false;
  if (!LoadImageRGBA(depthPath, &depthImg))
  {
    free(colorImg.pixels);
    return false;
  }

  if (colorImg.w != depthImg.w || colorImg.h != depthImg.h)
  {
    free(colorImg.pixels);
    free(depthImg.pixels);
    return false;
  }

  MapData_Clear(map);
  map->w = colorImg.w;
  map->h = colorImg.h;
  map->color = (uint32_t *)malloc((size_t)map->w * (size_t)map->h * sizeof(uint32_t));
  map->depth = (uint8_t *)malloc((size_t)map->w * (size_t)map->h * sizeof(uint8_t));
  if (!map->color || !map->depth)
  {
    free(colorImg.pixels);
    free(depthImg.pixels);
    MapData_Clear(map);
    return false;
  }

  const size_t count = (size_t)map->w * (size_t)map->h;
  for (size_t i = 0; i < count; i++)
  {
    uint32_t rgba = colorImg.pixels[i];
    uint8_t r = (uint8_t)(rgba & 0xFF);
    uint8_t g = (uint8_t)((rgba >> 8) & 0xFF);
    uint8_t b = (uint8_t)((rgba >> 16) & 0xFF);
    map->color[i] = (uint32_t)(b | (uint32_t)(g << 8) | (uint32_t)(r << 16));
    map->depth[i] = r;
  }

  ImageGray depthGray = {0};
  depthGray.w = depthImg.w;
  depthGray.h = depthImg.h;
  depthGray.pixels = (uint8_t *)malloc(count * sizeof(uint8_t));
  if (!depthGray.pixels)
  {
    free(colorImg.pixels);
    free(depthImg.pixels);
    MapData_Clear(map);
    return false;
  }

  for (size_t i = 0; i < count; i++)
  {
    uint32_t rgba = depthImg.pixels[i];
    uint8_t r = (uint8_t)(rgba & 0xFF);
    depthGray.pixels[i] = r;
  }

  memcpy(map->depth, depthGray.pixels, count * sizeof(uint8_t));

  free(colorImg.pixels);
  free(depthImg.pixels);
  free(depthGray.pixels);

  return true;
}

typedef struct Camera
{
  float x;
  float y;
  float z;
  float yaw;
  float fov;
} Camera;

typedef struct App
{
  HWND hwnd;
  int winW;
  int winH;
  BITMAPINFO bmi;
  uint32_t *framebuffer;
  MapData map;
  Camera cam;
  bool running;
  int mouseDx;
  int mouseDy;
  bool focused;
} App;

static uint8_t ClampU8(float v)
{
  if (v <= 0.0f) return 0;
  if (v >= 255.0f) return 255;
  return (uint8_t)v;
}

static void ClampCamera(App *app)
{
  if (app->cam.z > 400.0f) app->cam.z = 400.0f;
  if (app->cam.x < 1.0f) app->cam.x = 1.0f;
  if (app->cam.y < 1.0f) app->cam.y = 1.0f;
  if (app->cam.x >= app->map.w - 2) app->cam.x = (float)(app->map.w - 2);
  if (app->cam.y >= app->map.h - 2) app->cam.y = (float)(app->map.h - 2);

  int ix = (int)app->cam.x;
  int iy = (int)app->cam.y;
  size_t idx = (size_t)ix + (size_t)iy * (size_t)app->map.w;
  float heightUnder = (float)app->map.depth[idx] + 1.0f;
  if (app->cam.z < heightUnder) app->cam.z = heightUnder;
}

static void RenderVoxelSurf(App *app)
{
  const int winW = app->winW;
  const int winH = app->winH;
  const int mapW = app->map.w;
  const int mapH = app->map.h;

  uint32_t *restrict pixels = app->framebuffer;
  const size_t pixelCount = (size_t)winW * (size_t)winH;
  for (size_t i = 0; i < pixelCount; i++)
    pixels[i] = 0x00202020;

  const float camX = app->cam.x;
  const float camY = app->cam.y;
  const float camZ = app->cam.z;

  const uint8_t camZu8 = ClampU8(camZ + 1.0f);

  const float yaw = app->cam.yaw;
  const float forwardX = cosf(yaw);
  const float forwardY = sinf(yaw);
  const float rightX = -forwardY;
  const float rightY = forwardX;

  const float fov = app->cam.fov;
  const float aspect = (float)winW / (float)winH;

  const float halfHeight = tanf(fov * 0.5f);
  const float halfWidth = halfHeight * aspect;

  const float centerY = 0.5f * (float)winH;
  const float scale = centerY / halfHeight;
  const float invScale = 1.0f / scale;
  const float invWidth = 1.0f / (float)winW;

  const uint8_t *restrict depthPtr = app->map.depth;
  const uint32_t *restrict colorPtr = app->map.color;

  uint32_t *restrict columnBottom =
    pixels + (size_t)(winH - 1) * (size_t)winW;

  for (int x = 0; x < winW; x++)
  {
    int highestSeenY = 0;
    float slopeThreshold = (1.0f - centerY) * invScale;

    const float screenX = ((float)x * invWidth) * 2.0f - 1.0f;

    const float dirX = forwardX + rightX * (screenX * halfWidth);
    const float dirY = forwardY + rightY * (screenX * halfWidth);

    const float forwardDot = dirX * forwardX + dirY * forwardY;

    int cellX = (int)camX;
    int cellY = (int)camY;

    const int stepX = dirX >= 0.0f ? 1 : -1;
    const int stepY = dirY >= 0.0f ? 1 : -1;

    const float invDirX = dirX != 0.0f ? 1.0f / dirX : 1e30f;
    const float invDirY = dirY != 0.0f ? 1.0f / dirY : 1e30f;

    const float nextBoundaryX = (float)(cellX + (stepX > 0));
    const float nextBoundaryY = (float)(cellY + (stepY > 0));

    float tMaxX = (nextBoundaryX - camX) * invDirX;
    float tMaxY = (nextBoundaryY - camY) * invDirY;

    const float tDeltaX = fabsf(invDirX);
    const float tDeltaY = fabsf(invDirY);

    float tCur = 0.0f;

    int idx = cellX + cellY * mapW;
    const int yStride = stepY * mapW;

    while (true)
    {
      const bool stepInX = tMaxX < tMaxY;
      const float tNext = stepInX ? tMaxX : tMaxY;

      const uint8_t height = depthPtr[idx];

      const float tCandidate = (height < camZu8) ? tNext : tCur;
      const float forwardDistCandidate = tCandidate * forwardDot;

      const float requiredHeight =
        camZ + slopeThreshold * forwardDistCandidate;

      if ((float)height > requiredHeight)
      {
        const float num = (float)height - camZ;
        const float invForward = 1.0f / forwardDistCandidate;
        const float yEntry = centerY + scale * num * invForward;
        const int yInt = (int)yEntry;

        if (yInt > highestSeenY)
        {
          const bool columnDone = yInt >= winH;
          const int highestY = columnDone ? (winH - 1) : yInt;

          const uint32_t col = colorPtr[idx];
          uint32_t *dat =
            columnBottom - (size_t)highestSeenY * (size_t)winW;

          for (int y = highestSeenY; y < highestY; y++)
          {
            *dat = col;
            dat -= winW;
          }

          highestSeenY = highestY;
          slopeThreshold =
            ((float)(highestSeenY + 1) - centerY) * invScale;

          if (columnDone) break;
        }
      }

      if (stepInX)
      {
        tCur = tMaxX;
        cellX += stepX;
        idx += stepX;
        tMaxX += tDeltaX;
        if ((uint32_t)cellX >= (uint32_t)mapW) break;
      }
      else
      {
        tCur = tMaxY;
        cellY += stepY;
        idx += yStride;
        tMaxY += tDeltaY;
        if ((uint32_t)cellY >= (uint32_t)mapH) break;
      }
    }

    columnBottom++;
  }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  App *app = (App *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  switch (msg)
  {
  case WM_CLOSE:
    if (app) app->running = false;
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  case WM_SETFOCUS:
    if (app) app->focused = true;
    return 0;
  case WM_KILLFOCUS:
    if (app) app->focused = false;
    return 0;
  case WM_INPUT:
    if (app)
    {
      UINT size = 0;
      GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
      if (size)
      {
        uint8_t *data = (uint8_t *)malloc(size);
        if (data)
        {
          if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER)) == size)
          {
            RAWINPUT *ri = (RAWINPUT *)data;
            if (ri->header.dwType == RIM_TYPEMOUSE)
            {
              app->mouseDx += ri->data.mouse.lLastX;
              app->mouseDy += ri->data.mouse.lLastY;
            }
          }
          free(data);
        }
      }
    }
    return 0;
  }
  return DefWindowProc(hwnd, msg, wParam, lParam);
}

static bool InitWindow(App *app)
{
  HINSTANCE inst = GetModuleHandle(NULL);
  WNDCLASSA wc;
  ZeroMemory(&wc, sizeof(wc));
  wc.lpfnWndProc = WndProc;
  wc.hInstance = inst;
  wc.lpszClassName = "VoxelSurfWnd";
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  if (!RegisterClassA(&wc)) return false;

  HWND hwnd = CreateWindowExA(
    0, wc.lpszClassName, "VoxelSurf (clang-only)",
    WS_POPUP, 0, 0, app->winW, app->winH,
    NULL, NULL, inst, NULL);
  if (!hwnd) return false;

  app->hwnd = hwnd;
  SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)app);
  ShowWindow(hwnd, SW_SHOW);

  RAWINPUTDEVICE rid;
  ZeroMemory(&rid, sizeof(rid));
  rid.usUsagePage = 0x01;
  rid.usUsage = 0x02;
  rid.dwFlags = RIDEV_INPUTSINK;
  rid.hwndTarget = hwnd;
  RegisterRawInputDevices(&rid, 1, sizeof(rid));

  ZeroMemory(&app->bmi, sizeof(app->bmi));
  app->bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  app->bmi.bmiHeader.biWidth = app->winW;
  app->bmi.bmiHeader.biHeight = -app->winH;
  app->bmi.bmiHeader.biPlanes = 1;
  app->bmi.bmiHeader.biBitCount = 32;
  app->bmi.bmiHeader.biCompression = BI_RGB;

  app->framebuffer = (uint32_t *)malloc((size_t)app->winW * (size_t)app->winH * sizeof(uint32_t));
  if (!app->framebuffer) return false;

  return true;
}

static void Present(App *app)
{
  HDC dc = GetDC(app->hwnd);
  StretchDIBits(
    dc, 0, 0, app->winW, app->winH, 0, 0, app->winW, app->winH,
    app->framebuffer, &app->bmi, DIB_RGB_COLORS, SRCCOPY);
  ReleaseDC(app->hwnd, dc);
}

int main(void)
{
  App app;
  ZeroMemory(&app, sizeof(app));
  app.running = true;
  app.winW = GetSystemMetrics(SM_CXSCREEN);
  app.winH = GetSystemMetrics(SM_CYSCREEN);
  app.cam.z = 64.0f;
  app.cam.fov = 70.0f * 3.14159265f / 180.0f;

  if (!InitWindow(&app))
  {
    PrintText("Failed to create window.\n");
    return 1;
  }

  if (!MapData_Load(&app.map, "Temple"))
  {
    PrintText("Failed to load map.\n");
    return 1;
  }

  app.cam.x = app.map.w * 0.5f;
  app.cam.y = app.map.h * 0.5f;
  app.cam.z = 128.0f;

  LARGE_INTEGER freq;
  LARGE_INTEGER prev;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&prev);

  double fpsTimer = 0.0;
  int fpsFrames = 0;

  MSG msg;
  ZeroMemory(&msg, sizeof(msg));
  while (app.running)
  {
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (msg.message == WM_QUIT) app.running = false;
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    double dt = (double)(now.QuadPart - prev.QuadPart) / (double)freq.QuadPart;
    prev = now;

    int mdx = app.mouseDx;
    int mdy = app.mouseDy;
    app.mouseDx = 0;
    app.mouseDy = 0;

    if (app.focused)
    {
      float sens = 0.0025f;
      app.cam.yaw += (float)mdx * sens;
    }

    float move = 140.0f;
    float forwardX = cosf(app.cam.yaw);
    float forwardY = sinf(app.cam.yaw);
    float rightX = -forwardY;
    float rightY = forwardX;

    if (GetAsyncKeyState('W') & 0x8000)
    {
      app.cam.x += forwardX * move * (float)dt;
      app.cam.y += forwardY * move * (float)dt;
    }
    if (GetAsyncKeyState('S') & 0x8000)
    {
      app.cam.x -= forwardX * move * (float)dt;
      app.cam.y -= forwardY * move * (float)dt;
    }
    if (GetAsyncKeyState('A') & 0x8000)
    {
      app.cam.x -= rightX * move * (float)dt;
      app.cam.y -= rightY * move * (float)dt;
    }
    if (GetAsyncKeyState('D') & 0x8000)
    {
      app.cam.x += rightX * move * (float)dt;
      app.cam.y += rightY * move * (float)dt;
    }
    if (GetAsyncKeyState(VK_SPACE) & 0x8000) app.cam.z += 80.0f * (float)dt;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) app.cam.z -= 80.0f * (float)dt;
    if (GetAsyncKeyState('E') & 0x8000) app.cam.z += 80.0f * (float)dt;
    if (GetAsyncKeyState('X') & 0x8000) app.cam.z -= 80.0f * (float)dt;

    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) app.running = false;

    if (GetAsyncKeyState('1') & 0x8000) MapData_Load(&app.map, "Ice");
    if (GetAsyncKeyState('2') & 0x8000) MapData_Load(&app.map, "Hills");
    if (GetAsyncKeyState('3') & 0x8000) MapData_Load(&app.map, "Forest");
    if (GetAsyncKeyState('4') & 0x8000) MapData_Load(&app.map, "Temple");

    ClampCamera(&app);
    RenderVoxelSurf(&app);
    Present(&app);

    fpsTimer += dt;
    fpsFrames++;
    if (fpsTimer >= 1.0)
    {
      PrintFPS(fpsFrames);
      fpsFrames = 0;
      fpsTimer = 0.0;
    }
  }

  MapData_Clear(&app.map);
  free(app.framebuffer);
  return 0;
}
