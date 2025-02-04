/* Based on SensorTile.box_conf_template.h */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SENSORTILE_BOX_CONF_H__
#define __SENSORTILE_BOX_CONF_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32l4xx_hal.h"
#include "SensorTile.box_bus.h"
#include "SensorTile.box_errno.h"

#define USE_MOTION_SENSOR_LIS2DW12_0        0U
#define USE_MOTION_SENSOR_LIS2MDL_0         0U
#define USE_MOTION_SENSOR_LIS3DHH_0         0U
#define USE_MOTION_SENSOR_LSM6DSOX_0        0U
#define USE_ENV_SENSOR_HTS221_0             0U
#define USE_ENV_SENSOR_LPS22HH_0            0U
#define USE_ENV_SENSOR_STTS751_0            0U
#define USE_AUDIO_IN                        1U

/* Define 1 to use already implemented callback; 0 to implement callback into an application file */
#define USE_BC_CHG_IRQ_HANDLER              0
#define USE_BC_TIM_IRQ_CALLBACK             1
#define USE_BC_IRQ_CALLBACK                 0

#define AUDIO_SAMPLING_FREQUENCY  48000
#define BSP_AUDIO_IN_IT_PRIORITY 5U

#ifdef __cplusplus
}
#endif

#endif /* __SENSORTILE_BOX_CONF_H__*/

/****END OF FILE****/
