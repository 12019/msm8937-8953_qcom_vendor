/*
 * Copyright (c) 2015 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 * Confidential and Proprietary - Qualcomm Technologies, Inc.
 */
 
#include "qcosal.h"
#include <malloc.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define LOG_TAG "irisTA"
#include <utils/Log.h>
#include <sys/mman.h>


static os_memory_prof_t osMemoryProf = {0, 0, 0, 0, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, 0};
static int osPrintLog = 1;

// Generic function for both WIN32 and Linux
void os_enable_runtime_log(void)
{
    osPrintLog = 1;
}

void os_disable_runtime_log(void)
{
    osPrintLog = 0;
}

void os_log(int level, const char* fileName, const char* funcName, const int lineNum, const char* format, ...)
{
    char buf[1024];

    va_list argptr;

	if (!osPrintLog && (level < OS_LOG_SEVERE) )
		return;

    va_start(argptr, format);
    vsnprintf(buf, 1024, format, argptr);
    va_end(argptr);
    ALOGE(buf);
}



/*========================================================================
 Function Definitions - Memory
 ========================================================================*/
static void os_counter_crosscheck(os_mem_comp_t *pMemComp)
{
    os_mem_comp_t *pTmpMemComp = pMemComp;

    // reset everything
    if(pTmpMemComp->nAllocCounter == pTmpMemComp->nFreeCounter)
    {
        pTmpMemComp->nAllocCounter = 0;
        pTmpMemComp->nFreeCounter = 0;
        pTmpMemComp->nSize = 0;
    }
}


void* os_memset(void* ptr, unsigned char value, int num)
{
	return memset(ptr, value, num);
}


void* os_malloc(unsigned int size, unsigned int id)
{
    void* pMem;

    pMem = malloc(size);
    if(NULL != pMem)
    {
        osMemoryProf.nTotalAllocCounter += 1;
        osMemoryProf.nTotalAllocSize += size;

        switch (id)
        {
            case OS_ALGO_ID:
                os_counter_crosscheck(&osMemoryProf.sAlgo);
                osMemoryProf.sAlgo.nSize += size;
                osMemoryProf.sAlgo.nAllocCounter += 1;
                break;
            case OS_TESTAPP_ID:
                os_counter_crosscheck(&osMemoryProf.sTestApps);
                osMemoryProf.sTestApps.nSize += size;
                osMemoryProf.sTestApps.nAllocCounter += 1;
                break;
            case OS_CORE_ID:
                os_counter_crosscheck(&osMemoryProf.sCore);
                osMemoryProf.sCore.nSize += size;
                osMemoryProf.sCore.nAllocCounter += 1;
                break;
            default:
                break;
        }
    }

    return pMem;
}


void os_free(void* ptr, unsigned int id)
{
    free(ptr);
    osMemoryProf.nTotalFreeCounter += 1;

    switch (id)
    {
        case OS_ALGO_ID:
            osMemoryProf.sAlgo.nFreeCounter += 1;
            break;
        case OS_TESTAPP_ID:
            osMemoryProf.sTestApps.nFreeCounter += 1;
            break;
        case OS_CORE_ID:
            osMemoryProf.sCore.nFreeCounter += 1;
            break;
        default:
            break;
    }

}

// To retrieve run time memory info
os_memory_prof_t os_get_memory_prof_info()
{
    return osMemoryProf;
}

// To reset run time memory counter
void os_reset_memory_prof_info()
{
    os_memset(&osMemoryProf, 0, sizeof(osMemoryProf));
}

unsigned int os_get_memory_runtime_max()
{
    unsigned int max = 0;

    max = osMemoryProf.sAlgo.nSize;
    if(max < osMemoryProf.sTestApps.nSize)
    {
        max = osMemoryProf.sTestApps.nSize;
    }
    return max;
}

void* os_calloc(unsigned int num, unsigned int size)
{
    void *ptr = malloc(size*num);

    if (ptr)
    {
        os_memset(ptr, 0, size*num);
    }

    return ptr;
}

unsigned int os_memscpy(void *dest, unsigned int dest_size, const void *src, unsigned int copy_size)
{
    uint8_t *d_ptr = 0;
    uint8_t *s_ptr = 0;
    uint8_t *s_end = 0;
    size_t num_to_copy = (dest_size < copy_size) ? dest_size : copy_size;

    // TODO: check for overlap and return NULL? (memmove is supposed to be used if overlap)

    if ((dest == NULL) || (src == NULL) || (num_to_copy <= 0)) {
        return 0;
    }

    // TODO: optimize this
    d_ptr = (uint8_t *) dest;
    s_ptr = (uint8_t *) src;
    s_end = &s_ptr[num_to_copy];

    while (s_ptr < s_end)
    {
        *d_ptr++ = *s_ptr++;
    }

    return num_to_copy;
}

/*========================================================================
 Function Definitions - String
 ========================================================================*/

void os_strncpy(char * dst, const char* src, unsigned int num)
{
    strncpy(dst, src, num);
}

void os_strlcpy(char * dst, const char* src, unsigned int dstsize)
{
    strncpy(dst, src, dstsize);
}

unsigned int os_strlen(const char* str)
{
    return strlen(str);
}

int os_strncmp(const char * str1, const char *str2, unsigned int num)
{
    while(num--)
    {
        if(*(str1++) != *(str2)++)
        {
            return (int) (*(unsigned char*)(str1 - 1) - *(unsigned char*)(str2 - 1));
        }
    }
    return 0;
}


