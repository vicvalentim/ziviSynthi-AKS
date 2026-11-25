#ifndef ZIVI_AKS_CALIBRATION_H
#define ZIVI_AKS_CALIBRATION_H

#include "dsp_real.h"

#define AKS_CALIBRATION(name, value) static const ZiviReal aks_##name = value;
#include "aks_calibration.def"
#undef AKS_CALIBRATION

#endif
