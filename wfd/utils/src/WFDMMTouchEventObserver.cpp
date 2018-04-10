/* =======================================================================
                              WFDMMTouchEventObserver.cpp
DESCRIPTION

This module is for capturing touch events from the input device node. It does
not actually interpret the inout events but just provides a way of receiving
notifications on input events.

Copyright (c) 2015,2016 Qualcomm Technologies, Inc.
All Rights Reserved.
Confidential and Proprietary - Qualcomm Technologies, Inc.
========================================================================== */


/* =======================================================================
                             Edit History
   When            Who           Why
-----------------  ------------  -----------------------------------------------
17/03/2015                       InitialDraft
========================================================================== */

/*========================================================================
 *                             Include Files
 *==========================================================================*/
#include <fcntl.h>
#include <sys/poll.h>
#include <linux/input.h>
#include <errno.h>

#include "MMMemory.h"
#include "MMTime.h"
#include "MMTimer.h"
#include "WFDMMLogs.h"
#include "WFDMMThreads.h"
#include "WFDMMSourceSignalQueue.h"
#include "WFDMMTouchEventObserver.h"
#include "QServiceUtils.h"
#include "display_config.h"
#include <dirent.h>
#include <sys/inotify.h>
#include "wfd_cfg_parser.h"
#include <cutils/properties.h>

/*========================================================================
 *                             Macro Definitons
 *==========================================================================*/

#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "WFDMMTEO"

#define LOCK_MUTEX do {\
        if(pthread_mutex_lock(&sTEOLock))\
        {\
            WFDMMLOGE1("Failed to acquire mutex due to %s", strerror(errno));\
        }\
    }while(0);


#define UNLOCK_MUTEX do {\
        if(pthread_mutex_unlock(&sTEOLock))\
        {\
            WFDMMLOGE1("Failed to acquire mutex due to %s", strerror(errno));\
        }\
    }while(0);

#ifndef WFD_RETRY_TEMP_FAILURE
/* Used to retry system calls that can return EINTR. */
#define WFD_RETRY_TEMP_FAILURE(exp) ({     \
    typeof (exp) _rc;                      \
    do {                                   \
        _rc = (exp);                       \
    } while (_rc == -1 && errno == EINTR); \
    _rc; })
#endif

/*-----------------------------------------------------------------------------
 Critical Sections
 ------------------------------------------------------------------------------
*/
static pthread_mutex_t sTEOLock = PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP;

/*-----------------------------------------------------------------------------
 The array has to be a byte array
-------------------------------------------------------------------------------
*/

#define testBit(bit, array) (array[bit>>3] & (1<<(bit&7)))

static const char *INPUT_DEV_DIR = "/dev/input";
#define POLL_TIMEOUT 500 //ms
#define IP_EV_TIMEOUT 10000 //10 seconds
#define INOTIFY_SLURP_COUNT 100

/*========================================================================
 *                             Enums, structs etc.
 *==========================================================================*/

enum {
    POLL_FOR_INPUT,
    STOP_POLLING,
    MAX_NO_SIGNALS
};

