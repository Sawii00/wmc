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

/* Exported defines ----------------------------------------------------------*/
/* AUDIO_SAMPLING_FREQUENCY defined in SensorTile.box_conf.h!!!!!!!!!! */
#define INTERMEDIATE_BUFFER_SIZE (AUDIO_SAMPLING_FREQUENCY/1000)
#define AUDIO_BUFFER_SIZE (INTERMEDIATE_BUFFER_SIZE*64)

/* Exported functions prototypes ---------------------------------------------*/
void AUDIOLOG_SDInit(void);
void AUDIOLOG_SDDeInit(void);

void AUDIOLOG_Enable(void);
void AUDIOLOG_Disable(void);

void AUDIOLOG_InitMic(void);
void AUDIOLOG_DeInitMic(void);

void AUDIOLOG_StartRecording(void);
void AUDIOLOG_StopRecording(void);
void AUDIOLOG_RecordingProcess(uint16_t *pIntermediateBuffer, uint32_t len);
void AUDIOLOG_Save2SD(void);

/* Private defines -----------------------------------------------------------*/

#ifdef __cplusplus
}
#endif

#endif /* __AUDIOLOG_H */

/****END OF FILE****/
