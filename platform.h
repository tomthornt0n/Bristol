#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>

#define fallthrough  

#define _Stringify(_a) #_a
#define Stringify(_a) _Stringify(_a)

#define _WidenString(_a) L ## _a
#define WidenString(_a) _WidenString(_a)

#define WideStringify(_a) WidenString(Stringify(_a))

#define ArrayCount(_a) (sizeof(_a) / sizeof(_a[0]))

static inline int
MinI(int a, int b)
{
 if(a < b)
 {
  return a;
 }
 return b;
}

static inline int
MaxI(int a, int b)
{
 if(a > b)
 {
  return a;
 }
 return b;
}

enum ScreenDimension
{
 ScreenDimension_X = 1440,
 ScreenDimension_Y = 810,
};

struct Colour
{
 unsigned char b;
 unsigned char g;
 unsigned char r;
 unsigned char a;
};
typedef struct Colour Colour;
typedef struct Colour Pixel;
#define Colour(...) ((Colour){__VA_ARGS__})

typedef struct
{
 unsigned char h;
 unsigned char s;
 unsigned char v;
} HSV;
#define HSV(...) ((HSV){__VA_ARGS__})

typedef struct
{
 size_t width;
 size_t height;
 Pixel pixels[];
} Canvas;

typedef enum
{
 HitTestResult_Canvas,
 HitTestResult_ColourPicker,
 HitTestResult_ColourPicker_Resize,
} HitTestResult;

typedef enum
{
 InputState_None,
 InputState_Drawing,
 InputState_ResizingColourPicker,
} InputState;

#endif