/*=============================================================================

         FUNCTION:           supportTEO

         DESCRIPTION:
*//**       @brief           Helper to query a debug property and disable TEO
                             in case the debug property is set to 0
*//**
@par     DEPENDENCIES:
                             None
*//*
         PARAMETERS:
*//*         @param[in]      NONE

*//*     RETURN VALUE:
*//**       @return
                             whether TEO is supported based on debug prop
@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

static inline bool supportTEO()
{
    int ret = 0;
    char szTemp[PROPERTY_VALUE_MAX];
    const char* propString = "persist.debug.wfd.teo";
    ret = property_get(propString,szTemp,NULL);
    if (ret > 0)
    {
        if(strncmp(szTemp,"0", sizeof("0"))==0)
        {
            WFDMMLOGH1("%s is set to 0, disabling TEO",propString);
            return false;
        }
    }
    return true;//Always default to true
}
/*=============================================================================

         FUNCTION:          WFDMMTouchEventObserver

         DESCRIPTION:
*//**       @brief          Ctor for WFDMMTouchEventObserver
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
WFDMMTouchEventObserver::WFDMMTouchEventObserver(TEOUserProfile prof):
m_hThread(NULL),
m_nScreenTimeout(IP_EV_TIMEOUT),
m_nPollTimeout(POLL_TIMEOUT),
m_nBrightnessCache(0),
m_bPollForData(false),
m_bDisplayOn(true),
m_bWFDHIDFound(false),
m_eUserProfile(prof),
m_pPollFds(NULL),
m_nPollFds(0),
m_nWatchFd(-1),
m_pDeviceNames(NULL),
m_pCmdQ(NULL)
{
    WFDMMLOGE("Hola from WFDMMTouchEventObserver\n");
}

/*=============================================================================

         FUNCTION:          ~WFDMMTouchEventObserver

         DESCRIPTION:
*//**       @brief          Dtor for WFDMMTouchEventObserver
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
WFDMMTouchEventObserver::~WFDMMTouchEventObserver()
{
    stop();
    WFDMMLOGH("Done with ~WFDMMTouchEventObserver. Adios!\n");
}

/*=============================================================================

         FUNCTION:          scanDirectory

         DESCRIPTION:
*//**       @brief          scans the directory
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          dirname Name of the directory to be scanned

*//*     RETURN VALUE:
*//**       @return         0 on success, -1 on failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
int WFDMMTouchEventObserver::scanDirectory(const char *dirname)
{
    if(dirname)
    {
        char devName[PATH_MAX];
        memset(devName,0,sizeof(devName));
        DIR *dir = opendir(dirname);
        if(dir == NULL)
        {
            WFDMMLOGE2("Failed to open dir %s due to %s\n", dirname,
                strerror(errno));
            return -1;
        }

        size_t initSize = sizeof(devName);
        strlcpy(devName, dirname,sizeof(devName));
        size_t initLen = strlen(devName);
        if(initSize == initLen + 1)//devName buffer is at max capacity!
        {
            WFDMMLOGE1("Can't work on %s due to excess size\n",devName);
            closedir(dir);
            return -1;
        }

        char *filename;
        /*------------------------------------------------------------------
         Move to the end of the directory name
         -------------------------------------------------------------------
         */
        filename = devName + initLen;
        *filename++ = '/';

        /*------------------------------------------------------------------
         Path now should look like '/dev/input/'
         Now iterate over the directory tree
         -------------------------------------------------------------------
         */

        errno = 0;
        struct dirent *de;
        while((de = readdir(dir)))
        {
            if(de->d_name[0] == '.' &&
               (de->d_name[1] == '\0' ||
                (de->d_name[1] == '.' && de->d_name[2] == '\0')))
                continue;//Ignore . and .. entries
            strlcpy(filename, de->d_name,sizeof(devName) - (initLen+1));

            if (0 == managePollFd(devName, sizeof(devName), true))
            {
                WFDMMLOGH1("Added %s for input polling\n",devName);
            }
        }
        closedir(dir);
        return 0;
    }
    return -1;
}

/*=============================================================================

         FUNCTION:          checkDevice

         DESCRIPTION:
*//**       @brief          Checks whether a device is worthy of polling for
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          devName Name of the device
                            fd file descriptor of the device

*//*     RETURN VALUE:
*//**       @return         0 if device should be polled for
                            1 if the device isn't interesting enough

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

int WFDMMTouchEventObserver::checkDevice(const char * devName, int fd)
{
    char name[NAME_MAX];
    bool isInteresting = false;
    /*-------------------------------------------------------------------------
       Try to get the device name (just for kicks!)
    ---------------------------------------------------------------------------
    */

    name[sizeof(name) - 1] = '\0';
    if(ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name) < 1) {
        WFDMMLOGE2("Could not get device name for %s, %s\n", devName,
            strerror(errno));//Nothing fatal
        name[0] = '\0';
    }
    else
    {
        WFDMMLOGH1("Device name is %s\n",name);
    }

    /*-------------------------------------------------------------------------
       Try to get the events supported by the device. The ioctl works like this
       We supply 0 to get all events supported
    ---------------------------------------------------------------------------
    */

    uint8_t evtypeBitmask[(EV_MAX + 7)>>3] = {0};

    if (ioctl(fd, EVIOCGBIT(0, EV_MAX), evtypeBitmask) < 0)
    {
        WFDMMLOGE2("Could not get device events for %s due to %s\n", devName,
            strerror(errno));
        return 1;
    }

    /*-------------------------------------------------------------------------
       Now test the events for EV_KEY
    ---------------------------------------------------------------------------
    */

    uint8_t suppKeyBitmask[(KEY_MAX + 7)>>3] = {0};

    if (testBit(EV_KEY, evtypeBitmask))
    {
        if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(suppKeyBitmask)),
            suppKeyBitmask)<0)
        {
            WFDMMLOGE2("EVIOCGBIT failed for %s due to %s\n",
                devName, strerror(errno));
            return 1;
        }

        /*----------------------------------------------------------------------
           Now test for interesting features supported. This is the core part
        ------------------------------------------------------------------------
        */

        /*----------------------------------------------------------------------
           Test the device for BTN_TOUCH & BTN_TOOL_FINGER
        ------------------------------------------------------------------------
        */
        if (testBit(BTN_TOUCH, suppKeyBitmask) ||
            testBit(BTN_TOOL_FINGER, suppKeyBitmask))
        {
            WFDMMLOGE1("%s supports touch\n",devName);
            isInteresting = true;
        }
        /*----------------------------------------------------------------------
           Test the device for any alphabet key to determine if it's a keyboard
        ------------------------------------------------------------------------
        */
        else if(testBit(KEY_Z,suppKeyBitmask))
        {
            WFDMMLOGE1("%s supports keys\n",devName);
            isInteresting = true;
            if(strncmp(name,"wfd-uhid-device",sizeof("wfd-uhid-device")) == 0) {
               WFDMMLOGE1("%s Device found\n",name);
               m_bWFDHIDFound = true;
            }
        }
    }
    return isInteresting ? 0:1;
}

