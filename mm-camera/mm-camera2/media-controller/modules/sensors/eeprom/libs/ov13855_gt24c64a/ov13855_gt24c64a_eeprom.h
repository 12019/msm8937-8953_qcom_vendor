/*============================================================================

Copyright (c) 2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.

============================================================================*/
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "../eeprom_util/eeprom.h"
#include "eeprom_lib.h"
#include "eeprom_util.h"
#include "debug_lib.h"
#include "sensor_lib.h"


#define MODULE_INFO_FLAG_OFFSET 0x00
#define MODULE_INFO_OFFSET      0x01

#define AWB_FLAG_OFFSET         0x08
#define AWB_OFFSET              AWB_FLAG_OFFSET + 1

#define VALID_FLAG          0x55
#define QVALUE                  1024.0
#define INVALID_DATA        0xFFFF



#define LSC_FLAG_OFFSET         0x1A
#define LSC_OFFSET              LSC_FLAG_OFFSET + 1
#define LSC_GRID_SIZE           (17*13)



typedef struct {
  unsigned char module_id;
  unsigned char lens_id;
  unsigned char vcm_id;
  unsigned char year;
  unsigned char month;
  unsigned char day;
} module_info_t;


typedef struct {
  unsigned char r_over_gr_l;
  unsigned char r_over_gr_h;
  unsigned char b_over_gr_l;
  unsigned char b_over_gr_h;
  unsigned char golden_r_over_gr_l;
  unsigned char golden_r_over_gr_h;
  unsigned char golden_b_over_gr_l;
  unsigned char golden_b_over_gr_h;
} awb_data_t;

typedef struct {
  unsigned char macro_h;
  unsigned char macro_l;
  unsigned char infinity_h;
  unsigned char infinity_l;
} af_data_t;

static struct camera_i2c_reg_setting g_reg_setting;
static struct camera_i2c_reg_array g_reg_array[20];


static unsigned char awb_present      = FALSE;
static unsigned char af_present       = FALSE;
static unsigned char lsc_present      = FALSE;
static unsigned char insensor_present = FALSE;




void ov13855_gt24c64a_eeprom_get_calibration_items(void *e_ctrl);
void ov13855_gt24c64a_eeprom_format_calibration_data(void *e_ctrl);
static int sunny_gt24c64_ov13855_eeprom_get_raw_data(void *e_ctrl, void *data);

static eeprom_lib_func_t ov13855_gt24c64a_eeprom_lib_func_ptr = {
  .get_calibration_items    = ov13855_gt24c64a_eeprom_get_calibration_items,
  .format_calibration_data  = ov13855_gt24c64a_eeprom_format_calibration_data,
  .do_wbc_calibration       = eeprom_whitebalance_calibration,
  .get_raw_data             = sunny_gt24c64_ov13855_eeprom_get_raw_data,
  .get_ois_raw_data         = NULL,
  .eeprom_info =
  {
    .power_setting_array =
    {
      .power_setting_a =
      {
        {
          .seq_type = CAMERA_POW_SEQ_VREG,
          .seq_val = CAMERA_VIO,
          .config_val = 0,
          .delay = 0,
        },
      },
      .size = 1,
      .power_down_setting_a =
      {
        {
          .seq_type = CAMERA_POW_SEQ_VREG,
          .seq_val = CAMERA_VIO,
          .config_val = 0,
          .delay = 0,
        },
      },
      .size_down = 1,
    },
    .i2c_freq_mode = SENSOR_I2C_MODE_STANDARD,
    .mem_map_array =
    {
      .memory_map =
      {
        {
          .slave_addr = 0xa0,
          .mem_settings =
          {
            {0x0, CAMERA_I2C_WORD_ADDR,
              1796, CAMERA_I2C_BYTE_DATA, CAMERA_I2C_OP_READ, 0 },
          },
          .memory_map_size = 1,
        },
      },
      .size_map_array = 1,
    },
  },
};
