/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */

#ifndef _QC_OSAL_H_
#define _QC_OSAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h> // For vargs
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <limits.h> // for INT_MAX/etc
#include <math.h>
#include <float.h>
#include <errno.h>
#if !WIN32
#include <sys/mman.h>
#endif
/*========================================================================
 Defines
 ========================================================================*/

// For logging
#define OS_LOG_VERBOSE 1
#define OS_LOG_SEVERE  2
#define OS_LOG_MAX     3

// File IO Flags
#define OS_O_RDONLY 0x00000000
#define OS_O_WRONLY 0x00000001
#define OS_O_RDWR   0x00000002
#define OS_O_CREAT  0x00000100
#define OS_O_APPEND 0x00002000

// Memory profiling
#define OS_TESTAPP_ID         1
#define OS_ALGO_ID            2
#define OS_CORE_ID            3

/*========================================================================
 Macros
 ========================================================================*/

// **** Uncomment to enable the profile by default
#define OS_PROFILE_ENABLE
// **** Uncomment to enable the log by default
#define OS_LOG_ENABLE

#ifdef OS_LOG_ENABLE
#define OS_LOG(...) os_log(OS_LOG_VERBOSE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
#else
#define OS_LOG(level, ...)
#endif
#define OS_LOG_HIGH(...) os_log(OS_LOG_SEVERE, __FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

void os_log(int level, const char* fileName, const char* funcName, const int lineNum, const char* format, ...);
void os_enable_runtime_log(void);
void os_disable_runtime_log(void);

#define os_assert(...) assert(__VA_ARGS__)
/*========================================================================
 Data Structures
 ========================================================================*/

typedef struct os_file {
    size_t fd;
    int32_t secure;
} *os_file_ptr;

typedef struct os_memory_comp_t
{
    unsigned int nSize;
    unsigned int nAllocCounter;
    unsigned int nFreeCounter;
} os_mem_comp_t;

typedef struct os_memory_prof_t
{
   unsigned int nTotalAllocSize;
   unsigned int nTotalFreeSize;
   unsigned int nTotalAllocCounter;
   unsigned int nTotalFreeCounter;
   os_mem_comp_t sAlgo;
   os_mem_comp_t sTestApps;
   os_mem_comp_t sCore;
   unsigned int nFlag;
} os_memory_prof_t;

// Memory
void* os_malloc(unsigned int size, unsigned int id);
void* os_calloc(unsigned int num, unsigned int size);
void os_free(void* ptr, unsigned int id);
unsigned int os_memscpy(void *dest, unsigned int dest_size, const void *src, unsigned int copy_size);
void* os_memset(void* ptr, unsigned char value, int num);

// File IO
os_file_ptr os_open(const char* path, int32_t flags, int32_t secure);
int32_t os_close(os_file_ptr fp);
int32_t os_read(os_file_ptr fp, char *buf, int32_t nbytes);
int32_t os_write(os_file_ptr fp, const char *buf, int32_t nbytes);
int32_t os_get_file_size(os_file_ptr fp);
void *os_map(uint32_t fd, uint32_t length);
void os_unmap(void *ptr, uint32_t length);

// Timer
unsigned int os_get_elapsed_milliseconds(void);

#ifdef OS_PROFILE_ENABLE
#define INIT_PROFILE \
	int start_time; \
	int end_time;

#define START_PROFILE(str) \
	start_time = os_get_elapsed_milliseconds(); \
	OS_LOG_HIGH("%s profile start\n", str);

#define END_PROFILE(str) \
	end_time = os_get_elapsed_milliseconds(); \
	OS_LOG_HIGH("%s profile end: %dms\n", str, end_time - start_time);

#else
#define INIT_PROFILE
#define START_PROFILE(str)
#define END_PROFILE(str)
#endif

// String related
void os_strncpy(char * dst, const char* src, unsigned int num);
void os_strlcpy(char * dst, const char* src, unsigned int dstsize);
unsigned int os_strlen(const char* str);
int os_strncmp(const char * str1, const char *str2, unsigned int num);

// Misc
void os_itoa(int num, char* outputString, int digitsRequired);
void os_ftoa(float floatVal, char* outputString);
int os_atoi(const char* str);
float os_atof(const char* str);

#ifdef __cplusplus
}
#endif

#endif//_QC_OSAL_H_
