#include "doomgeneric.h"
#include "doomkeys.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <poll.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdio.h>
//#include <math.h>

struct fb_var_screeninfo fbinfo;
int fbfd;
int kefd;
int fb_size;

char key_b[KEY_MAX/8 + 1];

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} color;

color* fbdata;
color* tempbuf;

void DG_Init()
{
    fbfd = open("/dev/fb0", O_RDWR);
    kefd = open("/dev/input/by-path/platform-gpio_keys-event", O_RDONLY);
    if (fbfd < 0)
        return;
    
    ioctl(fbfd, FBIOGET_VSCREENINFO, &fbinfo);
    memset(key_b, 0, sizeof(key_b));
    
    fb_size = fbinfo.xres * fbinfo.yres * (fbinfo.bits_per_pixel / 8);

    fbdata = mmap(0, fb_size, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    tempbuf = malloc(fb_size);
}

inline color bri_to_col(uint8_t num)
{
    color col;
    col.r = num;
    col.g = num;
    col.b = num;
    //col.a = 255;
    return col;
}

inline int index_2d(int x, int y, int w)
{
    return x + y * w;
}

// https://tech-algorithm.com/articles/nearest-neighbor-image-scaling/

void resize_nearest(color* target, color* source, int w1, int h1, int w2, int h2)
{
    int x_ratio = (int)((w1<<16)/w2) + 1;
    int y_ratio = (int)((h1<<16)/h2) + 1;

    int x2, y2;
    for (size_t i = 0; i < h2; i++)
    {
        for (size_t j = 0; j < w2; j++)
        {
            x2 = ((j*x_ratio)>>16);
            y2 = ((i*y_ratio)>>16);
            color pixel = source[index_2d(x2, y2, w1)];

            pixel.g = (pixel.r + pixel.g + pixel.b) / 3;

            pixel.r = pixel.g;
            pixel.b = pixel.g;
            //pixel.a = 255;
            target[index_2d(j, i, w2)] = pixel;
        }
        
    }
    
}

void dither(color* target, int w, int h)
{
    for (size_t i = 0; i < h - 1; i++)
    {
        for (size_t j = 0; j < w - 1; j++)
        {
            color pixel = target[index_2d(j, i, w)];

            //uint8_t quPixel = (uint8_t)(round((float)(pixel.g / 64.0f))) * 64;
            uint8_t quPixel = (pixel.g >> 6) << 6;

            target[index_2d(j, i, w)] = bri_to_col(quPixel);

            int16_t qErr = pixel.g - quPixel;

            int index = index_2d(j+1, i, w);
            uint8_t val = target[index].g + (qErr >> 2);// * (7.0f/16.0f);
            target[index] = bri_to_col(val);
            
            index = index_2d(j-1, i+1, w);
            val = target[index].g + qErr / 5;// * (3.0f/16.0f);
            target[index] = bri_to_col(val);

            index = index_2d(j, i+1, w);
            val = target[index].g + qErr / 3;// * (5.0f/16.0f);
            target[index] = bri_to_col(val);

            index = index_2d(j+1, i+1, w);
            val = target[index].g + (qErr >> 4);// * (1.0f/16.0f);
            target[index] = bri_to_col(val);
        }
    }
}

void DG_DrawFrame()
{
    resize_nearest(tempbuf, (color*)DG_ScreenBuffer, DOOMGENERIC_RESX, DOOMGENERIC_RESY, fbinfo.xres, fbinfo.yres);
    dither(tempbuf, fbinfo.xres, fbinfo.yres);
    memcpy(fbdata, tempbuf, fb_size);
}

void DG_SleepMs(uint32_t ms)
{
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs()
{
    struct timeval  tp;
    struct timezone tzp;

    gettimeofday(&tp, &tzp);

    return (tp.tv_sec * 1000) + (tp.tv_usec / 1000);
}

inline char is_key(int key)
{
    return !!(key_b[key/8] & (1<<(key % 8)));
}

char prevKeys[8];

int DG_GetKey(int* pressed, unsigned char* key)
{
    ioctl(kefd, EVIOCGKEY(sizeof(key_b)), key_b);
    
    char keyp = is_key(KEY_UP);
    if (keyp != prevKeys[0])
    {
        *pressed = keyp;
        *key = KEY_UPARROW;
        prevKeys[0] = keyp;
        return 1;
    }
    if (keyp != prevKeys[7])
    {
        *pressed = keyp;
        *key = KEY_USE;
        prevKeys[7] = keyp;
        return 1;
    }
    keyp = is_key(KEY_DOWN);
    if (keyp != prevKeys[1])
    {
        *pressed = keyp;
        *key = KEY_DOWNARROW;
        prevKeys[1] = keyp;
        return 1;
    }
    keyp = is_key(KEY_LEFT);
    if (keyp != prevKeys[2])
    {
        *pressed = keyp;
        *key = KEY_LEFTARROW;
        prevKeys[2] = keyp;
        return 1;
    }
    keyp = is_key(KEY_RIGHT);
    if (keyp != prevKeys[3])
    {
        *pressed = keyp;
        *key = KEY_RIGHTARROW;
        prevKeys[3] = keyp;
        return 1;
    }
    keyp = is_key(KEY_ENTER);
    if (keyp != prevKeys[4])
    {
        *pressed = keyp;
        *key = 13;
        prevKeys[4] = keyp;
        return 1;
    }
    if (keyp != prevKeys[6])
    {
        *pressed = keyp;
        *key = KEY_FIRE;
        prevKeys[6] = keyp;
        return 1;
    }
    keyp = is_key(KEY_BACKSPACE);
    if (keyp != prevKeys[5])
    {
        *pressed = keyp;
        *key = KEY_ESCAPE;
        prevKeys[5] = keyp;
        return 1;
    }
    

    return 0;
}

void DG_SetWindowTitle(const char* title)
{
    // No window... printing to terminal
    printf("Title: %s\n", title);
}

int main(int argc, char **argv)
{
    doomgeneric_Create(argc, argv);

    while(1)
    {
        doomgeneric_Tick();
    }

    munmap(fbdata, fb_size);
    close(fbfd);
    close(kefd);
    free(tempbuf);

    return 0;
}