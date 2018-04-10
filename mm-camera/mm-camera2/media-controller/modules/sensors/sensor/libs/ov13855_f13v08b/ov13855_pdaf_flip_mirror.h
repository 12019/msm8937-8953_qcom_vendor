/**
 * ov13855_pdaf_flip_mirror.h
 *
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
 .vendor_id = QTI,
 .orientation= PDAF_ORIENTATION_MIRROR_AND_FLIP,
 .sensor_native_pattern_info =
 {
  /* Res 0 */
  {
   .block_pattern = {
    .pd_offset_horizontal = 72,
    .pd_offset_vertical = 64,
    .pix_count = 16,
    .block_dim = {
      .width = 32,
      .height = 32,
    },
    .pix_coords =
    {
     {
      .x = 2 + 72,
      .y = 14 + 64,
      .flag = PDAF_LEFT_PIXEL,
     },
     {
      .x = 10 + 72,
      .y = 2 + 64,
      .flag = PDAF_LEFT_PIXEL,
     },
     {
      .x = 18 + 72,
      .y = 14 + 64,
      .flag = PDAF_LEFT_PIXEL,
     },
     {
      .x = 26 + 72,
      .y = 2 + 64,
      .flag = PDAF_LEFT_PIXEL,
     },
     {
     .x = 2 + 72,
     .y = 30 + 64,
     .flag = PDAF_LEFT_PIXEL,
     },
     {
     .x = 10 + 72,
     .y = 18 + 64,
     .flag = PDAF_LEFT_PIXEL,
     },
     {
     .x = 18 + 72,
     .y = 30 + 64,
     .flag = PDAF_LEFT_PIXEL,
     },
     {
     .x = 26 + 72,
     .y = 18 + 64,
     .flag = PDAF_LEFT_PIXEL,
     },
     {
      .x = 2 + 72,
      .y = 10 + 64,
      .flag = PDAF_RIGHT_PIXEL,
     },
     {
      .x = 10 + 72,
      .y = 6 + 64,
      .flag = PDAF_RIGHT_PIXEL,
     },
     {
      .x = 18 + 72,
      .y = 10 + 64,
      .flag = PDAF_RIGHT_PIXEL,
     },
     {
      .x = 26 + 72,
      .y = 6 + 64,
      .flag = PDAF_RIGHT_PIXEL,
     },
      {
      .x = 2 + 72,
      .y = 26 + 64,
      .flag = PDAF_RIGHT_PIXEL,
     },
     {
      .x = 10 + 72,
      .y = 22+ 64,
      .flag = PDAF_RIGHT_PIXEL,
     },
     {
      .x = 18 + 72,
      .y = 26 + 64,
      .flag = PDAF_RIGHT_PIXEL,
     },
     {
      .x = 26 + 72,
      .y = 22 + 64,
      .flag = PDAF_RIGHT_PIXEL,
     },
    },
   },
   /* use estimate blk level instead of ref to chromatix */
   .block_count_horizontal = 128,
   .block_count_vertical = 94,
  },
 },
 .buffer_block_pattern_info =
 {
  /* Res 0 */
  {
 .buffer_data_type = PDAF_DATA_TYPE_RAW10_LSB,
 .buffer_type = PDAF_BUFFER_FLAG_SPARSE,
  },
 },
 .black_level = 64,
 .cali_version = CALIBRATION_VERSION_2D,
 .window_configure = {
  .pdaf_sw_window_mode = FIXED_GRID_WINDOW,
  /* ingore the floating window configuration */
  .fixed_grid_window_configure =
  {
   .af_fix_window = {
    .pdaf_address_start_hori = 0.33333,
    .pdaf_address_start_ver  = 0.33334,
    .pdaf_address_end_hori   = 0.66668,
    .pdaf_address_end_ver    = 0.66668,
   },
   .window_number_hori = 1,
   .window_number_ver  = 1,
  },
 },
 .defocus_confidence_th = 200,
