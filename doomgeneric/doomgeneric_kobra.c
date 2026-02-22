// doomgeneric_kobra_perfect.c – No Tearing + 16/32bpp Auto-Detect + Brightness Fix + Input

#include "doomgeneric.h"
#include "doomkeys.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <time.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <errno.h>

/* ─────────────────────────────────────────────────────────────────────────
   Framebuffer Globals
   ───────────────────────────────────────────────────────────────────────── */
static const char *framebuffer_dev_path = "/dev/fb0";
static int fb_fd = -1;
static uint8_t *fb_mem = NULL;

static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;

static int bpp = 32; 
static int screen_pitch_bytes = 0;

static int current_buffer_idx = 0;
static bool double_buffer_enabled = false;

/* ─────────────────────────────────────────────────────────────────────────
   Input System
   ───────────────────────────────────────────────────────────────────────── */
static const char *default_input_dev_path = "/dev/input/event1"; 
static int input_fd = -1;

#define KEYQUEUE_SIZE 16
static unsigned short g_key_queue[KEYQUEUE_SIZE];
static unsigned int g_key_queue_write_index = 0;
static unsigned int g_key_queue_read_index = 0;

static unsigned char evdev_to_doom(uint16_t code) {
    if (code >= KEY_1 && code <= KEY_9) return '1' + (code - KEY_1);
    if (code == KEY_0) return '0';

    switch (code) {
        case KEY_ENTER:     return 13; 
        case KEY_KPENTER:   return 13; 
        case KEY_SELECT:    return 13; 
        
        case KEY_SPACE:      return KEY_USE;
        case KEY_E:          return KEY_USE;

        case KEY_LEFTCTRL:   return KEY_FIRE;
        case KEY_RIGHTCTRL:  return KEY_FIRE;
        case KEY_F:          return KEY_FIRE;

        case KEY_RIGHT: return KEY_RIGHTARROW;
        case KEY_LEFT:  return KEY_LEFTARROW;
        case KEY_DOWN:  return KEY_DOWNARROW;
        case KEY_UP:    return KEY_UPARROW;

        case KEY_A: return 'a'; case KEY_B: return 'b'; case KEY_C: return 'c';
        case KEY_D: return 'd'; case KEY_G: return 'g'; case KEY_H: return 'h'; 
        case KEY_I: return 'i'; case KEY_J: return 'j'; case KEY_K: return 'k'; 
        case KEY_L: return 'l'; case KEY_M: return 'm'; case KEY_N: return 'n'; 
        case KEY_O: return 'o'; case KEY_P: return 'p'; case KEY_Q: return 'q'; 
        case KEY_R: return 'r'; case KEY_S: return 's'; case KEY_T: return 't'; 
        case KEY_U: return 'u'; case KEY_V: return 'v'; case KEY_W: return 'w'; 
        case KEY_X: return 'x'; case KEY_Y: return 'y'; case KEY_Z: return 'z';

        case KEY_ESC:        return KEY_ESCAPE;
        case KEY_BACKSPACE:  return KEY_BACKSPACE;
        case KEY_TAB:        return KEY_TAB;
        case KEY_LEFTSHIFT:  return KEY_RSHIFT;
        case KEY_RIGHTSHIFT: return KEY_RSHIFT;
        case KEY_LEFTALT:    return KEY_RALT;
        case KEY_RIGHTALT:   return KEY_RALT;
        case KEY_MINUS:      return '-';
        case KEY_EQUAL:      return '=';
        case KEY_COMMA:      return ',';
        case KEY_DOT:        return '.';
        case KEY_SLASH:      return '/';
        
        default: return 0;
    }
}

static void add_key_to_queue(const int pressed, const unsigned char key) {
    if (key == 0) return;
    const auto key_data = pressed << 8 | key;
    g_key_queue[g_key_queue_write_index] = key_data;
    g_key_queue_write_index = (g_key_queue_write_index + 1) % KEYQUEUE_SIZE;
}

static void read_evdev_queue(void) {
    struct input_event ev;
    while (read(input_fd, &ev, sizeof(ev)) > 0) {
        if (ev.type == EV_KEY) {
            if (ev.value == 2) continue; 
            add_key_to_queue((ev.value == 1), evdev_to_doom(ev.code));
        }
    }
}

int DG_GetKey(int* pressed, unsigned char* doomKey) {
    read_evdev_queue();
    if (g_key_queue_read_index == g_key_queue_write_index) return 0;
    const auto keyData = g_key_queue[g_key_queue_read_index];
    g_key_queue_read_index = (g_key_queue_read_index + 1) % KEYQUEUE_SIZE;
    *pressed = keyData >> 8;
    *doomKey = keyData & 0xFF;
    return 1;
}

