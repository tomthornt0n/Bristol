#include <windows.h>
#include <windowsx.h>

#include "platform.h"
#include "app.c"

static int w32_global_is_running = 1;

typedef struct
{
 BITMAPINFO bitmap_info;
 Pixel pixels[ScreenDimension_X * ScreenDimension_Y];
} W32GraphicsContext;
static W32GraphicsContext w32_global_graphics_context;

typedef enum
{
 HotKey_Show,
 
 HotKey_MAX,
} HotKey;

static void
W32HideWindow(HWND window_handle)
{
 ShowWindow(window_handle, SW_HIDE);
 AppCallback_Cleanup(w32_global_graphics_context.pixels);
}

static void
W32_RenderToWindow(HDC device_context_handle)
{
 StretchDIBits(device_context_handle,
               0, 0,
               ScreenDimension_X,
               ScreenDimension_Y,
               0, 0,
               ScreenDimension_X,
               ScreenDimension_Y,
               w32_global_graphics_context.pixels,
               &w32_global_graphics_context.bitmap_info,
               DIB_RGB_COLORS,
               SRCCOPY);
}

LRESULT CALLBACK
W32WindowMessageCallback(HWND window_handle,
                         UINT message,
                         WPARAM w_param,
                         LPARAM l_param)
{
 LRESULT result = 0;
 
 static int is_mouse_hover_active = 0;
 
 static InputState input_state = InputState_None;
 
 switch(message)
 {
  case(WM_HOTKEY):
  {
   switch(w_param)
   {
    case(HotKey_Show):
    {
     ShowWindow(window_handle, SW_NORMAL);
     SetActiveWindow(window_handle);
     SetFocus(window_handle);
     BringWindowToTop(window_handle);
     SetForegroundWindow(window_handle);
    } break;
    
    default:
    {
     result = DefWindowProc(window_handle, message, w_param, l_param);
    } break;
   }
  } break;
  
  case(WM_LBUTTONDOWN):
  {
   int x = GET_X_LPARAM(l_param);
   int y = GET_Y_LPARAM(l_param);
   HitTestResult hit_test_result = AppCallback_MouseDown(w32_global_graphics_context.pixels, x, y);
   if(hit_test_result == HitTestResult_Canvas)
   {
    input_state = InputState_Drawing;
   }
   else if(hit_test_result == HitTestResult_ColourPicker_Resize)
   {
    input_state = InputState_ResizingColourPicker;
   }
   else
   {
    input_state = InputState_None;
   }
   
   HDC device_context_handle = GetDC(window_handle);
   W32_RenderToWindow(device_context_handle);
   ReleaseDC(window_handle, device_context_handle);
  } break;
  
  // TODO(tbt): pointer input
  
  case(WM_MOUSELEAVE):
  {
   is_mouse_hover_active = 0;
  } fallthrough;
  case(WM_LBUTTONUP):
  {
   AppCallback_DrawEnd(w32_global_graphics_context.pixels,
                       GET_X_LPARAM(l_param),
                       GET_Y_LPARAM(l_param));
   input_state = 0;
  } break;
  
  case(WM_MOUSEWHEEL):
  {
   POINT mouse;
   GetCursorPos(&mouse);
   ScreenToClient(window_handle, &mouse);
   
   AppCallback_Scroll(w32_global_graphics_context.pixels,
                      mouse.x, mouse.y,
                      GET_WHEEL_DELTA_WPARAM(w_param) / 120.0f);
   HDC device_context_handle = GetDC(window_handle);
   W32_RenderToWindow(device_context_handle);
   ReleaseDC(window_handle, device_context_handle);
  } break;
  
  case(WM_MOUSEMOVE):
  {
   // NOTE(tbt): uggghhhhhh
   if(!is_mouse_hover_active)
   {
    is_mouse_hover_active = 1;
    TRACKMOUSEEVENT track_mouse_event = {0};
    {
     track_mouse_event.cbSize = sizeof(track_mouse_event);
     track_mouse_event.dwFlags = TME_LEAVE;
     track_mouse_event.hwndTrack = window_handle;
     track_mouse_event.dwHoverTime = HOVER_DEFAULT;
    }
    TrackMouseEvent(&track_mouse_event);
   }
   
   AppCallback_MouseMotion(w32_global_graphics_context.pixels,
                           GET_X_LPARAM(l_param),
                           GET_Y_LPARAM(l_param),
                           input_state);
   
   HDC device_context_handle = GetDC(window_handle);
   W32_RenderToWindow(device_context_handle);
   ReleaseDC(window_handle, device_context_handle);
  }
  
  case(WM_SETCURSOR):
  {
   if(InputState_ResizingColourPicker == input_state)
   {
    SetCursor(LoadCursorW(0, IDC_SIZENWSE));
   }
   else
   {
    SetCursor(LoadCursorW(0, IDC_ARROW));
   }
  } break;
  
  case(WM_PAINT):
  {
   PAINTSTRUCT ps;
   HDC device_context_handle = BeginPaint(window_handle, &ps);
   W32_RenderToWindow(device_context_handle);
   result = EndPaint(window_handle, &ps);
  } break;
  
  case(WM_KEYDOWN):
  case(WM_SYSKEYDOWN):
  case(WM_KEYUP):
  case(WM_SYSKEYUP):
  {
   int is_down = !(l_param & (1 << 31));
   
   if(is_down &&
      (VK_ESCAPE == w_param && (GetKeyState(VK_SHIFT) & 0x8000)) ||
      (VK_F4 == w_param && (GetKeyState(VK_MENU) & 0x8000)))
   {
    w32_global_is_running = 0;
    W32HideWindow(window_handle);
   }
   else if(is_down && VK_ESCAPE == w_param)
   {
    W32HideWindow(window_handle);
   }
   else if(VK_TAB == w_param)
   {
    AppCallback_Tab(w32_global_graphics_context.pixels, is_down);
    HDC device_context_handle = GetDC(window_handle);
    W32_RenderToWindow(device_context_handle);
    ReleaseDC(window_handle, device_context_handle);
   }
   else if(is_down && VK_RETURN == w_param)
   {
    if(OpenClipboard(window_handle))
    {
     EmptyClipboard();
     {
      Canvas *canvas = LocalAlloc(0, AppCallback_GetCanvasSize());
      AppCallback_GetCanvas(canvas);
      size_t canvas_size = canvas->width * canvas->height * sizeof(Pixel);
      
      HGLOBAL memory_handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT,
                                          sizeof(BITMAPINFO) + canvas_size);
      char *clipboard_data = (char *)GlobalLock(memory_handle);
      {
       BITMAPINFOHEADER *bih = &(((BITMAPINFO *)clipboard_data)->bmiHeader);
       bih->biSize = sizeof(BITMAPINFOHEADER);
       bih->biPlanes = 1;
       bih->biWidth = canvas->width;
       bih->biHeight = -canvas->height;
       bih->biBitCount = 32;
       bih->biCompression = BI_RGB;
       bih->biSizeImage = 0;
       bih->biClrUsed = 0;
       bih->biClrImportant = 0;
       
       Pixel *pixels = (Pixel *)(clipboard_data + sizeof(BITMAPINFO));
       memcpy(pixels, canvas->pixels, canvas_size);
      }
      GlobalUnlock(memory_handle);
      SetClipboardData(CF_DIB, memory_handle);
     }
     
     CloseClipboard();
    }
    
    ShowWindow(window_handle, SW_HIDE);
    W32HideWindow(window_handle);
   }
   
   if((GetKeyState(VK_CONTROL) & 0x8000) && is_down)
   {
    if('Z' == w_param)
    {
     AppCallback_Undo(w32_global_graphics_context.pixels);
     HDC device_context_handle = GetDC(window_handle);
     W32_RenderToWindow(device_context_handle);
     ReleaseDC(window_handle, device_context_handle);
    }
    else if ('Y' == w_param)
    {
     AppCallback_Redo(w32_global_graphics_context.pixels);
     HDC device_context_handle = GetDC(window_handle);
     W32_RenderToWindow(device_context_handle);
     ReleaseDC(window_handle, device_context_handle);
    }
   }
  } break;
  
  case(WM_DESTROY):
  case(WM_CLOSE):
  case(WM_QUIT):
  case(WM_KILLFOCUS):
  {
   W32HideWindow(window_handle);
  } break;
  
  default:
  {
   result = DefWindowProc(window_handle, message, w_param, l_param);
  } break;
 }
 
 return result;
}

