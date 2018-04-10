/*===========================================================================

Copyright (c) 2013-2015 Qualcomm Technologies, Inc.  All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential.

===========================================================================*/

#include "thermal.h"
#include "thermal_config.h"
#include "ss_algorithm.h"

static struct setting_info ss_cfgs_8974[] =
{
	{
		.desc = "SS-CPU0",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu1",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU2",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu2",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu3",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 80000,
			.set_point_clr = 55000,
			.time_constant = 16,
		},
	},
};

static struct setting_info ss_cfgs_8974_pro_ac[] =
{
	{
		.desc = "SS-CPU0",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu1",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU2",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu2",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu3",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 80000,
			.set_point_clr = 55000,
			.time_constant = 16,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "tsens_tz_sensor10",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 100000,
			.set_point_clr = 65000,
		},
	},
};

static struct setting_info ss_cfgs_8226_v1[] =
{
	{
		.desc = "SS-CPU2-3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu2-3",
			.device = "cpu",
			.sampling_period_ms = 1000,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cpu",
			.sampling_period_ms = 1000,
			.set_point = 50000,
			.set_point_clr = 45000,
			.time_constant = 5,
		},
	},
};

static struct setting_info ss_cfgs_8226[] =
{
	{
		.desc = "SS-CPU0-1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0-1",
			.device = "cpu",
			.sampling_period_ms = 250,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU2-3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu2-3",
			.device = "cpu",
			.sampling_period_ms = 250,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cpu",
			.sampling_period_ms = 1000,
			.set_point = 60000,
			.set_point_clr = 45000,
			.time_constant = 2,
		},
	},
};

static struct setting_info ss_cfgs_8610[] =
{
	{
		.desc = "SS-CPU0-1-2-3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0-1-2-3",
			.device = "cpu",
			.sampling_period_ms = 250,
			.set_point = 80000,
			.set_point_clr = 65000,
		},
	},
};

static struct setting_info ss_cfgs_8916[] =
{
	{
		.desc = "SS-CPU0-1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0-1",
			.device = "cpu",
			.sampling_period_ms = 250,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU2-3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu2-3",
			.device = "cpu",
			.sampling_period_ms = 250,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cpu",
			.sampling_period_ms = 1000,
			.set_point = 70000,
			.set_point_clr = 45000,
			.time_constant = 2,
		},
	},
};

static struct setting_info ss_cfgs_9900[] =
{
	{
		.desc = "SS-CPU0",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu1",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU2",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu2",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu3",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
};

static struct setting_info ss_cfgs_8939[] =
{
	{
		.desc = "SS-CPU0",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu1",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU2",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu2",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu3",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU4-5-6-7",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu4-5-6-7",
			.device = "cluster0",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster1",
			.sampling_period_ms = 250,
			.set_point = 70000,
			.set_point_clr = 55000,
			.time_constant = 2,
		},
	},
};

static struct setting_info ss_cfgs_8936[] =
{
	{
		.desc = "SS-CPU0",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0",
			.device = "cluster0",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu1",
			.device = "cluster0",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU2",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu2",
			.device = "cluster0",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu3",
			.device = "cluster0",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster0",
			.sampling_period_ms = 250,
			.set_point = 70000,
			.set_point_clr = 55000,
			.time_constant = 2,
		},
	},
};

static struct setting_info ss_cfgs_8994[] =
{
	{
		.desc = "SS-CPU0",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0",
			.device = "cluster0",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu1",
			.device = "cluster0",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU2",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu2",
			.device = "cluster0",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu3",
			.device = "cluster0",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU4",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu4",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU5",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu5",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU6",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu6",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU7",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu7",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 80000,
			.set_point_clr = 55000,
			.time_constant = 16,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
};

static struct setting_info ss_cfgs_8909[] =
{
	{
		.desc = "SS-CPU0-2",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0-2",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU1-3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu1-3",
			.device = "cpu",
			.sampling_period_ms = 65,
			.set_point = 90000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cpu",
			.sampling_period_ms = 250,
			.set_point = 80000,
			.set_point_clr = 55000,
			.time_constant = 2,
		},
	},
};

