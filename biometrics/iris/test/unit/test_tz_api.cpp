/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include <stdio.h>
#include <errno.h>
#include "string.h"
#include "iris_tz_api.h"

static uint32_t enrollee_id =  0;

int iris_test_get_version(iris_interface *intf)
{
	int ret;
	struct iris_version version;
	ret = intf->get_version(version);
	if (!ret)
		printf("iris get_version success! version %u\n", version.version);
	else
		printf("iris get_version failed %d\n", ret);

	return ret;
}

int iris_test_cmd_input(iris_interface *intf, uint32_t size, char** buf)
{
	int ret;
	ret = intf->test(size, buf);
	printf("iris test done %d\n", ret);

	return ret;
}

int iris_test_enroll_begin(iris_interface *intf)
{
	int ret;

	ret = intf->enroll_begin(enrollee_id);
	if (!ret)
		printf("iris enroll_begin success\n");
	else
		printf("iris enroll_begin failed %d\n", ret);

	return ret;
}

int iris_test_delete_enrollee(iris_interface *intf)
{
	int ret;

	ret = intf->delete_enrollee(0, enrollee_id);
	if (!ret)
		printf("delete_enrollee success\n");
	else
		printf("delete_enrollee failed %d\n", ret);

	return ret;
}

int do_test(iris_interface *intf)
{
	int ret;

	ret = iris_test_get_version(intf);
//	ret = iris_test_enroll_begin(intf);
//	ret = iris_test_delete_enrollee(intf);
	return ret;
}

int main(int argc, char** argv)
{
	int ret;
	iris_interface *intf = NULL;
       bool tz = true;

       if ((argc >= 2) && !strcmp(argv[1], "-h")) {
            printf("Select hlos communication\n");
            tz = false;
       }
	intf = create_iris_tzee_obj(tz);
	if (!intf) {
		printf("fail to create iris tzee obj\n");
		return -EINVAL;
	}
	if (tz) {
		if (argc >= 2)
			ret = iris_test_cmd_input(intf, argc - 1, &argv[1]);
		else
			ret = iris_test_cmd_input(intf, 0, NULL);
	} else {
		ret = iris_test_cmd_input(intf, argc - 2, &argv[2]);
	}

	delete intf;

    return ret;
}