/*=============================================================================

         FUNCTION:          handleDirChange

         DESCRIPTION:
*//**       @brief          handle directory change events
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          dirname Name of directory where changes occured

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/

void WFDMMTouchEventObserver::handleDirChange(const char *dirname)
{
    char devName[PATH_MAX];
    memset(devName, 0, sizeof(devName));

    static const size_t EVENT_BUF_SIZE = INOTIFY_SLURP_COUNT * (sizeof(inotify_event) + NAME_MAX);
    char eventBuf[EVENT_BUF_SIZE];
    memset(eventBuf, 0, sizeof(eventBuf));

    int eventPos = 0;
    struct inotify_event *event;
    int res = WFD_RETRY_TEMP_FAILURE(read(m_pPollFds[0].fd, eventBuf,
        sizeof(eventBuf)));
    if(res < 0)
    {
        WFDMMLOGE1("Could not read inotify event due to %s\n", strerror(errno));
        return;
    }

    const size_t initSize = sizeof(devName);
    strlcpy(devName, dirname,sizeof(devName));
    size_t initLen = strlen(devName);
    if(initSize == initLen + 1)//devName buffer is at max capacity!
    {
        WFDMMLOGE1("Can't work on %s due to excess size\n",devName);
        return;
    }

    /*------------------------------------------------------------------
     Move to the end of the directory name
     -------------------------------------------------------------------
     */
    char *filename = devName + initLen;
    *filename++ = '/';

    /*------------------------------------------------------------------
     Path now should look like '/dev/input/'
     -------------------------------------------------------------------
     */

    while(res >= (int)sizeof(*event))
    {
        event = (struct inotify_event *)(eventBuf + eventPos);

        const int eventSize = sizeof(*event) + event->len;
        res -= eventSize;
        eventPos += eventSize;

        if (eventPos > EVENT_BUF_SIZE)
        {
          // filename goes beyond the allocated buffer size
          MM_MSG_PRIO2(MM_GENERAL, MM_PRIO_ERROR,
                       "Insufficient buffer size in handleDirChange eventPos=%d > EVENT_BUF_SIZE=%d",
                       eventPos, (int)EVENT_BUF_SIZE);
          break;
        }

        if(event->len > 0)
        {
            strlcpy(filename, event->name,sizeof(devName) - (initLen+1));
            if(event->mask & IN_CREATE)
            {
                WFDMMLOGH1("Received a create event for %s\n", devName);
                if (0 == managePollFd(devName, sizeof(devName), true))
                {
                    WFDMMLOGH1("Added %s for input polling\n",devName);
                }
            }
            else if(event->mask & IN_DELETE)
            {
                WFDMMLOGH1("Received delete event for %s\n", devName);
                managePollFd(devName, sizeof(devName), false);
            }
        }
        else
        {
            WFDMMLOGE("HandleDirChange Received unexpected event len 0");
        }
    }
}

