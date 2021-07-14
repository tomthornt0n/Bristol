#include <windows.h>
#include <windowsx.h>

#include "platform.h"
#include "app.c"

#define ApplicationNameString WideStringify(APPLICATION_NAME)

typedef struct
{
 BITMAPINFO bitmap_info;
 Pixel pixels[ScreenDimension_X * ScreenDimension_Y];
} W32GraphicsContext;
static W32GraphicsContext w32_global_graphics_context;

typedef enum
{
 NotificationIconID_Main,
 NotificationIconID_MAX,
} NotificationIconID;

typedef enum
{
 HotKey_Show,
 HotKey_MAX,
} HotKey;

NOTIFYICONDATAW w32_global_notify_icon_data =
{
 .uVersion = NOTIFYICON_VERSION_4,
 .cbSize = sizeof(w32_global_notify_icon_data),
 .uID = NotificationIconID_Main,
 .uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP,
 .uCallbackMessage = WM_USER + NotificationIconID_Main,
 .szTip = ApplicationNameString,
};

static void
W32ShowWindow(HWND window_handle)
{
 Shell_NotifyIconW(NIM_DELETE, &w32_global_notify_icon_data);
 
 ShowWindow(window_handle, SW_NORMAL);
 SetActiveWindow(window_handle);
 SetFocus(window_handle);
 BringWindowToTop(window_handle);
 SetForegroundWindow(window_handle);
}

static HICON global_icon_handle;

static void
W32HideWindow(HWND window_handle)
{
 w32_global_notify_icon_data.hWnd = window_handle;
 w32_global_notify_icon_data.hIcon = global_icon_handle;
 Shell_NotifyIconW(NIM_ADD, &w32_global_notify_icon_data);
 Shell_NotifyIconW(NIM_SETVERSION, &w32_global_notify_icon_data);
 ShowWindow(window_handle, SW_HIDE);
 AppCallback_WindowHidden(w32_global_graphics_context.pixels);
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
     W32ShowWindow(window_handle);
    } break;
    
    default:
    {
     result = DefWindowProc(window_handle, message, w_param, l_param);
    } break;
   }
  } break;
  
  case(WM_POINTERDOWN):
  case(WM_NCPOINTERDOWN):
  case(WM_LBUTTONDOWN):
  {
   POINT position =
   {
    .x = GET_X_LPARAM(l_param),
    .y = GET_Y_LPARAM(l_param),
   };
   
   if(WM_LBUTTONDOWN != message)
   {
    ScreenToClient(window_handle, &position);
   }
   
   HitTestResult hit_test_result = AppCallback_MouseDown(w32_global_graphics_context.pixels, position.x, position.y);
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
  
  
  case(WM_POINTERDEVICEINRANGE):
  {
  } break;
  
  case(WM_MOUSELEAVE):
  {
   is_mouse_hover_active = 0;
  } fallthrough;
  case(WM_POINTERUP):
  case(WM_POINTERENTER):
  case(WM_POINTERLEAVE):
  case(WM_POINTERDEVICEOUTOFRANGE):
  case(WM_NCPOINTERUP):
  case(WM_LBUTTONUP):
  {
   AppCallback_MouseUp(w32_global_graphics_context.pixels,
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
  
  case(WM_POINTERUPDATE):
  case(WM_NCPOINTERUPDATE):
  case(WM_MOUSEMOVE):
  {
   // NOTE(tbt): uggghhhhhh
   if(WM_MOUSEMOVE == message && !is_mouse_hover_active)
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
   
   POINT position =
   {
    .x = GET_X_LPARAM(l_param),
    .y = GET_Y_LPARAM(l_param),
   };
   
   float pressure = 1.0f;
   
   if(WM_MOUSEMOVE != message)
   {
    ScreenToClient(window_handle, &position);
    
    POINTER_PEN_INFO pen_info;
    if(GetPointerPenInfo(GET_POINTERID_WPARAM(w_param), &pen_info))
    {
     if(0 < pen_info.pressure)
     {
      pressure = (float)pen_info.pressure / 1024.0f;
     }
    }
   }
   
   AppCallback_MouseMotion(w32_global_graphics_context.pixels,
                           position.x, position.y,
                           pressure,
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
    PostQuitMessage(0);
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
       
       LocalFree(canvas);
      }
      GlobalUnlock(memory_handle);
      SetClipboardData(CF_DIB, memory_handle);
     }
     
     CloseClipboard();
    }
    
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
  {
   W32HideWindow(window_handle);
  } break;
  
  case(WM_USER + NotificationIconID_Main):
  {
   if(WM_LBUTTONDOWN == LOWORD(l_param))
   {
    W32ShowWindow(window_handle);
   }
   else if(WM_RBUTTONDOWN == LOWORD(l_param))
   {
    enum
    {
     MenuItem_NONE,
     MenuItem_Show,
     MenuItem_Quit,
    } menu_item;
    
    HMENU menu_handle = CreatePopupMenu();
    {
     AppendMenuW(menu_handle, MF_STRING, MenuItem_Show, L"S&how");
     AppendMenuW(menu_handle, MF_STRING, MenuItem_Quit, L"Q&uit");
    }
    
    menu_item = TrackPopupMenu(menu_handle,
                               TPM_LEFTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                               GET_X_LPARAM(w_param),
                               GET_Y_LPARAM(w_param),
                               0, window_handle, NULL);
    DestroyMenu(menu_handle);
    if(MenuItem_Show == menu_item)
    {
     W32ShowWindow(window_handle);
    }
    else if (MenuItem_Quit == menu_item)
    {
     W32HideWindow(window_handle);
     PostQuitMessage(0);
    }
   }
  } break;
  
  default:
  {
   result = DefWindowProc(window_handle, message, w_param, l_param);
  } break;
 }
 
 return result;
}

