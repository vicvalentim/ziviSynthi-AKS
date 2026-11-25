#ifndef ZIVI_FLASH_POLICY_H
#define ZIVI_FLASH_POLICY_H

#include <stdbool.h>
#include <stdint.h>

enum FlashTarget {
    FLASH_UNVERIFIED,
    FLASH_SMK37_PRO_REFERENCE
};

bool flash_test_span(enum FlashTarget target, uint32_t start, uint32_t length);

#endif
