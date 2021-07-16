#include <stdlib.h>
#include <stddef.h>

#include "icon.h"

static float
Power(float a, unsigned int b)
{
 float a0 = a;
 for(int i = 0;
     i < b - 1;
     i += 1)
 {
  a *= a0;
 }
 return a;
}

static Colour
ColourLerp(Colour a,
           Colour b,
           float t)
{
 Colour result;
 unsigned char t0 = 255 * t;
 if(0 == t0)
 {
  result = a;
 }
 else if(255 == t0)
 {
  result = b;
 }
 else
 {
  result.b = ((t0 * (b.b - a.b)) >> 8) + a.b;
  result.g = ((t0 * (b.g - a.g)) >> 8) + a.g;
  result.r = ((t0 * (b.r - a.r)) >> 8) + a.r;
 }
 return result;
}

typedef struct
{
 Pixel canvas_state[ScreenDimension_X * ScreenDimension_Y];
} UndoFrame;

typedef struct
{
 size_t max_frames;
 UndoFrame *frames;
 size_t top;
 size_t current;
} UndoStack;
static UndoStack global_undo_stack = {0};

static Pixel global_canvas[ScreenDimension_X * ScreenDimension_Y];

enum Settings
{
 ColourPicker_Margin = 12,
 ColourPicker_SizeMin = 50,
 ColourPicker_SizeMax = 400,
 
 BrushSize_Min = 2,
 BrushSize_Max = 48,
};

static Colour
RGBFromHSV(HSV hsv)
{
 Colour rgb;
 unsigned char region;
 unsigned char remainder;
 unsigned char p;
 unsigned char q;
 unsigned char t;
 
 if (hsv.s == 0)
 {
  rgb.r = hsv.v;
  rgb.g = hsv.v;
  rgb.b = hsv.v;
  return rgb;
 }
 
 region = hsv.h / 43;
 remainder = (hsv.h - (region * 43)) * 6; 
 
 p = (hsv.v * (255 - hsv.s)) >> 8;
 q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
 t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;
 
 switch (region)
 {
  case 0:
  rgb.r = hsv.v; rgb.g = t; rgb.b = p;
  break;
  case 1:
  rgb.r = q; rgb.g = hsv.v; rgb.b = p;
  break;
  case 2:
  rgb.r = p; rgb.g = hsv.v; rgb.b = t;
  break;
  case 3:
  rgb.r = p; rgb.g = q; rgb.b = hsv.v;
  break;
  case 4:
  rgb.r = t; rgb.g = p; rgb.b = hsv.v;
  break;
  default:
  rgb.r = hsv.v; rgb.g = p; rgb.b = q;
  break;
 }
 
 return rgb;
}

static HSV
HSVFromRGB(Colour rgb)
{
 HSV hsv;
 unsigned char rgb_min, rgb_max;
 
 rgb_min = rgb.r < rgb.g ? (rgb.r < rgb.b ? rgb.r : rgb.b) : (rgb.g < rgb.b ? rgb.g : rgb.b);
 rgb_max = rgb.r > rgb.g ? (rgb.r > rgb.b ? rgb.r : rgb.b) : (rgb.g > rgb.b ? rgb.g : rgb.b);
 
 hsv.v = rgb_max;
 if (hsv.v == 0)
 {
  hsv.h = 0;
  hsv.s = 0;
  return hsv;
 }
 
 hsv.s = 255 * (long)(rgb_max - rgb_min) / hsv.v;
 if (hsv.s == 0)
 {
  hsv.h = 0;
  return hsv;
 }
 
 if (rgb_max == rgb.r)
  hsv.h = 0 + 43 * (rgb.g - rgb.b) / (rgb_max - rgb_min);
 else if (rgb_max == rgb.g)
  hsv.h = 85 + 43 * (rgb.b - rgb.r) / (rgb_max - rgb_min);
 else
  hsv.h = 171 + 43 * (rgb.r - rgb.g) / (rgb_max - rgb_min);
 
 return hsv;
}

#define FgColour  64,  52,  46
#define BgColour 255, 255, 255
//#define BgColour 244, 239, 236

typedef struct
{
 int is_showing;
 int size;
 int hue;
 Colour recents[8];
} ColourPicker;
static ColourPicker global_colour_picker =
{
 .is_showing = 1,
 .size = 200,
 .hue = 0,
 .recents =
 {
  { 106,  97, 191 },
  { 112, 135, 208 },
  { 139, 203, 235 },
  { 140, 190, 163 },
  { 172, 129,  94 },
  { 173, 142, 180 },
  
  { FgColour },
  { BgColour },
 }
};

