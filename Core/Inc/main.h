/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_conf.h"
#include "stm32l4xx_hal_def.h"
#include "cmsis_os.h"
#include "arm_math.h"
#include "SensorTile.box.h"
#include "audiolog.h"
#include "wmc_processing.h"
#include "wmc_processing.h"

/* Exported types ------------------------------------------------------------*/
/**
 * @brief  Errors type definitions
 */
typedef enum
{
  ERROR_INIT=0,
  ERROR_AUDIO,
  ERROR_FATFS,
  ERROR_OS,
  ERROR_SYSTEMCLOCK,
  ERROR_AI,
} ErrorType_t;

/* Exported defines ---------------------------------------------------------*/
/* AUDIO_SAMPLING_FREQUENCY defined in SensorTile.box_conf.h! */
#define PCM_BUFFER_SIZE     (AUDIO_SAMPLING_FREQUENCY / 1000)

/* Exported variables ------------------------------------------------------*/

/* Exported functions prototypes ---------------------------------------------*/
void ErrorHandler(ErrorType_t ErrorType);

#endif /* __MAIN_H */

/****END OF FILE****/

