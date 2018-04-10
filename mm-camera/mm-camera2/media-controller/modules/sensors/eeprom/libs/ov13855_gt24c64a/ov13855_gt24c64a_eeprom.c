#include "ov13855_gt24c64a_eeprom.h"
void wintech_otp_error()
{
	g_reg_setting.size = 1;
	g_reg_setting.addr_type = CAMERA_I2C_WORD_ADDR;
	g_reg_setting.data_type = CAMERA_I2C_BYTE_DATA;
	g_reg_setting.reg_setting = &g_reg_array[0];
	g_reg_setting.delay = 0;
	g_reg_setting.reg_setting[0].reg_addr=0x4503;
	g_reg_setting.reg_setting[0].reg_data=0x80;
	g_reg_array[0].reg_addr = 0x4503;
	g_reg_array[0].reg_data = 0x80;
}
static unsigned char ov13855_sunny_checksum(unsigned char *buf,
unsigned int first, unsigned int last, unsigned int position)
{
  unsigned char retval = 0;
  unsigned int i = 0, sum = 0;
  SDBG("Enter");
  for(i = first; i <= last; i++){
    sum += buf[i];
  }
  if((sum%255+1) == buf[position]){
    SLOW("Checksum SUCESS, first:%x, last:%x, pos:%x, sum:%d %d, buf:%d" , first, last, position, sum, (sum%255+1), buf[position]);
    retval = 1;
  } else {
    wintech_otp_error();
    SERR("Checksum fail, first:%x, last:%x, pos:%x, sum:%d %d, buf:%d" , first, last, position, sum, (sum%255+1), buf[position]);
  }
  SDBG("Exit");
  return retval;
}

void ov13855_gt24c64a_eeprom_get_calibration_items(void *e_ctrl)
{
  sensor_eeprom_data_t *ectrl = (sensor_eeprom_data_t *)e_ctrl;
  eeprom_calib_items_t *e_items = &(ectrl->eeprom_data.items);
  e_items->is_wbc = awb_present ? TRUE : FALSE;
  e_items->is_afc         = FALSE;
  e_items->is_lsc = lsc_present ? TRUE : FALSE;
  e_items->is_dpc         = FALSE;
  e_items->is_insensor    = TRUE;
  e_items->is_ois         = FALSE;
  SLOW("is_wbc:%d,is_afc:%d,is_lsc:%d,is_dpc:%d,is_insensor:%d,\
is_ois:%d",e_items->is_wbc,e_items->is_afc,
    e_items->is_lsc,e_items->is_dpc,e_items->is_insensor,
    e_items->is_ois);
}
static void sunny_gt24c64_13855_eeprom_format_wbdata(sensor_eeprom_data_t *e_ctrl)
{
  unsigned char flag;
  module_info_t *module_info;
  awb_data_t    *wb;
  float         r_over_gr, b_over_gb, gr_over_gb;
  int           i;
  SLOW("Enter");
  flag = e_ctrl->eeprom_params.buffer[AWB_FLAG_OFFSET];
  if (flag != VALID_FLAG) {
    awb_present = FALSE;
    SERR("AWB : empty or invalid data");
    return;
  }
  awb_present = TRUE;
  /* Print module info */
  flag = e_ctrl->eeprom_params.buffer[MODULE_INFO_FLAG_OFFSET];
  if (flag == VALID_FLAG) {
    module_info = (module_info_t*)&e_ctrl->eeprom_params.buffer[MODULE_INFO_OFFSET];
    SLOW("Module ID: %d", module_info->module_id);
    SLOW("LENS ID: %d", module_info->lens_id);
    SLOW("VCM ID: %d", module_info->vcm_id);
    SLOW("Y/M/D: %d/%d/%d", module_info->year + 2000, module_info->month, module_info->day);
  } else {
    SERR("MODULE_INFO : empty or invalid data");
  }

  /* Get AWB data */
  wb = (awb_data_t *)(e_ctrl->eeprom_params.buffer + AWB_OFFSET);
 SLOW("AWB : checksum");
  ov13855_sunny_checksum((unsigned char *)(e_ctrl->eeprom_params.buffer + AWB_FLAG_OFFSET),0,8,9);
  
  r_over_gr = (((float)((wb->r_over_gr_h << 8) | wb->r_over_gr_l))-0.5) / QVALUE;  
  gr_over_gb=1;
  b_over_gb = ((((float)((wb->b_over_gr_h << 8) | wb->b_over_gr_l))-0.5) / QVALUE) * gr_over_gb;

  SLOW("AWB : r/gr = %f", r_over_gr);
  SLOW("AWB : b/gb = %f", b_over_gb);
  SLOW("AWB : gr/gb = %f", gr_over_gb);

  for (i = 0; i < AGW_AWB_MAX_LIGHT; i++) {
    e_ctrl->eeprom_data.wbc.r_over_g[i] = r_over_gr;
    e_ctrl->eeprom_data.wbc.b_over_g[i] = b_over_gb;
  }
  e_ctrl->eeprom_data.wbc.gr_over_gb = gr_over_gb;


  SERR("Exit");
}

