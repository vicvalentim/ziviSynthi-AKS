#ifndef ZIVI_FM1_DEVICE_APP_H
#define ZIVI_FM1_DEVICE_APP_H

#include "plat.h"

#define APP_NAME "ziviSynthi AKS"
#define APP_BLOCK 256u
#define APP_CANVAS_PX (240u * 240u)
#define APP_SPLASH

/* Keep the FM-1 panel calibration in fm1-base's standard slot. */
enum { OBJ_PANEL, OBJ_NOBJ };
#define APP_STORE_MAP {0xDC000u}

void app_splash(void);
void app_init(void);
void app_render(int32_t *lr, uint32_t frames, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *first, const char *second);

#endif