static struct setting_info ss_cfgs_8992[] =
{
	{
		.desc = "SS-CPU0",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0-1",
			.device = "cluster0",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU2",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu2",
			.device = "cluster0",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu3",
			.device = "cluster0",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU4",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu4",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU5",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu5",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 80000,
			.set_point_clr = 55000,
			.time_constant = 16,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
};

static struct setting_info ss_cfgs_8996AU[] =
{
	{
		.desc = "SS-CPUS-ALL",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "VIRTUAL-CPUS",
			.device = "cpu_voltage",
			.sampling_period_ms = 10,
			.set_point = 105000,
			.set_point_clr = 85000,
			.device_perf_floor = 1036800,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 95000,
			.set_point_clr = 65000,
			.time_constant = 16,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
};

static struct setting_info ss_cfgs_8996[] =
{
	{
		.desc = "SS-CPUS-ALL",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "VIRTUAL-CPUS",
			.device = "cpu_voltage",
			.sampling_period_ms = 10,
			.set_point = 95000,
			.set_point_clr = 65000,
			.device_perf_floor = 1036800,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 95000,
			.set_point_clr = 65000,
			.time_constant = 16,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
};

static struct setting_info ss_cfgs_8952[] =
{
	{
		.desc = "SS-CPU0",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "tsens_tz_sensor5",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "tsens_tz_sensor6",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU2",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "tsens_tz_sensor7",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "tsens_tz_sensor8",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU4-5-6-7",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "tsens_tz_sensor9",
			.device = "cluster0",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster1",
			.sampling_period_ms = 250,
			.set_point = 70000,
			.set_point_clr = 55000,
			.time_constant = 2,
			.device_mtgn_max_limit = 960000,
		},
	},
};

static struct setting_info ss_cfgs_8976[] =
{
	{
		.desc = "SS-CPU0-1-2-3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0-1-2-3",
			.device = "cluster0",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU4",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu4",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU5",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu5",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU6",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu6",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU7",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu7",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-L2_CACHE_1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "L2_cache_1",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster1",
			.sampling_period_ms = 250,
			.set_point = 70000,
			.set_point_clr = 55000,
			.time_constant = 2,
		},
	},
};

static struct setting_info ss_cfgs_8956[] =
{
	{
		.desc = "SS-CPU0-1-2-3",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu0-1-2-3",
			.device = "cluster0",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU4",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu4",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CPU5",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "cpu5",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-L2_CACHE_1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "L2_cache_1",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster1",
			.sampling_period_ms = 250,
			.set_point = 70000,
			.set_point_clr = 55000,
			.time_constant = 2,
		},
	},
};