static Colour
ColourPicker_ColourFromPosition(int x, int y)
{
 Colour result = {0};
 
 if(ColourPicker_Margin <= x && x < global_colour_picker.size - ColourPicker_Margin)
 {
  if(ColourPicker_Margin <= y && y < global_colour_picker.size - ColourPicker_Margin)
  {
   float scale = 255.0f / (float)(global_colour_picker.size - ColourPicker_Margin * 2);
   result = RGBFromHSV(HSV(global_colour_picker.hue,
                           (x - ColourPicker_Margin) * scale,
                           (y - ColourPicker_Margin) * scale));
  }
  else if(global_colour_picker.size - ColourPicker_Margin <= y && y < global_colour_picker.size)
  {
   int cell_width = (global_colour_picker.size - ColourPicker_Margin * 2) / ArrayCount(global_colour_picker.recents);
   result = global_colour_picker.recents[(x - ColourPicker_Margin) / cell_width];
  }
 }
 
 return result;
}

typedef struct
{
 Colour colour;
 int radius;
 float hardness;
} BrushSettings;
static BrushSettings global_brush_settings =
{
 .colour = { FgColour },
 .radius = 3,
};


enum { BrushSizeIndicator_VisibleDuration = 8 };
typedef struct
{
 int visible_frames;
 int x;
 int y;
} BrushSizeIndicator;
static BrushSizeIndicator global_brush_size_indicator = {0};

static void
AppCallback_Render(Pixel *screen)
{
 // NOTE(tbt): render canvas
 memcpy(screen, global_canvas, sizeof(global_canvas));
 
 // NOTE(tbt): render colour picker
 if(global_colour_picker.is_showing)
 {
  for(int y = ColourPicker_Margin;
      y < global_colour_picker.size;
      y += 1)
  {
   for(int x = ColourPicker_Margin;
       x < global_colour_picker.size - ColourPicker_Margin;
       x += 1)
   {
    screen[x + y * ScreenDimension_X] = ColourPicker_ColourFromPosition(x, y);
   }
  }
  
  int brush_colour_preview_size = (global_colour_picker.size - ColourPicker_Margin * 2) / 3;
  for(int y = 0;
      y < brush_colour_preview_size;
      y += 1)
  {
   for(int x = 0;
       x < brush_colour_preview_size;
       x += 1)
   {
    int pixel_x = x + global_colour_picker.size;
    int pixel_y = y + ColourPicker_Margin;
    screen[pixel_x + pixel_y * ScreenDimension_X] = global_brush_settings.colour;
   }
  }
 }
 
 // NOTE(tbt): render brush size indicator
 if(global_brush_size_indicator.visible_frames >= 0)
 {
  int radius_squared = global_brush_settings.radius * global_brush_settings.radius;
  int outline_thickness_squared = radius_squared / 8;
  int half_outline_thickness = (outline_thickness_squared / 2.0f) + 0.5f;
  
  for(int y0 = -global_brush_settings.radius - half_outline_thickness;
      y0 < global_brush_settings.radius + half_outline_thickness;
      y0 += 1)
  {
   for(int x0 = -global_brush_settings.radius - half_outline_thickness;
       x0 < global_brush_settings.radius + half_outline_thickness;
       x0 += 1)
   {
    int pixel_x = global_brush_size_indicator.x + x0;
    int pixel_y = global_brush_size_indicator.y + y0;
    if(0 <= pixel_x && pixel_x < ScreenDimension_X &&
       0 <= pixel_y && pixel_y < ScreenDimension_Y)
    {
     int distance_squared = x0 * x0 + y0 * y0;
     if(distance_squared >= radius_squared - outline_thickness_squared &&
        distance_squared <= radius_squared + outline_thickness_squared)
     {
      Pixel *current = &screen[pixel_x + pixel_y * ScreenDimension_X];
      float alpha = ((float)global_brush_size_indicator.visible_frames / (float)BrushSizeIndicator_VisibleDuration);
      *current = ColourLerp(*current, global_brush_settings.colour, alpha);
     }
    }
   }
  }
 }
}

static int global_prev_x = 0;
static int global_prev_y = 0;

