#include <stdlib.h>
#include <stddef.h>

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
RenderGUI(Pixel *screen)
{
 memcpy(screen, global_canvas, sizeof(global_canvas));
 
 // NOTE(tbt): draw debug gradient
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
}

static void
AppCallback_Init(Pixel *screen)
{
 memset(global_canvas, 255, sizeof(global_canvas));
 
 global_undo_stack.max_frames = 4096;
 global_undo_stack.frames = malloc(global_undo_stack.max_frames * sizeof(*global_undo_stack.frames));
 
 RenderGUI(screen);
}

static void
AppCallback_DrawBegin(Pixel *screen,
                      int x, int y)
{
 UndoFrame *undo_frame = &global_undo_stack.frames[global_undo_stack.current];
 memcpy(undo_frame->canvas_state, global_canvas, sizeof(undo_frame->canvas_state));
}

static void
AppCallback_DrawMovement(Pixel *screen,
                         int x, int y)
{
 static int brush_radius = 3;
 
 int x0, y0;
 for(y0 = -brush_radius;
     y0 <= brush_radius;
     y0 += 1)
 {
  for(x0 = -brush_radius;
      x0 <= brush_radius;
      x0 += 1)
  {
   int pixel_x = x + x0;
   int pixel_y = y + y0;
   if(x0 * x0 + y0 * y0 <= brush_radius * brush_radius &&
      pixel_x < ScreenDimension_X &&
      pixel_y < ScreenDimension_Y)
   {
    global_canvas[pixel_x + pixel_y * ScreenDimension_X] = Colour(0);
   }
  }
 }
 RenderGUI(screen);
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
  RenderGUI(screen);
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
  RenderGUI(screen);
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
