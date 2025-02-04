//! @file wmc_tables.h

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef _ASC_FEATURESCALER_H_
#define _ASC_FEATURESCALER_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "arm_math.h"

/* Imported variables --------------------------------------------------------*/
extern const float featureScalerMean[960];
extern const float featureScalerStd[960];

extern const float32_t hannWin_1024[1024];

#ifdef __cplusplus
}
#endif

#endif /* _ASC_FEATURESCALER_H_ */

/*****END OF FILE****/

