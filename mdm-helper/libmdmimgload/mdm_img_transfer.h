/*
 * ----------------------------------------------------------------------------
 *  Copyright (c) 2012-2014 Qualcomm Technologies, Inc.  All Rights Reserved.
 *  Qualcomm Technologies Proprietary and Confidential.
 *  ---------------------------------------------------------------------------
 */

#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <errno.h>

typedef enum {
  MODE_DEFAULT,
  MODE_FORCE_RAM_DUMPS,
  MODE_IGNORE_RAM_DUMPS
} img_transfer_mode;

bool mdm_img_transfer(char* mdm_name,
                      char* mdm_link,
                      char* ram_dump_path,
                      char* saved_file_prefix,
                      img_transfer_mode mode);