/*=============================================================================

         FUNCTION:          createResources

         DESCRIPTION:
*//**       @brief          create threads & queues for WFDMMTouchEventObserver
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            0 on success 1 otherwise
@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
int WFDMMTouchEventObserver::createResources()
{

    if (managePollFd(NULL, 0, true) < 0)
    {
        return 1;
    }

    int timeout = 0;
    getCfgItem(DS_SCREEN_TIMEOUT_KEY,&timeout);
    if(timeout > 0)
    {
        m_nScreenTimeout = timeout*1000; //in ms
        WFDMMLOGH1("Setting TEO timeout to %d\n", m_nScreenTimeout);
    }

    scanDirectory(INPUT_DEV_DIR);

    if(!m_pCmdQ)
    {
        m_pCmdQ = MM_New_Args(SignalQueue,(100, sizeof(wfdUtils::TEOMsg)));

        if(!m_pCmdQ)
        {
            WFDMMLOGE("Failed to create Command Q\n");
            return 1;
        }

    }

    if(!m_hThread)
    {
        m_hThread = MM_New_Args(WFDMMThreads,(MAX_NO_SIGNALS));

        if(!m_hThread)
        {
            WFDMMLOGE("Failed to create IEO thread\n");
            return 1;
        }

        m_hThread->Start(eventObserverCb,0,32768,this,NULL);

    }

    return 0;

}

/*=============================================================================

         FUNCTION:          releaseResources

         DESCRIPTION:
*//**       @brief          frees resources of WFDMMTouchEventObserver
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return
                            None
@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
void WFDMMTouchEventObserver::releaseResources()
{
    /*------------------------------------------------------------------
     The thread needs to always go first, then release other resources
     -------------------------------------------------------------------
     */
    if(m_hThread)
    {
        m_bPollForData = false;
        MM_Delete(m_hThread);
        m_hThread = NULL;
    }

    if(m_pPollFds)
    {
        /*------------------------------------------------------------------
         First remove the inotify watch
         -------------------------------------------------------------------
         */
        if(inotify_rm_watch(m_pPollFds[0].fd,m_nWatchFd) < 0)
        {
            WFDMMLOGE1("Failed to remove watch due to %s\n", strerror(errno));
            //Don't return and spoil the "free" binge.
        }
        WFD_RETRY_TEMP_FAILURE(close(m_pPollFds[0].fd));
        for(int i = 1; i < m_nPollFds; ++i)
        {
            WFD_RETRY_TEMP_FAILURE(close(m_pPollFds[i].fd));
        }
        MM_Free(m_pPollFds);
        m_pPollFds = NULL;
    }

    if(m_pDeviceNames)
    {
        for(int i = 0; i < m_nPollFds; ++i)
        {
            if(m_pDeviceNames[i])
            {
                MM_Free(m_pDeviceNames[i]);
                m_pDeviceNames[i] = NULL;
            }
        }
        MM_Free(m_pDeviceNames);
        m_pDeviceNames = NULL;
    }

    if(m_pCmdQ)
    {
        MM_Delete(m_pCmdQ);
        m_pCmdQ = NULL;
    }
}

