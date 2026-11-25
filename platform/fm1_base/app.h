#ifndef ZIVI_FM1BASE_APP_H
#define ZIVI_FM1BASE_APP_H

#ifndef PLAT_HOST
#error "Este alvo foi validado exclusivamente como simulador fm1-base."
#endif

#include "plat.h"

#define APP_NAME "ziviSynthiA SMK"
#define APP_BLOCK 256u
#define APP_CANVAS_PX (240u * 240u)
#define APP_SPLASH
#define APP_HOST_EXPECT

enum { OBJ_NOBJ = 1 };  /* marcador exigido pelo simulador, sem escrita pela aplicação */
#define APP_STORE_MAP {0u}

void app_splash(void);
void app_init(void);
void app_render(int32_t *lr, uint32_t frames, uint32_t master_q12);
void app_panic(void);
void app_ui_init(void);
void app_frame(void);
void app_input_only(void);
void app_say(const char *first, const char *second);
int app_expect(const char *what, int *got);

#endif
