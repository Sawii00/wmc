/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __AUDIOLOG_H
#define __AUDIOLOG_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <stdio.h>
#include <math.h>
#include "string.h"

#include "main.h"
#include "SensorTile.box_audio.h"

/* FatFs includes */
#include "ff.h"
#include "ff_gen_drv.h"
#include "sd_diskio_SensorTile.box.h"

/* Exported functions prototypes ---------------------------------------------*/
void AUDIOLOG_SDInit(void);
void AUDIOLOG_SDDeInit(void);

void AUDIOLOG_Enable(void);
void AUDIOLOG_Disable(void);

void AUDIOLOG_RecordingProcess(uint16_t *pPCMBuffer);
void AUDIOLOG_Save2SD(void);

void WMC_Save2SD(uint16_t *WMC_Buffer, uint8_t end);
#ifdef __cplusplus
}
#endif

#endif /* __AUDIOLOG_H */

/****END OF FILE****/

