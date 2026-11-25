#include "flash_policy.h"

enum {
    FLASH_SIZE = 0x100000,
    ERASE_SIZE = 0x1000,
    CONFIG_FIRST = 0x9D000,
    CONFIG_END = 0xAF000
};

_Static_assert(CONFIG_FIRST % ERASE_SIZE == 0 && CONFIG_END % ERASE_SIZE == 0,
               "A janela de teste deve abranger somente setores inteiros");
_Static_assert(CONFIG_FIRST >= 0x04000 && CONFIG_END <= 0xC0000
               && CONFIG_END <= FLASH_SIZE && CONFIG_END > CONFIG_FIRST,
               "A janela de teste deve permanecer na área documentada de configuração");

bool flash_test_span(enum FlashTarget target, uint32_t start, uint32_t length)
{
    /* Modelo de teste da janela de configurações do port kit.
       A integração real deve falhar por padrão até validar variante e mapa. */
    return target == FLASH_SMK37_PRO_REFERENCE
        && length > 0
        && start % ERASE_SIZE == 0
        && length % ERASE_SIZE == 0
        && start >= CONFIG_FIRST && start < CONFIG_END
        && length <= (uint32_t)CONFIG_END - start;
}
