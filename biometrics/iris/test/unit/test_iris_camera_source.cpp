/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#include "iris_camera_source.h"

using namespace android;

static void dump_frame(iris_frame *frame, char* filename)
{
    FILE *f;
    void *ret;
    int bufferFD = frame->frame_handle;

    printf("buffer FD %d len %x\n", bufferFD, frame->frame_len);
    ret = mmap(NULL,
        frame->frame_len,
        PROT_READ  | PROT_WRITE,
        MAP_SHARED,
        bufferFD,
        0);

    if (ret == MAP_FAILED) {
        printf("mmap failed\n");
        return;
    }

    f = fopen(filename, "wb");

    if (f == NULL) {
        printf("%s: fopen failed: %d\n", __func__, errno);
        return;
    }

    printf("dump file %p size %d to file %s\n", ret, frame->frame_len, filename);
    size_t n = fwrite(ret, frame->frame_len, 1, f);
    if (n != 1) {
        printf("%s: fwrite failed size %d (%u): %d\n", __func__, frame->frame_len, (unsigned)n, errno);
        return;
    }
    munmap(ret, frame->frame_len);

    fclose(f);
}

#define TEST_OP_MODE IRIS_CAMERA_OP_MODE_PREVIEW

int main(int argc, char **argv)
{
    status_t ret;
    int fd;
    Vector<Size> sizes;
    Size size;
    IrisCameraSource *camera;
    struct iris_frame_info info;
    struct iris_frame frame;
    int i, opt, camId = 0;
    String16 packageName("test");
    int dumpFrameCount = 1;
    char fileName[256];
    uint32_t sensor_gain = 50, sensor_exp_time = 8;
    bool nr_on = false;

    size.width = 1920;
    size.height = 1920;

    while ((opt = getopt(argc, argv, "i:c:e:g:w:h:n")) != -1) {
        switch (opt) {
        case 'i':
           camId = atoi(optarg);
           break;
        case 'c':
           dumpFrameCount = atoi(optarg);
           break;
        case 'g':
           sensor_gain = atoi(optarg);
           break;
        case 'e':
           sensor_exp_time = atoi(optarg);
           break;
        case 'w':
            size.width = atoi(optarg);
            break;
        case 'h':
            size.height = atoi(optarg);
            break;
        case 'n':
            nr_on = true;
            break;
        default: /* '?' */
           fprintf(stderr, "Usage: %s [-i cameraId] [-c frameCount] [-g sensor gain] [-e sensor exposure time(ms)] [-w preview_width] [-h preview_height] [-n]\n",
                   argv[0]);
           exit(EXIT_FAILURE);
        }
    }

    printf("dump %d frames\n", dumpFrameCount);

    // Open camera
    camera = new IrisCameraSource(camId);
    if (camera == NULL) {
        printf("Camera init failed\n");
        return -1;
    }

    // Start camera data flow
    ret = camera->open(0, packageName);
	if (ret != OK) {
		printf("Cannot open camera!");
		return -1;
    }

    camera->set_op_mode(IRIS_CAMERA_OP_MODE_PREVIEW);
    camera->set_op_size(IRIS_CAMERA_OP_MODE_PREVIEW, size);
    printf("Select preview size %dx%d\n", size.width, size.height);

    printf("Set sensor gain %d exposure time %d ms\n", sensor_gain, sensor_exp_time);
    camera->set_sensor_gain(sensor_gain);
    camera->set_sensor_exposure_time(sensor_exp_time);
    camera->set_nr_mode(nr_on);

    camera->start();

    camera->get_source_info(info);
    printf("width %d height %d\n", info.width, info.height);
    // Wait for enter
    printf("Press enter\n");

    char dummybuffer[16];
    fgets(dummybuffer, 15, stdin);

    for (i = 0; i < dumpFrameCount; i++) {
        ret = camera->get_frame(frame);
        if (ret != OK)
            printf("Can't get frame\n");
        else {
            printf("Get one frame width %d height %d fd %d len %d\n", frame.info.width, frame.info.height, frame.frame_handle, frame.frame_len);

            sprintf(fileName, "/sdcard/test_iris_camera_dump_%d.bin", i);
            dump_frame(&frame, fileName);

            printf("put frame\n");
            camera->put_frame(frame);
        }

        usleep(100000);
    }
    // Stop camera data flow
    printf("stop camera\n");
    camera->stop();
    printf("close camera\n");
    camera->close();
    printf("delete camera\n");
    delete camera;
    printf("shutdown: pass\n");

    return 0;
}