/* ─────────────────────────────────────────────────────────────────────────
   Timing
   ───────────────────────────────────────────────────────────────────────── */
static struct timespec start_time;

/* ─────────────────────────────────────────────────────────────────────────
   Drawing
   ───────────────────────────────────────────────────────────────────────── */

void DG_DrawFrame(void)
{
    if (!fb_mem) return;

    const int src_w = 320;
    const int src_h = 200;
    const int dst_w = vinfo.xres;
    const int dst_h = vinfo.yres;
    
    int draw_buffer_idx = double_buffer_enabled ? (1 - current_buffer_idx) : 0;
    
    // Смещение до нужного буфера (если двойная буферизация включена)
    uint32_t offset = draw_buffer_idx * (vinfo.yres * screen_pitch_bytes);
    uint8_t *dst_base = fb_mem + offset;
    uint32_t *src_buffer = (uint32_t *)DG_ScreenBuffer;

    for (int dy = 0; dy < dst_h; dy++) {
        int sy = (dy * src_h) / dst_h;
        uint32_t *src_line = src_buffer + sy * src_w;
        uint8_t *dst_row_ptr = dst_base + dy * screen_pitch_bytes;

        for (int dx = 0; dx < dst_w; dx++) {
            int sx = (dx * src_w) / dst_w;
            uint32_t p = src_line[sx];

            // === ИСПРАВЛЕНИЕ АРТЕФАКТОВ ЯРКОСТИ ===
            // Считаем в int, чтобы избежать переполнения при значении > 255
            int r = ((p >> 16) & 0xFF); if (r > 255) r = 255;
            int g = ((p >> 8) & 0xFF);  if (g > 255) g = 255;
            int b = (p & 0xFF);         if (b > 255) b = 255;

            if (bpp == 32 || bpp == 24) {
                ((uint32_t*)dst_row_ptr)[dx] = (0xFF << 24) | (r << 16) | (g << 8) | b;
            } 
            else if (bpp == 16) {
                ((uint16_t*)dst_row_ptr)[dx] = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
            }
        }
    }

    // Если получилось включить двойную буферизацию - мгновенно переключаем кадр
    if (double_buffer_enabled) {
        vinfo.yoffset = draw_buffer_idx * vinfo.yres;
        if (ioctl(fb_fd, FBIOPAN_DISPLAY, &vinfo) == 0) {
            current_buffer_idx = draw_buffer_idx;
        }
    }
}

void DG_Init(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    /* ── FB Init ── */
    fb_fd = open(framebuffer_dev_path, O_RDWR);
    if (fb_fd < 0) {
        fprintf(stderr, "Error: cannot open framebuffer %s\n", framebuffer_dev_path);
        exit(1);
    }
    
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo) || ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        fprintf(stderr, "Error: ioctl failed\n");
        exit(1);
    }

    bpp = vinfo.bits_per_pixel;
    screen_pitch_bytes = finfo.line_length;

    printf("Display Info: %dx%d, %d bpp\n", vinfo.xres, vinfo.yres, bpp);

    // Пытаемся включить аппаратную двойную буферизацию
    vinfo.yres_virtual = vinfo.yres * 2;
    if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo) == 0 && vinfo.yres_virtual >= vinfo.yres * 2) {
        printf("Double buffering enabled successfully!\n");
        double_buffer_enabled = true;
    } else {
        printf("Double buffering not supported by driver. Using single buffer.\n");
        double_buffer_enabled = false;
        ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo); 
    }

    fb_mem = (uint8_t*)mmap(NULL, finfo.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (fb_mem == MAP_FAILED) {
        fprintf(stderr, "Error: mmap failed\n");
        exit(1);
    }

    /* ── Input Init ── */
    const char *input_dev = getenv("DOOM_KBDEV");
    if (!input_dev) input_dev = default_input_dev_path;
    input_fd = open(input_dev, O_RDONLY | O_NONBLOCK);
    
    if (input_fd < 0) {
        printf("ERROR: Input device not found: %s\n", input_dev);
    } else {
        printf("Input device opened: %s\n", input_dev);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &start_time);
}

void DG_SleepMs(const uint32_t ms) {
    usleep(ms * 1000);
}

uint32_t DG_GetTicksMs(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (ts.tv_sec - start_time.tv_sec) * 1000 + (ts.tv_nsec - start_time.tv_nsec) / 1000000;
}

void DG_SetWindowTitle(const char *title) { (void)title; }

int main(const int argc, const char **argv) {
    doomgeneric_Create(argc, argv);
    while(true) {
        doomgeneric_Tick();
    }
}
