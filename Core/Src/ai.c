#ifdef __cplusplus
 extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
/* System headers */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>

#include "ai.h"
#include "main.h"
#include "ai_datatypes_defines.h"

/* Global AI objects */
static ai_handle wmc = AI_HANDLE_NULL;
static ai_network_report wmc_info;

/* Global c-array to handle the activations buffer */
AI_ALIGNED(4)
static ai_u8 activations[AI_WMC_DATA_ACTIVATIONS_SIZE];

static void ai_log_err(const ai_error err, const char *fct)
{
  /* Possibility to print out error information */
  //if (fct)
  //  printf("TEMPLATE - Error (%s) - type=0x%02x code=0x%02x\r\n", fct,
  //      err.type, err.code);
  //else
  //  printf("TEMPLATE - Error - type=0x%02x code=0x%02x\r\n", err.type, err.code);
}

static int ai_boostrap(ai_handle w_addr, ai_handle act_addr)
{
  ai_error err;

  /* 1 - Create an instance of the model */
  err = ai_wmc_create(&wmc, AI_WMC_DATA_CONFIG);
  if (err.type != AI_ERROR_NONE) {
    ai_log_err(err, "ai_wmc_create");
    return -1;
  }

  /* 2 - Initialize the instance */
  const ai_network_params params = AI_NETWORK_PARAMS_INIT(
      AI_WMC_DATA_WEIGHTS(w_addr),
      AI_WMC_DATA_ACTIVATIONS(act_addr) );

  if (!ai_wmc_init(wmc, &params)) {
      err = ai_wmc_get_error(wmc);
      ai_log_err(err, "ai_wmc_init");
      return -1;
    }

  /* 3 - Retrieve the network info of the created instance */
  if (!ai_wmc_get_info(wmc, &wmc_info)) {
    err = ai_wmc_get_error(wmc);
    ai_log_err(err, "ai_wmc_get_error");
    ai_wmc_destroy(wmc);
    wmc = AI_HANDLE_NULL;
    return -3;
  }

  return 0;
}

int ai_run(void *data_in, void *data_out)
{
  ai_i32 batch;

  ai_buffer *ai_input = wmc_info.inputs;
  ai_buffer *ai_output = wmc_info.outputs;

  ai_input[0].data = AI_HANDLE_PTR(data_in);
  ai_output[0].data = AI_HANDLE_PTR(data_out);

  batch = ai_wmc_run(wmc, ai_input, ai_output);
  if (batch != 1) {
    ai_log_err(ai_wmc_get_error(wmc),
        "ai_wmc_run");
    return -1;
  }

  return 0;
}

int ai_init(void)
{
  return ai_boostrap(ai_wmc_data_weights_get(), activations);
}

int ai_deinit(void)
{
 if (wmc) {
      if (ai_wmc_destroy(wmc) != AI_HANDLE_NULL) {
          ai_log_err(ai_wmc_get_error(wmc), "ai_mnetwork_destroy");
      }
      wmc = NULL;
      return -1;
  }
  return 0;
 }

int ai_convertinputfloat_2_int8(ai_float *In_f32, ai_i8 *Out_int8)
{
  if( AI_HANDLE_NULL == wmc) {
      return -1;
  }

  ai_buffer *bufferPtr = &(wmc_info.inputs[0]);
  ai_buffer_format format = bufferPtr->format;
  int size  = AI_BUFFER_SIZE(bufferPtr);
  ai_float scale ;
  int zero_point ;

  if (AI_BUFFER_FMT_TYPE_Q != AI_BUFFER_FMT_GET_TYPE(format) &&\
    ! AI_BUFFER_FMT_GET_SIGN(format) &&\
    8 != AI_BUFFER_FMT_GET_BITS(format)) {
      return -1;
  }

  if (AI_BUFFER_META_INFO_INTQ(bufferPtr->meta_info)) {
      scale = AI_BUFFER_META_INFO_INTQ_GET_SCALE(bufferPtr->meta_info, 0);
      if (scale != 0.0F) {
         scale= 1.0F/scale ;
      }
      else {
        return 2;
      }
      zero_point = AI_BUFFER_META_INFO_INTQ_GET_ZEROPOINT(bufferPtr->meta_info, 0);
   }
   else {
     return 3;
   }

  for (int i = 0; i < size ; i++) {
    Out_int8[i] = __SSAT((int32_t) roundf((float)zero_point + In_f32[i]*scale), 8);
  }

  return 0;
}

int ai_convertoutputint8_2_float(ai_i8 *In_int8, ai_float *Out_f32)
{
  if( AI_HANDLE_NULL == wmc) {
      return -1;
  }

  ai_buffer * bufferPtr   = &(wmc_info.outputs[0]);
  ai_buffer_format format = bufferPtr->format;
  int size  = AI_BUFFER_SIZE(bufferPtr);
  ai_float scale ;
  int zero_point ;

  if (AI_BUFFER_FMT_TYPE_Q != AI_BUFFER_FMT_GET_TYPE(format) &&\
    ! AI_BUFFER_FMT_GET_SIGN(format) &&\
    8 != AI_BUFFER_FMT_GET_BITS(format)) {
      return -1;
  }

  if (AI_BUFFER_META_INFO_INTQ(bufferPtr->meta_info)) {
      scale = AI_BUFFER_META_INFO_INTQ_GET_SCALE(bufferPtr->meta_info, 0);
      zero_point = AI_BUFFER_META_INFO_INTQ_GET_ZEROPOINT(bufferPtr->meta_info, 0);
  }
  else {
      return -1;
  }

  for (uint32_t i = 0; i < size ; i++) {
    Out_f32[i] = scale * ((ai_float)(In_int8[i]) - zero_point);
  }

  return 0;
}

#ifdef __cplusplus
}
#endif

/****END OF FILE****/