static void
AppCallback_Init(Pixel *screen)
{
 for(int pixel_index = 0;
     pixel_index < ScreenDimension_X * ScreenDimension_Y;
     pixel_index += 1)
 {
  global_canvas[pixel_index] = Colour(BgColour);
 }
 
 global_undo_stack.max_frames = 4096;
 global_undo_stack.frames = malloc(global_undo_stack.max_frames * sizeof(*global_undo_stack.frames));
 
 AppCallback_Render(screen);
}

static HitTestResult
HitTest(int x, int y)
{
 HitTestResult result = HitTestResult_Canvas;
 
 int resize_target_size = 4;
 
 if(global_colour_picker.is_showing)
 {
  if(y > global_colour_picker.size - resize_target_size &&
     y < global_colour_picker.size + resize_target_size &&
     x > (global_colour_picker.size - ColourPicker_Margin) - resize_target_size &&
     x < (global_colour_picker.size - ColourPicker_Margin) + resize_target_size)
  {
   result = HitTestResult_ColourPicker_Resize;
  }
  else if(y < global_colour_picker.size && x < global_colour_picker.size)
  {
   result = HitTestResult_ColourPicker;
  }
 }
 
 return result;
}

static HitTestResult
AppCallback_MouseDown(Pixel *screen,
                      int x, int y)
{
 HitTestResult hit_test_result = HitTest(x, y);
 
 if(HitTestResult_Canvas == hit_test_result)
 {
  global_prev_x = x;
  global_prev_y = y;
 }
 else if(HitTestResult_ColourPicker == hit_test_result)
 {
  global_brush_settings.colour = ColourPicker_ColourFromPosition(x, y);
  int is_already_in_recent_colours = 0;
  for(int colour_index = 0;
      colour_index < ArrayCount(global_colour_picker.recents);
      colour_index += 1)
  {
   if(global_colour_picker.recents[colour_index].b == global_brush_settings.colour.b &&
      global_colour_picker.recents[colour_index].g == global_brush_settings.colour.g &&
      global_colour_picker.recents[colour_index].r == global_brush_settings.colour.r)
   {
    is_already_in_recent_colours = 1;
    break;
   }
  }
  if(!is_already_in_recent_colours)
  {
   memcpy(&global_colour_picker.recents[1],
          &global_colour_picker.recents[0],
          sizeof(global_colour_picker.recents) - 3 * sizeof(global_colour_picker.recents[0]));
   global_colour_picker.recents[0] = global_brush_settings.colour;
  }
  AppCallback_Render(screen);
 }
 
 return hit_test_result;
}

static void
AppCallback_MouseMotion(Pixel *screen,
                        int x, int y,
                        float pressure,
                        InputState input_state,
                        int is_pen_input)
{
 if(InputState_None != input_state)
 {
  global_brush_size_indicator.x = x;
  global_brush_size_indicator.y = y;
 }
 
 if(!is_pen_input)
 {
  global_brush_size_indicator.visible_frames -= 1;
 }
 
 if(InputState_Drawing == input_state)
 {
  enum { MaxSteps = 128 };
  int steps;
  {
   int x_off = x - global_prev_x;
   int y_off = y - global_prev_y;
   
   steps = MinI(x_off * x_off + y_off * y_off, MaxSteps);
  }
  
  int from_x = MinI(x, global_prev_x) - global_brush_settings.radius;
  int from_y = MinI(y, global_prev_y) - global_brush_settings.radius;
  int dest_x = MaxI(x, global_prev_x) + global_brush_settings.radius;
  int dest_y = MaxI(y, global_prev_y) + global_brush_settings.radius;
  
  float positions[MaxSteps][2];
  for(int i = 0;
      i < steps;
      i += 1)
  {
   float t = (float)i / steps;
   positions[i][0] = global_prev_x * (1.0f - t) + x * t;
   positions[i][1] = global_prev_y * (1.0f - t) + y * t;
  }
  
  float pressure_adjusted_brush_radius = global_brush_settings.radius * pressure;
  pressure_adjusted_brush_radius *= pressure_adjusted_brush_radius;
  
  for(int y0 = from_y;
      y0 < dest_y;
      y0 += 1)
  {
   for(int x0 = from_x;
       x0 < dest_x;
       x0 += 1)
   {
    for(int i = 0;
        i < steps;
        i += 1)
    {
     if(0 <= x0 && x0 < ScreenDimension_X &&
        0 <= y0 && y0 < ScreenDimension_Y)
     {
      int x_offset = positions[i][0] - x0;
      int y_offset = positions[i][1] - y0;
      int distance_squared = x_offset * x_offset + y_offset * y_offset;
      
      if(distance_squared <= pressure_adjusted_brush_radius)
      {
       Pixel *current = &global_canvas[x0 + y0 * ScreenDimension_X];
       float alpha = 1.0f / distance_squared;
       *current = ColourLerp(*current, global_brush_settings.colour, alpha);
      }
     }
    }
   }
  }
 }
 else if(InputState_ResizingColourPicker == input_state)
 {
  int new_size = (x - ColourPicker_Margin + y) / 2;
  if(ColourPicker_SizeMin <= new_size && new_size < ColourPicker_SizeMax)
  {
   global_colour_picker.size = new_size;
  }
 }
 
 global_prev_x = x;
 global_prev_y = y;
 
 AppCallback_Render(screen);
}