static struct setting_info ss_cfgs_cobalt[] =
{
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster1",
			.sampling_period_ms = 10,
			.set_point = 95000,
			.set_point_clr = 65000,
			.time_constant = 16,
		},
	},
	{
		.desc = "SS-GPU-1",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "tsens_tz_sensor12",
			.device = "gpu",
			.sampling_period_ms = 10,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
	{
		.desc = "SS-GPU-2",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "tsens_tz_sensor13",
			.device = "gpu",
			.sampling_period_ms = 10,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
};

static struct setting_info ss_cfgs_8937[] =
{
	{
		.desc = "SS-CPUS",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "VIRTUAL-CPUS",
			.device = "cpu_voltage",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cpu_voltage",
			.sampling_period_ms = 250,
			.set_point = 70000,
			.set_point_clr = 55000,
			.time_constant = 2,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	}
};

static struct setting_info ss_cfgs_8937_qrd[] =
{
	{
		.desc = "SS-CPUS",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "VIRTUAL-CPUS",
			.device = "cpu_voltage",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CASE-THERM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "case_therm",
			.device = "cpu_voltage",
			.sampling_period_ms = 1000,
			.set_point = 44000,
			.set_point_clr = 41000,
			.time_constant = 3,
			.device_perf_floor = 1094400,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
	{
		.desc = "SS-GPU-CASE-THERM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "case_therm",
			.device = "gpu",
			.sampling_period_ms = 10000,
			.set_point = 50000,
			.set_point_clr = 45000,
			.device_mtgn_max_limit = 375000000,
		},
	}
};

static struct setting_info ss_cfgs_8940_qrd[] =
{
	{
		.desc = "SS-CPUS",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "VIRTUAL-CPUS",
			.device = "cpu_voltage",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CASE-THERM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "case_therm",
			.device = "cpu_voltage",
			.sampling_period_ms = 1000,
			.set_point = 46000,
			.set_point_clr = 42000,
			.time_constant = 3,
			.device_perf_floor = 1094400,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
	{
		.desc = "SS-GPU-CASE-THERM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "case_therm",
			.device = "gpu",
			.sampling_period_ms = 10000,
			.set_point = 52000,
			.set_point_clr = 45000,
			.device_mtgn_max_limit = 375000000,
		},
	}
};

static struct setting_info ss_cfgs_8917[] =
{
	{
		.desc = "SS-CPUS",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "VIRTUAL-CPUS",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster1",
			.sampling_period_ms = 250,
			.set_point = 70000,
			.set_point_clr = 55000,
			.time_constant = 2,
			.device_mtgn_max_limit = 960000,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
};

static struct setting_info ss_cfgs_8917_qrd[] =
{
	{
		.desc = "SS-CPUS",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "VIRTUAL-CPUS",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CASE-THERM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "case_therm",
			.device = "cluster1",
			.sampling_period_ms = 1000,
			.set_point = 44000,
			.set_point_clr = 41000,
			.time_constant = 3,
			.device_mtgn_max_limit = 960000,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
	{
		.desc = "SS-GPU-CASE-THERM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "case_therm",
			.device = "gpu",
			.sampling_period_ms = 10000,
			.set_point = 50000,
			.set_point_clr = 45000,
			.device_mtgn_max_limit = 400000000,
		},
	}
};

static struct setting_info ss_cfgs_8920_qrd[] =
{
	{
		.desc = "SS-CPUS",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "VIRTUAL-CPUS",
			.device = "cluster1",
			.sampling_period_ms = 50,
			.set_point = 85000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CASE-THERM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "case_therm",
			.device = "cluster1",
			.sampling_period_ms = 1000,
			.set_point = 44000,
			.set_point_clr = 40000,
			.time_constant = 3,
			.device_mtgn_max_limit = 1094400,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
	{
		.desc = "SS-GPU-CASE-THERM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "case_therm",
			.device = "gpu",
			.sampling_period_ms = 10000,
			.set_point = 50000,
			.set_point_clr = 45000,
			.device_mtgn_max_limit = 400000000,
		},
	}
};

static struct setting_info ss_cfgs_9607[] =
{
	{
		.desc = "SS-CPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "tsens_tz_sensor4",
			.device = "cpu",
			.sampling_period_ms = 50,
			.set_point = 105000,
			.set_point_clr = 75000,
		},
	},
};

static struct setting_info ss_cfgs_8953[] =
{
	{
		.desc = "SS-CPUS-ALL",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "VIRTUAL-CPUS",
			/* Since all cores are synchronous cores using
			   a single cpufreq policy, use "cluster0" device
			   for all core/L2 tsens */
			.device = "cluster0",
			.sampling_period_ms = 50,
			.set_point = 95000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-POPMEM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "pop_mem",
			.device = "cluster0",
			.sampling_period_ms = 250,
			.set_point = 70000,
			.set_point_clr = 55000,
			.time_constant = 2,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
};

static struct setting_info ss_cfgs_8953_qrd[] =
{
	{
		.desc = "SS-CPUS-ALL",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "VIRTUAL-CPUS",
			/* Since all cores are synchronous cores using
			   a single cpufreq policy, use "cluster0" device
			   for all core/L2 tsens */
			.device = "cluster0",
			.sampling_period_ms = 50,
			.set_point = 95000,
			.set_point_clr = 55000,
		},
	},
	{
		.desc = "SS-CASE-THERM",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "case_therm",
			.device = "cluster0",
			.sampling_period_ms = 10000,
			.set_point = 43000,
			.set_point_clr = 40000,
			.device_mtgn_max_limit = 1689600,
		},
	},
	{
		.desc = "SS-GPU",
		.algo_type = SS_ALGO_TYPE,
		.data.ss =
		{
			.sensor = "gpu",
			.device = "gpu",
			.sampling_period_ms = 250,
			.set_point = 95000,
			.set_point_clr = 65000,
		},
	},
};

