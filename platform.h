#ifndef PLATFORM_H
#define PLATFORM_H

#include <stddef.h>

#define fallthrough  

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
 size_t width;
 size_t height;
 Pixel pixels[];
} Canvas;

#endif