/*=============================================================================

         FUNCTION:          managePollFd

         DESCRIPTION:
*//**       @brief          Adds/removes file descriptors to be polled for
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
             @param         devName The path of the fd to be opened/closed
             @param         add     Boolean indicating whether file fd has to
                            be added/removed for polling by the poll thread


*//*     RETURN VALUE:
*//**       @return
                            0 on success, -1 on failure
@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
int WFDMMTouchEventObserver::managePollFd(const char* devName, size_t devNameSize, bool add)
{
    if(!m_pPollFds && !m_pDeviceNames)
    {
        if(devName == NULL && add)//Special case for inotify
        {
            m_pPollFds = static_cast<pollfd*>
                (MM_Calloc(1,sizeof(m_pPollFds[0])));
            if(!m_pPollFds)
            {
                WFDMMLOGE("Failed to create pollfd structure\n");
                return -1;
            }
            m_pPollFds[0].fd = inotify_init();
            if(m_pPollFds[0].fd < 0)
            {
                WFDMMLOGE1("inotify_init() failed due to %s\n", strerror(errno));
                return -1;
            }
            m_pPollFds[0].events = POLLIN;
            if((m_nWatchFd = inotify_add_watch(m_pPollFds[0].fd,INPUT_DEV_DIR,
                IN_DELETE | IN_CREATE)) < 0)
            {
                WFDMMLOGE("Failed to add watch to inotify!\n");
                return -1;
            }
            WFDMMLOGH("Succesfully set up inotify watches\n");
            m_nPollFds = 1 ;
        }
        else
        {
            WFDMMLOGE("Can't add fd because enough stuff doesn't exist\n");
            return -1;
        }
    }
    else if(m_pPollFds)
    {
        if(add && devName != NULL)
        {
            int fd = WFD_RETRY_TEMP_FAILURE(open(devName, O_RDONLY));

            if(fd < 0)
            {
                WFDMMLOGE2("Can't open %s due to %s\n", devName, strerror(errno));
                return -1;
            }

            if(0 != checkDevice(devName,fd))
            {
                WFDMMLOGH1("%s isn't worth polling for\n",devName);
                WFD_RETRY_TEMP_FAILURE(close(fd));
                return -1;
            }

            struct pollfd* newFds = static_cast<pollfd*>
                (MM_Realloc(m_pPollFds,sizeof(m_pPollFds[0])*(m_nPollFds +1)));
            if(!newFds)
            {
                WFDMMLOGE("Failed to re-allocate new poll fds!\n");
                WFD_RETRY_TEMP_FAILURE(close(fd));
                return -1;
            }
            m_pPollFds = newFds;

            char** new_device_names = static_cast<char**>
                (MM_Realloc(m_pDeviceNames,
                    sizeof(m_pDeviceNames[0]) * (m_nPollFds+ 1)));
            if(!new_device_names)
            {
                WFDMMLOGE("Failed to re-allocate new device names!\n");
                WFD_RETRY_TEMP_FAILURE(close(fd));
                return -1;
            }
            m_pDeviceNames = new_device_names;

            /*-----------------------------------------------------------------
             strdup would have served the purpose but MM_OSAL wouldn't have
             tracked it and while freeing would have caused problems. Grump :(
             ------------------------------------------------------------------
             */
            m_pDeviceNames[0] = NULL;//inotify needs no name

            m_pDeviceNames[m_nPollFds] = static_cast<char*>
                (MM_Calloc(1,strlen(devName) + 1));
            if(!m_pDeviceNames[m_nPollFds])
            {
                WFDMMLOGE("Failed to allocate device name!\n");
                WFD_RETRY_TEMP_FAILURE(close(fd));
                return -1;
            }
            strlcpy(m_pDeviceNames[m_nPollFds],devName,strlen(devName) + 1);
            /*-----------------------------------------------------------------
             Everything is allocated now, increment the managed fd count
             ------------------------------------------------------------------
             */
            m_pPollFds[m_nPollFds].fd = fd;
            m_pPollFds[m_nPollFds].events = POLLIN;
            m_nPollFds++;
        }
        else if(!add)//fd is being removed from poll management
        {
            for(int i = 1; i< m_nPollFds; ++i)
            {
                if(strncmp(m_pDeviceNames[i],devName, devNameSize) == 0)
                {
                    int newCount = m_nPollFds - (i +1);
                    MM_Free(m_pDeviceNames[i]);
                    m_pDeviceNames[i] = NULL;
                    WFD_RETRY_TEMP_FAILURE(close(m_pPollFds[i].fd));
                    memmove(m_pPollFds + i, m_pPollFds + i +1,
                        sizeof(m_pPollFds[0])*newCount);
                    memmove(m_pDeviceNames + i, m_pDeviceNames + i +1,
                        sizeof(m_pDeviceNames[0])*newCount);
                    m_nPollFds--;
                    WFDMMLOGH1("Removed %s for polling\n", devName);
                    break;
                }
            }
        }
    }
    else
    {
        WFDMMLOGE1("Invalid state to call %s\n", __FUNCTION__);
        return -1;
    }
    return 0;
}