void ss_init_data(struct thermal_setting_t *setting)
{
	int i, arr_size;
	struct setting_info *cfg;

	switch (therm_get_msm_id()) {
		case THERM_MSM_8974:
		case THERM_MSM_8x62:
		case THERM_MSM_8974PRO_AA:
		case THERM_MSM_8974PRO_AB:
			cfg = ss_cfgs_8974;
			arr_size = ARRAY_SIZE(ss_cfgs_8974);
			break;
		case THERM_MSM_8974PRO_AC:
		case THERM_MSM_8084:
			cfg = ss_cfgs_8974_pro_ac;
			arr_size = ARRAY_SIZE(ss_cfgs_8974_pro_ac);
			break;
		case THERM_MSM_8226:
			switch (therm_get_msm_version()) {
				case THERM_VERSION_V1:
					cfg = ss_cfgs_8226_v1;
					arr_size = ARRAY_SIZE(ss_cfgs_8226_v1);
					break;
				default:
					cfg = ss_cfgs_8226;
					arr_size = ARRAY_SIZE(ss_cfgs_8226);
					break;
			}
			break;
		case THERM_MSM_8926:
			cfg = ss_cfgs_8226;
			arr_size = ARRAY_SIZE(ss_cfgs_8226);
			break;
		case THERM_MSM_8916:
			cfg = ss_cfgs_8916;
			arr_size = ARRAY_SIZE(ss_cfgs_8916);
			break;
		case THERM_MSM_8610:
			cfg = ss_cfgs_8610;
			arr_size = ARRAY_SIZE(ss_cfgs_8610);
			break;
		case THERM_FSM_9900:
			cfg = ss_cfgs_9900;
			arr_size = ARRAY_SIZE(ss_cfgs_9900);
			break;
		case THERM_MSM_8939:
		case THERM_MSM_8929:
			cfg = ss_cfgs_8939;
			arr_size = ARRAY_SIZE(ss_cfgs_8939);
			break;
		case THERM_MSM_8936:
			cfg = ss_cfgs_8936;
			arr_size = ARRAY_SIZE(ss_cfgs_8936);
			break;
		case THERM_MSM_8994:
			cfg = ss_cfgs_8994;
			arr_size = ARRAY_SIZE(ss_cfgs_8994);
			break;
		case THERM_MSM_8909:
			cfg = ss_cfgs_8909;
			arr_size = ARRAY_SIZE(ss_cfgs_8909);
			break;
		case THERM_MSM_8992:
			cfg = ss_cfgs_8992;
			arr_size = ARRAY_SIZE(ss_cfgs_8992);
			break;
		case THERM_MSM_8996AU:
			cfg = ss_cfgs_8996AU;
			arr_size = ARRAY_SIZE(ss_cfgs_8996AU);
			break;
		case THERM_MSM_8996:
			cfg = ss_cfgs_8996;
			arr_size = ARRAY_SIZE(ss_cfgs_8996);
			break;
		case THERM_MSM_8952:
			cfg = ss_cfgs_8952;
			arr_size = ARRAY_SIZE(ss_cfgs_8952);
			break;
		case THERM_MSM_8976:
			cfg = ss_cfgs_8976;
			arr_size = ARRAY_SIZE(ss_cfgs_8976);
			break;
		case THERM_MSM_8956:
			cfg = ss_cfgs_8956;
			arr_size = ARRAY_SIZE(ss_cfgs_8956);
			break;
		case THERM_MSM_COBALT:
			cfg = ss_cfgs_cobalt;
			arr_size = ARRAY_SIZE(ss_cfgs_cobalt);
			break;
		case THERM_MSM_8937:
			switch (therm_get_hw_platform()) {
			case THERM_PLATFORM_MTP:
				cfg = ss_cfgs_8937;
				arr_size =
				ARRAY_SIZE(ss_cfgs_8937);
				break;
			default:
				cfg = ss_cfgs_8937_qrd;
				arr_size =
				ARRAY_SIZE(ss_cfgs_8937_qrd);
				break;
			}
			break;
		case THERM_MSM_8940:
			switch (therm_get_hw_platform()) {
			case THERM_PLATFORM_MTP:
				cfg = ss_cfgs_8937;
				arr_size =
				ARRAY_SIZE(ss_cfgs_8937);
				break;
			default:
				cfg = ss_cfgs_8940_qrd;
				arr_size =
				ARRAY_SIZE(ss_cfgs_8940_qrd);
				break;
			}
			break;
		case THERM_MDM_9607:
			cfg = ss_cfgs_9607;
			arr_size = ARRAY_SIZE(ss_cfgs_9607);
			break;
		case THERM_MSM_8953:
			switch (therm_get_hw_platform()) {
			case THERM_PLATFORM_MTP:
				cfg = ss_cfgs_8953;
				arr_size = ARRAY_SIZE(ss_cfgs_8953);
				break;
			default:
				cfg = ss_cfgs_8953_qrd;
				arr_size = ARRAY_SIZE(ss_cfgs_8953_qrd);
				break;
			}
			break;
		case THERM_MSM_8917:
			switch (therm_get_hw_platform()) {
			case THERM_PLATFORM_MTP:
				cfg = ss_cfgs_8917;
				arr_size =
					ARRAY_SIZE(ss_cfgs_8917);
				break;
			default:
				cfg = ss_cfgs_8917_qrd;
				arr_size =
					ARRAY_SIZE(ss_cfgs_8917_qrd);
				break;
			}
			break;
		case THERM_MSM_8920:
			switch (therm_get_hw_platform()) {
			case THERM_PLATFORM_MTP:
				cfg = ss_cfgs_8917;
				arr_size =
					ARRAY_SIZE(ss_cfgs_8917);
				break;
			default:
				cfg = ss_cfgs_8920_qrd;
				arr_size =
					ARRAY_SIZE(ss_cfgs_8920_qrd);
				break;
			}
			break;
		default:
			msg("%s: ERROR Uknown device\n", __func__);
			/* Better to have something in place even if it's wrong. */
			cfg = ss_cfgs_8974;
			arr_size = ARRAY_SIZE(ss_cfgs_8974);
			break;
	}

	for (i = 0; i < arr_size; i++)
		add_setting(setting, &cfg[i]);
}