int APIENTRY
wWinMain(HINSTANCE instance_handle,
         HINSTANCE previous_instance,
         LPWSTR command_line,
         int show_mode)
{
 HWND window_handle;
 
 //-NOTE(tbt): load icon
 {
  HICON icon = NULL;
  HDC dc = GetDC(NULL);
  
  union
  {
   BITMAPV5HEADER v5_header;
   BITMAPINFO info;
  } bitmap = {0};
  bitmap.v5_header.bV5Size = sizeof(bitmap.v5_header);
  bitmap.v5_header.bV5Width = global_icon.width;
  bitmap.v5_header.bV5Height = -global_icon.height;
  bitmap.v5_header.bV5Planes = 1;
  bitmap.v5_header.bV5BitCount = 32;
  bitmap.v5_header.bV5Compression = BI_BITFIELDS;
  bitmap.v5_header.bV5RedMask = 0x00ff0000;
  bitmap.v5_header.bV5GreenMask = 0x0000ff00;
  bitmap.v5_header.bV5BlueMask = 0x000000ff;
  bitmap.v5_header.bV5AlphaMask = 0xff000000;
  
  unsigned char *target;
  HBITMAP colour = CreateDIBSection(dc,
                                    &bitmap.info,
                                    DIB_RGB_COLORS,
                                    (void **)&target,
                                    NULL, 0);
  
  ReleaseDC(NULL, dc);
  
  if (colour)
  {
   HBITMAP mask = CreateBitmap(global_icon.width, global_icon.height, 1, 1, NULL);
   
   if (mask)
   {
    memcpy(target, global_icon.pixels, global_icon.width * global_icon.height * 4);
    
    ICONINFO icon_info = {0};
    icon_info.fIcon = 1;
    icon_info.xHotspot = global_icon.width / 2;
    icon_info.yHotspot = global_icon.height / 2;
    icon_info.hbmMask = mask;
    icon_info.hbmColor = colour;
    
    icon = CreateIconIndirect(&icon_info);
    
    DeleteObject(colour);
    DeleteObject(mask);
   }
  }
  
  global_icon_handle = icon;
 }
 
 //-NOTE(tbt): register window class
 {
  WNDCLASSEXW window_class = { sizeof(window_class) };
  window_class.lpfnWndProc = W32WindowMessageCallback;
  window_class.hInstance = instance_handle;
  window_class.hIcon = global_icon_handle;
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
  W32HideWindow(window_handle);
  if(RegisterHotKey(window_handle, HotKey_Show, MOD_NOREPEAT | MOD_WIN | MOD_SHIFT, 'D'))
  {
   //-NOTE(tbt): main message loop
   MSG msg;
   while(GetMessageW(&msg, NULL, 0, 0))
   {
    if(WM_QUIT == msg.message)
    {
     break;
    }
    else
    {
     TranslateMessage(&msg);
     DispatchMessage(&msg);
    }
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
 
 Shell_NotifyIconW(NIM_DELETE, &w32_global_notify_icon_data);
 
 return 0;
}
