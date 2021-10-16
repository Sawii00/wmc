/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __AI_H
#define __AI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "ai_platform.h"
#include "wmc.h"
#include "wmc_data.h"

int ai_init(void);
int ai_deinit(void);
int ai_run(void *data_in, void *data_out);

int ai_convertinputfloat_2_int8(ai_float *In_f32, ai_i8 *Out_int8);
int ai_convertoutputint8_2_float(ai_i8 *In_int8, ai_float *Out_f32);

#ifdef __cplusplus
}
#endif

#endif /* __AI_H */

/****END OF FILE****/