#define ApplicationNameString WideStringify(APPLICATION_NAME)

int APIENTRY
wWinMain(HINSTANCE instance_handle,
         HINSTANCE previous_instance,
         LPWSTR command_line,
         int show_mode)
{
 HWND window_handle;
 
 //-NOTE(tbt): register window class
 {
  WNDCLASSEXW window_class = { sizeof(window_class) };
  window_class.lpfnWndProc = W32WindowMessageCallback;
  window_class.hInstance = instance_handle;
  window_class.lpszClassName = ApplicationNameString;
  RegisterClassEx(&window_class);
 }
 
 //-NOTE(tbt): create window
 {
  int screen_w = GetSystemMetrics(SM_CXSCREEN);
  int screen_h = GetSystemMetrics(SM_CYSCREEN);
  window_handle = CreateWindowW(ApplicationNameString,
                                ApplicationNameString,
                                WS_POPUPWINDOW,
                                (screen_w - ScreenDimension_X) / 2,
                                (screen_h - ScreenDimension_Y) / 2,
                                ScreenDimension_X, ScreenDimension_Y,
                                NULL, NULL,
                                instance_handle,
                                NULL);
 }
 
 //-NOTE(tbt): initialise bitmap
 {
  BITMAPINFOHEADER *bih = &w32_global_graphics_context.bitmap_info.bmiHeader;
  bih->biSize = sizeof(BITMAPINFOHEADER);
  bih->biPlanes = 1;
  bih->biWidth = ScreenDimension_X;
  bih->biHeight = -ScreenDimension_Y;
  bih->biBitCount = 32;
  bih->biCompression = BI_RGB;
  bih->biSizeImage = 0;
  bih->biClrUsed = 0;
  bih->biClrImportant = 0;
  
  memset(w32_global_graphics_context.pixels, 255, sizeof(w32_global_graphics_context.pixels));
 }
 
 //-NOTE(tbt): initialise app
 AppCallback_Init(w32_global_graphics_context.pixels);
 
 
 if(window_handle)
 {
  //-NOTE(tbt): hide window and register hot key
  ShowWindow(window_handle, SW_HIDE);
  if(RegisterHotKey(window_handle, HotKey_Show, MOD_NOREPEAT | MOD_WIN | MOD_SHIFT, 'D'))
  {
   MSG msg;
   while(GetMessageW(&msg, NULL, 0, 0))
   {
    //-NOTE(tbt): main message loop
    TranslateMessage(&msg);
    DispatchMessage(&msg);
    if(!w32_global_is_running)
    {
     break;
    }
    WaitMessage(); // NOTE(tbt): is this necessary when using GetMessage()?
   }
  }
  else
  {
   MessageBoxW(window_handle, L"Could not register hot-key", L"Error", MB_OK | MB_ICONERROR);
  }
 }
 else
 {
  MessageBoxW(window_handle, L"Error creating window", L"Error", MB_OK | MB_ICONERROR);
 }
 
 for(int hot_key = 0;
     hot_key < HotKey_MAX;
     hot_key += 1)
 {
  UnregisterHotKey(window_handle, hot_key);
 }
 
 return 0;
}