int os_atoi(const char* str)
{
    return atoi(str);
}

float os_atof(const char* str)
{
    return (float)atof(str);
}

/*========================================================================
 Function Definitions - File IO
 ========================================================================*/

os_file_ptr os_open(const char* path, int32_t flags, int32_t secure)
{
    os_file_ptr fp = NULL;
    char fopenMode[10];

    memset(fopenMode, 0, 10);
    // Convert os flags to fopen flags
    os_strlcpy(fopenMode, "w+", 5); // default
    if (flags == OS_O_RDONLY || flags & OS_O_RDONLY) os_strlcpy(fopenMode, "rb", 5);
    if (flags & OS_O_CREAT || flags & OS_O_WRONLY  || flags & OS_O_RDWR) os_strlcpy(fopenMode, "wb", 5);

    if ((path == NULL) || (path[0] == '\0')) {
        OS_LOG_HIGH("filename parameter is null or empty!");
    }
    else if ((fp = (os_file_ptr)os_malloc(sizeof(struct os_file), OS_CORE_ID)) == NULL) {
        OS_LOG_HIGH("out of memory!");
    }
    else
    {
        FILE* fd=0;
        int32_t failed=0;

        if (secure)
        {
            OS_LOG_HIGH("secure file i/o NOT supported on this platform");
            failed = 1;
        }
        else
        {
            fd = fopen(path, fopenMode);
            if (fd == 0)
            {
                OS_LOG_HIGH("nonsecure open() FAILED! returned = %d, errno = %d", fd, errno);
                failed = 1;
            }
        }

        if (failed)
        {
            os_free(fp, OS_CORE_ID);
            fp = NULL;
        }
        else
        {
            fp->fd = (size_t)fd;
            fp->secure = secure;
        }
    }

    return fp;
}


int32_t os_close(os_file_ptr fp)
{
    int status = -1;

    if (fp == NULL)
    {
        OS_LOG_HIGH("file pointer is null!");
    }
    else
    {
        if (fp->secure)
        {
            OS_LOG_HIGH("secure file i/o NOT supported on this platform");
        }
        else {
            status = fclose((FILE *)fp->fd);
        }

        // clear the structure and free the memory
        memset(fp, 0, sizeof(struct os_file));
        os_free(fp, OS_CORE_ID);
    }

    return status;
}

#if WIN32
void *os_map(uint32_t fd, uint32_t length)
{
	OS_LOG("Dummy function: %d %d", fd, length);
	return NULL;
}

void os_unmap(void *ptr, uint32_t length)
{
	OS_LOG("Dummy function: %p %d", ptr, length);
}
#else
void *os_map(uint32_t fd, uint32_t length)
{
	return mmap(NULL, length, PROT_READ  | PROT_WRITE,
			MAP_SHARED, fd, 0);
}

void os_unmap(void *ptr, uint32_t length)
{
	munmap(ptr, length);
}
#endif

int32_t os_get_file_size(os_file_ptr fp)
{
    int32_t size = 0;

    if (fp == NULL)
    {
        OS_LOG_HIGH("file pointer is null!");
    }
    else
    {
        fseek( (FILE *)fp->fd, 0, SEEK_END); 
        size = ftell( (FILE *)fp->fd); // get current file pointer
        fseek( (FILE *)fp->fd, 0, SEEK_SET); // set back to begging
    }

    return size;
}
int32_t os_read(os_file_ptr fp, char *buf, int32_t nbytes)
{
    int status = -1;
    int retVal = 0;

    if (fp == NULL)
    {
        OS_LOG_HIGH("file pointer is null!");
    }
    else if (buf == NULL)
    {
        OS_LOG_HIGH("buf parameter is null!");
    }
    else if (nbytes < 1)
    {
        OS_LOG_HIGH("nbytes < 1");
    }
    else if (fp->secure)
    {
        OS_LOG_HIGH("secure file i/o NOT supported on this platform");
    }
    else
    {
        retVal = fread(buf, nbytes, 1, (FILE *)fp->fd);
        return retVal*nbytes;
    }

    return status;
}

int32_t os_write(os_file_ptr fp, const char *buf, int32_t nbytes)
{
    int status = -1;
    int retVal = 0;

    if (fp == NULL) {
        OS_LOG_HIGH("file pointer is null!");
    }
    else if (buf == NULL)
    {
        OS_LOG_HIGH("buf parameter is null!");
    }
    else if (nbytes < 1)
    {
        OS_LOG_HIGH("nbytes < 1");
    }
    else if (fp->secure)
    {
        OS_LOG_HIGH("secure file i/o NOT supported on this platform");
    }
    else
    {
        retVal = fwrite(buf, nbytes, 1, (FILE *)fp->fd);
        return retVal*nbytes;
    }

    return status;
}

#ifdef WIN32
// Windows Only
#include <sys/timeb.h>

/*========================================================================
 Function Definitions - Timer
 ========================================================================*/

unsigned int os_get_elapsed_milliseconds(void)
{
    struct timeb t;

	ftime(&t);
    return (unsigned int)(t.time * 1000.00 + t.millitm);
}

#else
// Linux Only
unsigned int os_get_elapsed_milliseconds(void)
{
    struct timeval tv;

	gettimeofday(&tv, NULL);
    return (unsigned int)((tv.tv_sec) * 1000 + (tv.tv_usec) / 1000);
}
#endif