/*=============================================================================

         FUNCTION:          start

         DESCRIPTION:
*//**       @brief          Triggers a thread to poll for input events on the
                            input device node
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         0 on success, 1 on failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
int WFDMMTouchEventObserver::start()
{

    LOCK_MUTEX;

    if(!supportTEO())
    {
        WFDMMLOGE("TEO is disabled\n");
        UNLOCK_MUTEX;
        return 0;//Let client remain in blissful oblivion
    }

    if(m_bPollForData)
    {
        WFDMMLOGE("Thread already polling for input events\n");
        UNLOCK_MUTEX;
        return 0;
    }

    m_bPollForData = true;

    if(0!= createResources())
    {
        WFDMMLOGE("Not enough stuff to proceed\n");
        UNLOCK_MUTEX;
        return 1;
    }

    if(m_hThread)
    {
        m_hThread->SetSignal(POLL_FOR_INPUT);
    }

    UNLOCK_MUTEX;
    return 0;
}

/*=============================================================================

         FUNCTION:          poke

         DESCRIPTION:
*//**       @brief          Change the TEO module's internals
*//**
@par     DEPENDENCIES:
                            Should be called only after TEO module has been
                            started
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         0 on success, other on failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
int WFDMMTouchEventObserver::poke(wfdUtils::TEOMsg& msg)
{
    if(m_pCmdQ)
    {
        return static_cast<int>(m_pCmdQ->Push(static_cast<OMX_PTR>(&msg),
        sizeof(msg)));
    }
    return -1;
}

bool WFDMMTouchEventObserver::isWFDHIDDeviceFound()
{
    int count = 100;

    while (!m_bWFDHIDFound)
    {
        MM_Timer_Sleep(2);
        count--;
    }

    return (m_bWFDHIDFound);
}
/*=============================================================================

         FUNCTION:          stop

         DESCRIPTION:
*//**       @brief          Triggers the poll thread to stop polling for input
                            events on the input device node
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         0 on success, 1 on failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
int WFDMMTouchEventObserver::stop()
{
    LOCK_MUTEX;

    if(!m_bPollForData)
    {
        WFDMMLOGE("IEO Thread wasn't polling for input events\n");
        UNLOCK_MUTEX;
        return 0;
    }

    m_bPollForData = false;

    if(m_hThread)
    {
        m_hThread->SetSignal(STOP_POLLING);
    }

    releaseResources();

    UNLOCK_MUTEX;

    return 0;
}

/*=============================================================================

         FUNCTION:          turnDisplayOn

         DESCRIPTION:
*//**       @brief          Turns the display on/off
*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          turnOn boolean to indicate to turn display on/off

*//*     RETURN VALUE:
*//**       @return         0 on success, 1 on failure

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
int inline WFDMMTouchEventObserver::turnDisplayOn(bool turnOn)
{
    if(turnOn)
    {
        if(!m_bDisplayOn)
        {
#ifdef DISPLAY_DS_APIS_AVAILABLE
            WFDMMLOGM1("Restore Brightness val at Display ON = %d",
                        m_nBrightnessCache);
            qdutils::setPanelBrightness(m_nBrightnessCache);
#endif
            toggleScreenUpdate(true);
            m_bDisplayOn = true;
        }
    }
    else
    {
        if(m_bDisplayOn)
        {
#ifdef DISPLAY_DS_APIS_AVAILABLE
            m_nBrightnessCache = qdutils::getPanelBrightness();
            WFDMMLOGM1("Brightness val at Display OFF = %d",
                        m_nBrightnessCache);
            qdutils::setPanelBrightness(0);
#endif
            toggleScreenUpdate(false);
            m_bDisplayOn = false;
        }
    }
    return 0;
}

/*=============================================================================

         FUNCTION:          eventObserverCb

         DESCRIPTION:
*//**       @brief          Entry point for eventObserver thread

*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
void WFDMMTouchEventObserver::eventObserverCb(
    void* pClientData,
    unsigned int signal)
{
    signal;
    if(!pClientData)
    {
        WFDMMLOGE1("Invalid client data in Thread callback %s\n", __FUNCTION__);
        return;
    }
    static_cast<WFDMMTouchEventObserver*>(pClientData)->eventObserver(signal);
}

/*=============================================================================

         FUNCTION:          eventObserver

         DESCRIPTION:
*//**       @brief          This function is where the eventObserver thread
                            keeps on polling the inpute device nodes to get
                            notified of i/p events. The logic isn't very subtle
                            given that on receiving an i/p event, it checks for
                            the display status as to whether it was on/off. If
                            it was off, then it switches on the display.

                            Similarly, if there is no inout event for 10s, then
                            it turns the display off, if it was on.

*//**
@par     DEPENDENCIES:
                            None
*//*
         PARAMETERS:
*//**       @param          None

*//*     RETURN VALUE:
*//**       @return         None

@par     SIDE EFFECTS:
                            None
*//*=========================================================================*/
int WFDMMTouchEventObserver::eventObserver(int signal)
{
    switch(signal)
    {
        case POLL_FOR_INPUT:
        {
            WFDMMLOGH("Received POLL_FOR_INPUT signal\n");
            struct input_event event;
            int ret = -1;
            unsigned long lastInpEvTime, currTime;
            MM_Time_GetTime(&currTime);
            lastInpEvTime = currTime;
            OMX_ERRORTYPE result = OMX_ErrorUndefined;
            while(m_bPollForData)
            {
                /*------------------------------------------------------
                 Check if client has requested internal tweaks
                 -------------------------------------------------------
                 */
                wfdUtils::TEOMsg msg;
                while(m_pCmdQ->GetSize())
                {
                    if((result = m_pCmdQ->Pop(&msg, sizeof(msg), 1))
                        == OMX_ErrorNone)
                    {
                        if(msg.cmd == wfdUtils::CHANGE_SCREEN_TIMEOUT_VAL)
                        {
                            m_nScreenTimeout = msg.value;
                            WFDMMLOGH1("Changed screen timeout to %ld ms\n",
                                msg.value);
                        }
                        else if(msg.cmd == wfdUtils::CHANGE_POLL_TIMEOUT_VAL)
                        {
                            m_nPollTimeout = msg.value;
                            WFDMMLOGH1("Changed poll timeout to %d ms\n",
                                static_cast<int>(msg.value));
                        }
                    }
                    else
                    {
                        WFDMMLOGH1("Failed to pop off command from cmdQ with "\
                               " result = %d\n",result);
                    }
                }

                ret = poll(m_pPollFds, m_nPollFds, m_nPollTimeout);
                if (ret < 0)
                {
                    WFDMMLOGE1("Cannot poll for fd due to %s\n",
                        strerror(errno));
                    if(errno != EINTR) {
                       break;
                    }
                }

                MM_Time_GetTime(&currTime);

                if(m_pPollFds[0].revents & POLLIN)//dir elements got changed
                {
                    handleDirChange(INPUT_DEV_DIR);
                    /*----------------------------------------------------------
                     We turn on the display temporarily for user to receive
                     notifications regarding new device changes.
                     -----------------------------------------------------------
                     */
                     lastInpEvTime = currTime;
                     if (m_eUserProfile == DS)
                        turnDisplayOn(true);
                }

                for(int i = 1; i < m_nPollFds; ++i)
                {
                    if (m_pPollFds[i].revents & POLLIN)
                    {
                        lastInpEvTime = currTime;
                        ret = WFD_RETRY_TEMP_FAILURE(read(m_pPollFds[i].fd, &event,
                                        sizeof(event)));

                        if(ret < (int)sizeof(event))
                        {
                            WFDMMLOGE1("Cannot read due to %s\n",
                                strerror(errno));
                            continue; //Soldier on
                        }
                        else
                        {
                            WFDMMLOGL3("Read event of type %x code %x value %x"\
                                "\n", event.type, event.code, event.value);
                            if (m_eUserProfile == DS)
                                turnDisplayOn(true);
                        }
                    }
                }

                /*--------------------------------------------------------------
                 Check whether after iterating over all polled fds we received
                 any input events from the devices (lastInpEvTime will have been
                 set, if so). If no, then turn off the display.
                 ---------------------------------------------------------------
                 */

                if (currTime - lastInpEvTime >= m_nScreenTimeout)
                {
                    if (m_eUserProfile == DS)
                        turnDisplayOn(false);
                }
            }
        }
        break;
        case STOP_POLLING:
            WFDMMLOGH("Received STOP_POLLING signal\n");
            /*------------------------------------------------------------------
             Never leave the display in limbo in cases where this module has
             switched off the display but it's internal logic never triggered
             the display to switch it on. On exit always ensure that display is
             in ON state.
             -------------------------------------------------------------------
             */
             if (m_eUserProfile == DS)
                 turnDisplayOn(true);
            break;
        default:
            WFDMMLOGE("How did I reach here?\n");
            break;
    }
    return 0;
}