static void
AppCallback_MouseUp(Pixel *screen,
                    int x, int y)
{
 if(global_undo_stack.top < global_undo_stack.max_frames)
 {
  global_undo_stack.current += 1;
  global_undo_stack.top = global_undo_stack.current;
  
  UndoFrame *undo_frame = &global_undo_stack.frames[global_undo_stack.current];
  memcpy(undo_frame->canvas_state, global_canvas, sizeof(undo_frame->canvas_state));
 }
}

static void
AppCallback_Scroll(Pixel *screen,
                   int x, int y,
                   float wheel_delta)
{
 HitTestResult hit_test_result = HitTest(x, y);
 
 if(HitTestResult_ColourPicker == hit_test_result)
 {
  global_colour_picker.hue += wheel_delta * 2;
  AppCallback_Render(screen);
 }
 else if(HitTestResult_Canvas == hit_test_result)
 {
  int new_radius = global_brush_settings.radius + wheel_delta * 2;
  if(BrushSize_Min <= new_radius && new_radius < BrushSize_Max)
  {
   global_brush_settings.radius = new_radius;
  }
  AppCallback_Render(screen);
  global_brush_size_indicator.visible_frames = BrushSizeIndicator_VisibleDuration;
  global_brush_size_indicator.x = x;
  global_brush_size_indicator.y = y;
 }
}

static void
AppCallback_Tab(Pixel *screen, int is_down)
{
 global_colour_picker.is_showing = !is_down;
 AppCallback_Render(screen);
}

static void
AppCallback_Undo(Pixel *screen)
{
 if(global_undo_stack.current > 1)
 {
  global_undo_stack.current -= 1;
  UndoFrame *undo_frame = &global_undo_stack.frames[global_undo_stack.current];
  memcpy(global_canvas, undo_frame->canvas_state, sizeof(undo_frame->canvas_state));
  AppCallback_Render(screen);
 }
}

static void
AppCallback_Redo(Pixel *screen)
{
 if(global_undo_stack.current < global_undo_stack.top)
 {
  global_undo_stack.current += 1;
  UndoFrame *undo_frame = &global_undo_stack.frames[global_undo_stack.current];
  memcpy(global_canvas, undo_frame->canvas_state, sizeof(undo_frame->canvas_state));
  AppCallback_Render(screen);
 }
}

static size_t
AppCallback_GetCanvasSize(void)
{
 return sizeof(Canvas) + (sizeof(Pixel) * ScreenDimension_X * ScreenDimension_Y);
}

static void
AppCallback_GetCanvas(Canvas *canvas)
{
 canvas->width = ScreenDimension_X;
 canvas->height= ScreenDimension_Y;
 memcpy(canvas->pixels, global_canvas, sizeof(global_canvas));
}

static void
AppCallback_WindowHidden(Pixel *screen)
{
 global_undo_stack.current = 0;
 // NOTE(tbt): reallocate to trick windows into unmapping from physical memory
 // TODO(tbt): stop using the stdlib and just reserve initially and commit as needed
 free(global_undo_stack.frames);
 global_undo_stack.frames = malloc(global_undo_stack.max_frames * sizeof(*global_undo_stack.frames));
 for(int pixel_index = 0;
     pixel_index < ScreenDimension_X * ScreenDimension_Y;
     pixel_index += 1)
 {
  global_canvas[pixel_index] = Colour(BgColour);
 }
 AppCallback_Render(screen);
}
