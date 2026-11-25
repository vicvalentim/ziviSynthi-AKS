#ifndef ZIVI_FELUCCA_APP_H
#define ZIVI_FELUCCA_APP_H

#include "plat.h"
#include "dsp_config.h"

#if !PLAT_SMK37 || PLAT_SMK37_MODEL != 1
#error "This candidate targets the SMK-37 Pro."
#endif

#define APP_NAME "ziviSynthi AKS"
#define APP_BLOCK 128u
#define FACTORY_PROGRAMS 128u
#define APP_GAIN(position) (position)
#define APP_CANVAS_PX (240u * 240u)
#define APP_SPLASH
#define APP_BLE_AT_BOOT 0
#ifdef PLAT_HOST
#define APP_HOST_EXPECT
#endif
#define APP_SYSEX

enum { OBJ_PANEL, OBJ_BLE, OBJ_USER_1, OBJ_USER_2, OBJ_USER_3, OBJ_NOBJ };
#define APP_USER_SLOTS 3u
#define APP_USER_STORE_LO 0xEE000u
#define APP_USER_STORE_HI 0xF4000u
#define APP_USER_TAG "ZS38"
#define APP_STORE_MAP {0xDC000u, 0xDE000u, 0xEE000u, 0xF0000u, 0xF2000u}

void app_init(void);
void app_render(int32_t *lr, uint32_t frames, uint32_t master_q12);
void app_limit_master(int32_t *lr, uint32_t master_q12);
int app_usb_matrix_active(void);
int32_t *app_usb_input_buffer(void);
void app_usb_input_ready(void);
#ifndef PLAT_HOST
void app_usb_platform_fence(void);
#endif
int app_led_flush_ready(void);
int app_master_turn(int delta);
void app_panic(void);
void app_splash(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *first, const char *second);
int app_expect(const char *what, int *got);
int app_sysex(const uint8_t *data, uint32_t size);

#endif
