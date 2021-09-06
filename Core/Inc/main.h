/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"
#include "stm32l4xx_hal_conf.h"
#include "stm32l4xx_hal_def.h"
#include "cmsis_os.h"
#include "SensorTile.box.h"
#include "audiolog.h"

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

/* Exported functions prototypes ---------------------------------------------*/
void ErrorHandler(ErrorType_t ErrorType);

#endif /* __MAIN_H */

/****END OF FILE****/