static void sunny_gt24c64_13855_eeprom_format_lscdata(
        sensor_eeprom_data_t *e_ctrl)
{
  unsigned char  flag;
  unsigned short i, light;
  unsigned char  *lsc_buf;
  float gain_r, gain_gr, gain_gb, gain_b;

  SDBG("Enter");
  /* Check validity */
  flag = e_ctrl->eeprom_params.buffer[LSC_FLAG_OFFSET];
  if (flag != VALID_FLAG) {
   lsc_present = FALSE;
   SERR("LSC : empty or invalid data");
   return;
  }
  lsc_present = TRUE;
  SLOW("LSC : checksum");
  ov13855_sunny_checksum((unsigned char *)(e_ctrl->eeprom_params.buffer + LSC_FLAG_OFFSET),0,1768,1769);

  lsc_buf = e_ctrl->eeprom_params.buffer + LSC_OFFSET;

  for (light = 0; light < ROLLOFF_MAX_LIGHT; light++) {
   e_ctrl->eeprom_data.lsc.lsc_calib[light].mesh_rolloff_table_size =
          LSC_GRID_SIZE;
  }

  /* read gain */
  for (i = 0; i < LSC_GRID_SIZE * 2; i += 2) {
   gain_r = lsc_buf[i + 0] << 8 | lsc_buf[i + 1];
   gain_gr = lsc_buf[i + (LSC_GRID_SIZE<<1)] << 8 | lsc_buf[i + (LSC_GRID_SIZE<<1) + 1];
   gain_gb = lsc_buf[i + (LSC_GRID_SIZE<<2)] << 8 | lsc_buf[i + (LSC_GRID_SIZE<<2) + 1];
   gain_b = lsc_buf[i + (LSC_GRID_SIZE*6) ] << 8 | lsc_buf[i + (LSC_GRID_SIZE*6) + 1];

  for (light = 0; light < ROLLOFF_MAX_LIGHT; light++) {
   e_ctrl->eeprom_data.lsc.lsc_calib[light].r_gain[i>>1]  = gain_r;
   e_ctrl->eeprom_data.lsc.lsc_calib[light].gr_gain[i>>1] = gain_gr;
   e_ctrl->eeprom_data.lsc.lsc_calib[light].gb_gain[i>>1] = gain_gb;
   e_ctrl->eeprom_data.lsc.lsc_calib[light].b_gain[i>>1]  = gain_b;
   }
  }

  SDBG("Exit");
}

static int sunny_gt24c64_ov13855_eeprom_get_raw_data(void *e_ctrl, void *data)
{
  SLOW("Enter");
  if (e_ctrl && data)
  {
   memcpy(data, &g_reg_setting, sizeof(g_reg_setting));
   }
  else {
    SERR("failed Null pointer");
    return SENSOR_FAILURE;
  }
  SLOW("Exit");
  return SENSOR_SUCCESS;
}

void ov13855_gt24c64a_eeprom_format_calibration_data(void *e_ctrl) {
  sensor_eeprom_data_t * ctrl = (sensor_eeprom_data_t *)e_ctrl;
  unsigned short       rc     = SENSOR_FAILURE;
  unsigned int i          = 0;

  RETURN_VOID_ON_NULL(ctrl);
  SLOW("Total bytes in OTP buffer: %d", ctrl->eeprom_params.num_bytes);

  if (!ctrl->eeprom_params.buffer || !ctrl->eeprom_params.num_bytes) {
    SERR("Buff pointer %p buffer size %d", ctrl->eeprom_params.buffer,
      ctrl->eeprom_params.num_bytes);
    return;
  }

 for(i = 0; i < ctrl->eeprom_params.num_bytes; i++)
 {
	if(i<30)
	{
	   SLOW("ctrl->eeprom_params.buffer[%x]: %x", i, ctrl->eeprom_params.buffer[i]);
	}
 }

sunny_gt24c64_13855_eeprom_format_wbdata(ctrl);
 sunny_gt24c64_13855_eeprom_format_lscdata(ctrl);
}

void* ov13855_gt24c64a_eeprom_open_lib(void) {
  return &ov13855_gt24c64a_eeprom_lib_func_ptr;
}
