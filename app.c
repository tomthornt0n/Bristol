#include <stdlib.h>
#include <stddef.h>

static float
Power(float a, unsigned int b)
{
 float a0 = a;
 for(int i = 0;
     i < b;
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
 result.b = ((t0 * (b.b - a.b)) >> 8) + a.b;
 result.g = ((t0 * (b.g - a.g)) >> 8) + a.g;
 result.r = ((t0 * (b.r - a.r)) >> 8) + a.r;
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
 struct UndoStack_ToClearOnCleanup
 {
  size_t top;
  size_t current;
 };
} UndoStack;
static UndoStack global_undo_stack = {0};

static Pixel global_canvas[ScreenDimension_X * ScreenDimension_Y];

static void
RenderApp(Pixel *screen)
{
 memcpy(screen, global_canvas, sizeof(global_canvas));
 
#define DrawDebugGradient 0
#if DrawDebugGradient
 for(int x = 0;
     x < 64;
     x += 1)
 {
  for(int y = 0;
      y < 64;
      y+= 1)
  {
   screen[x + y * ScreenDimension_X] = Colour(4 * x, 255, 4 * y);
  }
 }
#endif
#undef DrawDebugGradient
}

static int global_prev_x = 0;
static int global_prev_y = 0;

static void
AppCallback_Init(Pixel *screen)
{
 memset(global_canvas, 255, sizeof(global_canvas));
 
 global_undo_stack.max_frames = 4096;
 global_undo_stack.frames = malloc(global_undo_stack.max_frames * sizeof(*global_undo_stack.frames));
 
 RenderApp(screen);
}

typedef struct
{
 Colour colour;
 int radius;
 int hardness;
} BrushSettings;
static BrushSettings global_brush_settings =
{
 .colour = {0},
 .radius = 3,
 .hardness = 2,
};

static void
AppCallback_DrawBegin(Pixel *screen,
                      int x, int y)
{
 UndoFrame *undo_frame = &global_undo_stack.frames[global_undo_stack.current];
 memcpy(undo_frame->canvas_state, global_canvas, sizeof(undo_frame->canvas_state));
 
 global_prev_x = x;
 global_prev_y = y;
}

static void
AppCallback_DrawMovement(Pixel *screen,
                         int x, int y)
{
 int x_off = x - global_prev_x;
 int y_off = y - global_prev_y;
 
 int steps = x_off * x_off + y_off * y_off;
 
 for(int i = 0;
     i < steps;
     i += 1)
 {
  float t = (float)i / steps;
  float x_f = global_prev_x * (1.0f - t) + x * t;
  float y_f = global_prev_y * (1.0f - t) + y * t;
  
  int x0, y0;
  for(y0 = -global_brush_settings.radius;
      y0 <= global_brush_settings.radius;
      y0 += 1)
  {
   for(x0 = -global_brush_settings.radius;
       x0 <= global_brush_settings.radius;
       x0 += 1)
   {
    int pixel_x = x_f + x0;
    int pixel_y = y_f + y0;
    int dist_squared = x0 * x0 + y0 * y0;
    if(dist_squared <= global_brush_settings.radius * global_brush_settings.radius &&
       pixel_x < ScreenDimension_X &&
       pixel_y < ScreenDimension_Y)
    {
     float alpha = 1.0f / Power(dist_squared, global_brush_settings.hardness);
     Pixel *current = &global_canvas[pixel_x + pixel_y * ScreenDimension_X];
     *current = ColourLerp(*current, global_brush_settings.colour, alpha);
    }
   }
  }
 }
 
 global_prev_x = x;
 global_prev_y = y;
 
 RenderApp(screen);
}

static void
AppCallback_DrawEnd(Pixel *screen,
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
AppCallback_Undo(Pixel *screen)
{
 if(global_undo_stack.current > 0)
 {
  global_undo_stack.current -= 1;
  UndoFrame *undo_frame = &global_undo_stack.frames[global_undo_stack.current];
  memcpy(global_canvas, undo_frame->canvas_state, sizeof(undo_frame->canvas_state));
  RenderApp(screen);
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
  RenderApp(screen);
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
AppCallback_Cleanup(void)
{
 memset(&global_undo_stack.top, 0, sizeof(struct UndoStack_ToClearOnCleanup));
 memset(&global_canvas, 255, sizeof(global_canvas));;
}
