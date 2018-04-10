/*******************************************************************************

 Copyright (c) 2015 Qualcomm Technologies, Inc.  All Rights Reserved.
 Qualcomm Technologies Proprietary and Confidential.

 ---------------------------------------------------------------------------

 @file    qti-game_trigger.cpp
 @brief   Android game trigger library
 DESCRIPTION

 ---------------------------------------------------------------------------
*******************************************************************************/

#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dlfcn.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <dirent.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#define LOG_TAG "ANDR-PERF-LOGS"
#include <cutils/log.h>
#include <cutils/properties.h>
#include "mp-ctl.h"

#define GAME_TRIGGER_PROPERTY "sys.games.gt.prof"
#define WHITELISTED_APPS_XML "/system/etc/whitelistedapps.xml"
#define MAX_APPINFO_LEN 1024
#define LOOP_COUNT_SET_COLOCATE_SCHED_GRP_ID 2
#define MAX_MSG_APP_NAME_LEN 128
#define GAMEDET_EVENT_FGAPPSTART  1
#define GAMEDET_EVENT_FGAPPRESUME 2
#define GAMEDET_EVENT_FGAPPPAUSE  3
#define GAMEDET_EVENT_FGAPPSTOP   4

#define MAX_MSG_APP_NAME_SIZE 128
#define GAMED_VERSION             1
#define GAMED_SHIFT_BIT_VERSION   4
#define GAME_SOCKET "/dev/socket/gamed"

typedef struct gamedet_msg {
    int opcode;
    char name[MAX_MSG_APP_NAME_LEN];
}gamedet_msg;

using namespace std;

/* Should match with the values defined at ActivityTrigger.java */
#define FLAG_OVERRIDE_RESOLUTION 1

/* Whitelists for the following features are currently in place */
enum {
    OVERRIDE_RES_WHITELIST = 0,
    GAME_TRIGGER_WHITELIST,
    GAME_COLOCATE_WHITELIST,
    ANIM_TRIGGER_WHITELIST,
    NET_TRIGGER_WHITELIST
};

class GamedClient {
    private:
        //prev app name
        static GamedClient *mGamedClient;
        char mPrevApp[MAX_MSG_APP_NAME_LEN];
        char mCurrentApp[MAX_MSG_APP_NAME_LEN];
        GamedClient();
        GamedClient(GamedClient const& rh);
        GamedClient& operator=(GamedClient const& rh);

    public:
        ~GamedClient();
        static GamedClient *getInstance() {
            if (NULL == mGamedClient) {
                mGamedClient = new GamedClient();
            }
            return mGamedClient;
        }
    public:
        int game_client_send_cmd(struct gamedet_msg msg);
        int NotifyFgAppChange(unsigned int state,const char *name);
};
GamedClient *GamedClient::mGamedClient = NULL;
GamedClient::~GamedClient()
    {

    }
GamedClient::GamedClient()
    {
        mPrevApp[0] = '\0';
        mCurrentApp[0] = '\0';
    }
/*
 * Struct containing handle to a dynamically loaded lib as well as
 * function pointers to key interfaces.
 */
struct perlockHandler {
    void *dlhandle;
    int (*useprofile)(int, int);
    int (*perf_lock_acq)(int, int, int[], int);
    int handle;
    int prof_in_use;
};
static struct perlockHandler perfhandler;

class GameInfo {
public:
    explicit GameInfo(char *pname, char *aname, char *vcode) {
        mPackageName = pname;
        mActivityName = aname;
        mVersionCode = vcode;
    }
    const char* mPackageName;
    const char* mActivityName;
    const char* mVersionCode;
};

class AnimInfo {
public:
    explicit AnimInfo(char *aname, char *wanimscale, char *tanimscale, char *aanimscale) {
        mActivityName = aname;
        mWindowAnimationScale = wanimscale;
        mTransitionAnimationScale = tanimscale;
        mAnimatorScale = aanimscale;
    }
    const char* mActivityName;
    const char* mWindowAnimationScale;
    const char* mTransitionAnimationScale;
    const char* mAnimatorScale;
};

static vector<GameInfo*> resolutionOverrideApps;
static vector<GameInfo*> gameTriggerApps;
static vector<GameInfo*> gameColocateApps;
static vector<GameInfo*> animTriggerApps;
static vector<GameInfo*> netTriggerApps;

static vector<AnimInfo*> animScale;

static bool sEnableGTLogs = false;
static int sGameDetOn = 0;

int parseWhiteListedAppsXml() {
    xmlDocPtr doc;
    xmlNodePtr currNode;
    char *packagename, *activityname, *versioncode;
    char *wanimscale, *tanimscale, *aanimscale;
    packagename = activityname = versioncode = NULL;
    wanimscale = tanimscale = aanimscale = NULL;

    if(access(WHITELISTED_APPS_XML, F_OK) < 0) {
        return -1;
    }

    doc = xmlReadFile(WHITELISTED_APPS_XML, "UTF-8", XML_PARSE_RECOVER);
    if(!doc) {
        ALOGD_IF(sEnableGTLogs,"Document not parsed successfully");
        return -1;
    }

    currNode = xmlDocGetRootElement(doc);
    if(!currNode) {
        ALOGD_IF(sEnableGTLogs,"Empty document");
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return -1;
    }

    // Confirm the root-element of the tree
    if(xmlStrcmp(currNode->name, BAD_CAST "AppCatalog")) {
        ALOGD_IF(sEnableGTLogs,"document of the wrong type, root node != root");
        xmlFreeDoc(doc);
        xmlCleanupParser();
        return -1;
    }

    currNode = currNode->xmlChildrenNode;
    resolutionOverrideApps.clear();
    gameTriggerApps.clear();
    gameColocateApps.clear();
    animTriggerApps.clear();
    netTriggerApps.clear();

    for(; currNode != NULL; currNode=currNode->next) {

        if(currNode->type != XML_ELEMENT_NODE)
            continue;

        ALOGI_IF(sEnableGTLogs, "Parsing the whitelist of %s",currNode->name);

        xmlNodePtr node = currNode;

        if (!xmlStrcmp(node->name, BAD_CAST "ResolutionOverrideApps") ||
            !xmlStrcmp(node->name, BAD_CAST "GameTriggerApps") ||
            !xmlStrcmp(node->name, BAD_CAST "GameColocateApps") ||
            !xmlStrcmp(node->name, BAD_CAST "AnimTriggerApps") ||
            !xmlStrcmp(node->name, BAD_CAST "NetTriggerApps")) {
            node = node->children;
            while(node != NULL) {
                if(!xmlStrcmp(node->name, BAD_CAST "AppAttributes")) {
                    if(xmlHasProp(node, BAD_CAST "PackageName"))
                        packagename  =
                                (char *) xmlGetProp(node, BAD_CAST "PackageName");
                    if(xmlHasProp(node, BAD_CAST "ActivityName"))
                        activityname =
                                (char *) xmlGetProp(node, BAD_CAST "ActivityName");
                    if(xmlHasProp(node, BAD_CAST "VersionCode"))
                        versioncode  =
                                (char *) xmlGetProp(node, BAD_CAST "VersionCode");

                    ALOGI_IF(sEnableGTLogs,
                          "Identified pname %s aname %s vcode %s in %s whitelist",
                          packagename ? packagename : "NULL",
                          activityname ? activityname : "NULL",
                          versioncode ? versioncode : "NULL",
                          currNode->name);

                    if (!strcmp((char *)(currNode->name), "AnimTriggerApps")) {
                        if(xmlHasProp(node, BAD_CAST "WindowAnimationScale"))
                            wanimscale  =
                                (char *) xmlGetProp(node, BAD_CAST "WindowAnimationScale");
                        if(xmlHasProp(node, BAD_CAST "TransitionAnimationScale"))
                            tanimscale  =
                                (char *) xmlGetProp(node, BAD_CAST "TransitionAnimationScale");
                        if(xmlHasProp(node, BAD_CAST "AnimatorDurationScale"))
                            aanimscale  =
                                (char *) xmlGetProp(node, BAD_CAST "AnimatorDurationScale");

                        ALOGI_IF(sEnableGTLogs,
                              "Identified wanimscale %s tanimscale %s aanimscale %s in %s whitelist",
                              wanimscale ? wanimscale : "NULL",
                              tanimscale ? tanimscale : "NULL",
                              aanimscale ? aanimscale : "NULL",
                              currNode->name);
                    }

                    if(packagename != NULL) {
                        if(!xmlStrcmp(currNode->name, BAD_CAST "ResolutionOverrideApps")) {
                            resolutionOverrideApps.push_back(
                                         new GameInfo(packagename,activityname,versioncode));
                        } else if(!xmlStrcmp(currNode->name, BAD_CAST "GameTriggerApps")) {
                            gameTriggerApps.push_back(
                                         new GameInfo(packagename,activityname,versioncode));
                        } else if(!xmlStrcmp(currNode->name, BAD_CAST "GameColocateApps")) {
                            gameColocateApps.push_back(
                                         new GameInfo(packagename,"",""));
                        } else if (!xmlStrcmp(currNode->name, BAD_CAST "AnimTriggerApps")) {
                            animTriggerApps.push_back(
                                         new GameInfo(packagename,activityname,""));
                            animScale.push_back(
                                         new AnimInfo(activityname, wanimscale, tanimscale, aanimscale));
                        } else if(!xmlStrcmp(currNode->name, BAD_CAST "NetTriggerApps")) {
                            netTriggerApps.push_back(
                                         new GameInfo(packagename,"",""));
                        }
                    }
                    packagename = activityname = versioncode = NULL;
                    wanimscale = tanimscale = aanimscale = NULL;
                }
                node = node->next;
            }
        } else {
            //Whitelist for some other usecases/features in future
        }
    }

    xmlFreeDoc(doc);
    xmlCleanupParser();
    return 0;
}

char *getWhiteListName(int whiteListType) {
    switch(whiteListType) {
    case OVERRIDE_RES_WHITELIST:
        return (char*)"ResolutionOverrideApps";
    case GAME_TRIGGER_WHITELIST:
        return (char*)"GameTriggerApps";
    case GAME_COLOCATE_WHITELIST:
        return (char*)"GameColocateApps";
    case ANIM_TRIGGER_WHITELIST:
        return (char*)"AnimTriggerApps";
    case NET_TRIGGER_WHITELIST:
        return (char*)"NetTriggerApps";
    default:
        ALOGD_IF(sEnableGTLogs,"Unsupported whitelist %d in %s", whiteListType, __func__);
        return (char*)"Null";
    }
}

void printAnimScalesApps() {

    ALOGD_IF(sEnableGTLogs, "AnimScalesApps");

    for (vector<AnimInfo*>::iterator it = animScale.begin();
        it != animScale.end(); ++it) {
        ALOGD_IF(sEnableGTLogs, "ActivityName %s WindowAnimationScale %s TransitionAnimationScale %s AnimatorDurationScale %s",
              (*it)->mActivityName ? (*it)->mActivityName : "NULL",
              (*it)->mWindowAnimationScale ? (*it)->mWindowAnimationScale : "NULL",
              (*it)->mTransitionAnimationScale ? (*it)->mTransitionAnimationScale : "NULL",
              (*it)->mAnimatorScale ? (*it)->mAnimatorScale : "NULL");
    }
}

void printWhiteListedApps() {

    ALOGD_IF(sEnableGTLogs, "ResolutionOverrideApps");

    for (vector<GameInfo*>::iterator it = resolutionOverrideApps.begin();
         it != resolutionOverrideApps.end(); ++it) {
        ALOGD_IF(sEnableGTLogs, "PackageName %s ActivityName %s VersionCode %s",
              (*it)->mPackageName ? (*it)->mPackageName : "NULL",
              (*it)->mActivityName ? (*it)->mActivityName : "NULL",
              (*it)->mVersionCode ? (*it)->mVersionCode : "NULL");
    }

    ALOGD_IF(sEnableGTLogs, "GameTriggerApps");

    for (vector<GameInfo*>::iterator it = gameTriggerApps.begin();
         it != gameTriggerApps.end(); ++it) {
        ALOGD_IF(sEnableGTLogs, "PackageName %s ActivityName %s VersionCode %s",
              (*it)->mPackageName ? (*it)->mPackageName : "NULL",
              (*it)->mActivityName ? (*it)->mActivityName : "NULL",
              (*it)->mVersionCode ? (*it)->mVersionCode : "NULL");
    }

    ALOGD_IF(sEnableGTLogs, "GameColocateApps");

    for (vector<GameInfo*>::iterator it = gameColocateApps.begin();
         it != gameColocateApps.end(); ++it) {
        ALOGD_IF(sEnableGTLogs, "PackageName %s ActivityName %s VersionCode %s",
              (*it)->mPackageName ? (*it)->mPackageName : "NULL",
              (*it)->mActivityName ? (*it)->mActivityName : "NULL",
              (*it)->mVersionCode ? (*it)->mVersionCode : "NULL");
    }

    ALOGD_IF(sEnableGTLogs, "AnimTriggerApps");

    for (vector<GameInfo*>::iterator it = animTriggerApps.begin();
        it != animTriggerApps.end(); ++it) {
        ALOGD_IF(sEnableGTLogs, "PackageName %s ActivityName %s VersionCode %s",
              (*it)->mPackageName ? (*it)->mPackageName : "NULL",
              (*it)->mActivityName ? (*it)->mActivityName : "NULL",
              (*it)->mVersionCode ? (*it)->mVersionCode : "NULL");
    }

    ALOGD_IF(sEnableGTLogs, "NetTriggerApps");
    for (vector<GameInfo*>::iterator it = netTriggerApps.begin();
         it != netTriggerApps.end(); ++it) {
        ALOGD_IF(sEnableGTLogs, "PackageName %s ActivityName %s VersionCode %s",
              (*it)->mPackageName ? (*it)->mPackageName : "NULL",
              (*it)->mActivityName ? (*it)->mActivityName : "NULL",
              (*it)->mVersionCode ? (*it)->mVersionCode : "NULL");
    }
}

bool isWhiteListed(const char *name, int whiteListType) {

    /* flattened app string "name" will be as follows:
     * packageName/activityName/versionCode
     */

    char fs[MAX_APPINFO_LEN];
    strlcpy(fs,name,MAX_APPINFO_LEN);
    char *tmp_token = NULL;
    char *temp_ptr;
    char *packagename, *activityname, *versioncode;
    packagename = activityname = versioncode = NULL;

    tmp_token = strtok_r((char*)fs, "/" , &temp_ptr);
    int i = 0;
    while(tmp_token != NULL) {
        switch(i) {
        case 0:
            packagename = tmp_token;
            break;
        case 1:
            activityname = tmp_token;
            break;
        case 2:
            versioncode = tmp_token;
            break;
        }
        i++;
        tmp_token = strtok_r(NULL, "/",  &temp_ptr);
    }

    ALOGD_IF(sEnableGTLogs,
         "flattenedString: PackageName %s ActivityName %s VersionCode %s",
         packagename ? packagename : "NULL",
         activityname ? activityname : "NULL",
         versioncode ? versioncode : "NULL");

    if(whiteListType == GAME_COLOCATE_WHITELIST || whiteListType == NET_TRIGGER_WHITELIST) {
        if(!packagename) {
            ALOGD_IF(sEnableGTLogs,
                     "packagename is NULL. Not whitelisted");
            return false;
        }
    } else if (whiteListType == ANIM_TRIGGER_WHITELIST) {
        if(!packagename or !activityname) {
            ALOGD_IF(sEnableGTLogs,
                     "One of the incoming attributes is NULL. Not whitelisted");
            return false;
        }
    } else {
        if(!packagename or !activityname or !versioncode) {
            ALOGD_IF(sEnableGTLogs,
                     "One of the incoming attributes is NULL. Not whitelisted");
            return false;
        }
    }

    vector<GameInfo*>::iterator itbegin;
    vector<GameInfo*>::iterator itend;

    switch(whiteListType) {
    case OVERRIDE_RES_WHITELIST:
        itbegin = resolutionOverrideApps.begin();
        itend = resolutionOverrideApps.end();
        break;
    case GAME_TRIGGER_WHITELIST:
        itbegin = gameTriggerApps.begin();
        itend = gameTriggerApps.end();
        break;
    case GAME_COLOCATE_WHITELIST:
        itbegin = gameColocateApps.begin();
        itend = gameColocateApps.end();
        break;
    case ANIM_TRIGGER_WHITELIST:
        itbegin = animTriggerApps.begin();
        itend = animTriggerApps.end();
        break;
    case NET_TRIGGER_WHITELIST:
        itbegin = netTriggerApps.begin();
        itend = netTriggerApps.end();
        break;
    default:
        ALOGD_IF(sEnableGTLogs,"Unsupported whitelist %d", whiteListType);
        return false;
    }

    for (vector<GameInfo*>::iterator it = itbegin; it != itend; ++it) {
        if(packagename && !strcmp((*it)->mPackageName, packagename)) {
            if(!((*it)->mActivityName) || (!activityname) ||
               !strcmp((*it)->mActivityName, activityname)) {
                if(!((*it)->mVersionCode) || !versioncode ||
                   !strcmp((*it)->mVersionCode, versioncode)) {
                    ALOGD_IF(sEnableGTLogs, "Activity %s whitelisted for %s",
                             activityname ? activityname : "NULL",
                             getWhiteListName(whiteListType));
                    return true;
                }
            }
        }
    }

    ALOGD_IF(sEnableGTLogs, "Activity %s not whitelisted for %s",
             activityname ? activityname : "NULL",
             getWhiteListName(whiteListType));
    return false;
}

float getAnimScaleValue (const char *name, int animType) {
    /* flattened app string "name" will be as follows:
     * packageName/activityName/versionCode
    */
    char fs[MAX_APPINFO_LEN];
    strlcpy(fs,name,MAX_APPINFO_LEN);
    char *tmp_token = NULL;
    char *temp_ptr;
    char *packagename, *activityname, *versioncode;
    char *wanimscale, *tanimscale, *aanimscale;

    packagename = activityname = versioncode = NULL;
    wanimscale = tanimscale = aanimscale = NULL;
    float scalevalue = -1.0f;

    tmp_token = strtok_r((char*)fs, "/" , &temp_ptr);
    int i = 0;
    while(tmp_token != NULL) {
        switch(i) {
            case 0:
                packagename = tmp_token;
                break;
            case 1:
                activityname = tmp_token;
                break;
            case 2:
                versioncode = tmp_token;
                break;
        }
        i++;
        tmp_token = strtok_r(NULL, "/",  &temp_ptr);
    }
    vector<AnimInfo*>::iterator itbegin = animScale.begin();
    vector<AnimInfo*>::iterator itend = animScale.end();
    for (vector<AnimInfo*>::iterator it = itbegin; it != itend; ++it) {
        if (((*it)->mActivityName) && activityname &&
               !strcmp((*it)->mActivityName, activityname)) {
                    switch(animType) {
                        case 0:
                            ALOGD_IF(sEnableGTLogs, "activityname:%s mWindowAnimationScale:%s",
                                        activityname, (*it)->mWindowAnimationScale);
                            if((*it)->mWindowAnimationScale)
                                scalevalue = strtof((*it)->mWindowAnimationScale, NULL);
                            break;
                        case 1:
                            ALOGD_IF(sEnableGTLogs, "activityname:%s mTransitionAnimationScale:%s",
                                        activityname, (*it)->mTransitionAnimationScale);
                            if((*it)->mTransitionAnimationScale)
                                scalevalue = strtof((*it)->mTransitionAnimationScale, NULL);
                            break;
                        case 2:
                            ALOGD_IF(sEnableGTLogs, "activityname:%s mAnimatorScale:%s",
                                        activityname, (*it)->mAnimatorScale);
                            if((*it)->mAnimatorScale)
                                scalevalue = strtof((*it)->mAnimatorScale, NULL);
                            break;
                    }
        }
    }
    return scalevalue;
}

void open_perf_lock(void)
{
    const char *rc = NULL;
    static int opened = 0;
    char buf[PROPERTY_VALUE_MAX];

    if (!opened) {
         /* Retrieve name of vendor extension library */
         if (property_get("ro.vendor.extension_library", buf, NULL) <= 0) {
             ALOGD_IF(sEnableGTLogs,"%sperflock path not found\n", __func__);
             return;
         }

         /* Sanity check - ensure */
         buf[PROPERTY_VALUE_MAX-1] = '\0';
         if (strstr(buf, "/") != NULL) {
              ALOGD_IF(sEnableGTLogs,"%sInvalid perf lock extn lib\n", __func__);
              return;
         }

         /*
          * Clear error and load lib.
          */
         dlerror();
         perfhandler.dlhandle = dlopen(buf, RTLD_NOW | RTLD_LOCAL);
          if (perfhandler.dlhandle == NULL) {
              ALOGD_IF(sEnableGTLogs,"%s Failed to (dl)open perf lock\n", __func__);
              return;
          }

          *(void **) (&perfhandler.useprofile) = dlsym(perfhandler.dlhandle, "perf_lock_use_profile");
          if ((rc = dlerror()) != NULL) {
              ALOGD_IF(sEnableGTLogs,"%s Failed to get user_profile perf lock\n", __func__);
              dlclose(perfhandler.dlhandle);
              return;
          }

          *(void **) (&perfhandler.perf_lock_acq) = dlsym(perfhandler.dlhandle, "perf_lock_acq");
          if ((rc = dlerror()) != NULL) {
              ALOGE("%s Failed to get perf_lock_acq\n", __func__);
              dlclose(perfhandler.dlhandle);
              return;
          }
          perfhandler.handle = 0;
          opened = 1;
  }
}

/*This class has supporting function to set sched_group_id
  of colocated game thread*/
class SchedColocateTaskByGrouping {
    private:
            const char *pname;
            int ppid;
    public:
            SchedColocateTaskByGrouping(const char *name, int pid) {
                pname = name;
                ppid = pid;
            }

            ~SchedColocateTaskByGrouping() {
            }

            int setSchedGroupID() {
                int rc=-1,rc1=-1;

                DIR *dir;
                struct dirent *readDir;
                char file_name[MAX_APPINFO_LEN], dir_name[MAX_APPINFO_LEN], pid_str[MAX_APPINFO_LEN];
                int fd, itr_set_sched_grp_id=LOOP_COUNT_SET_COLOCATE_SCHED_GRP_ID;
                FILE *fp;

                memset(dir_name, 0, MAX_APPINFO_LEN);
                snprintf(dir_name, MAX_APPINFO_LEN, "/proc/%d/task", ppid);
                memset(pid_str, 0, MAX_APPINFO_LEN);
                snprintf(pid_str, MAX_APPINFO_LEN, "%d", ppid);
                do {
                    /*find all tasks related to game PID and set sched_group_id*/
                    if((dir = opendir(dir_name))) {
                        while((readDir = readdir(dir))) {
                            memset(file_name, 0, MAX_APPINFO_LEN);
                            snprintf(file_name, MAX_APPINFO_LEN, "/proc/%s/sched_group_id", readDir->d_name);
                            fd = open(file_name, O_WRONLY);
                            if (fd >= 0) {
                                rc1 = write(fd, pid_str, strlen(pid_str));// set sgid
                                close(fd);
                            }
                        } //while(readDir)
                        closedir(dir);
                    } else { //if(opendir)
                        rc=-1;
                        return rc;
                    }
                    /*Below while loop is for double verify by looping over tasks again, to check
                      if any newly created task's sched_group_id is not set to proper sgid. As such
                      scheduler colocation inheritance should take care of it but still corner case
                      may exist when a child task is created at boundary when parent sgid is just set*/
                } while(--itr_set_sched_grp_id > 0);
                rc=0;
                return rc;
            }
};

void close_perf_lock(void) {
    if(perfhandler.dlhandle) {
        if(perfhandler.useprofile) {
            perfhandler.useprofile = NULL;
        }
        if (perfhandler.perf_lock_acq) {
            perfhandler.perf_lock_acq = NULL;
        }
        dlclose(perfhandler.dlhandle);
        perfhandler.dlhandle = NULL;
    }
}

extern "C" void activity_trigger_init(void)
{
    char property[PROPERTY_VALUE_MAX] = {0};
    sEnableGTLogs = false;
    sGameDetOn = 0;
    if((property_get("persist.debug.gtlogs_enable", property, NULL) > 0) &&
       (!strncmp(property, "1", PROPERTY_VALUE_MAX ) ||
        (!strncasecmp(property,"true", PROPERTY_VALUE_MAX )))) {
        sEnableGTLogs = true;
    }
    parseWhiteListedAppsXml();
    if(perfhandler.dlhandle == NULL || perfhandler.useprofile == NULL || perfhandler.perf_lock_acq == NULL)
        open_perf_lock();
    ALOGD_IF(sEnableGTLogs, "Game trigger library initialized successfully");

    property_get("debug.enable.gamed", property,"0");
    sGameDetOn = atoi(property);
    ALOGD_IF(sEnableGTLogs,"Enable the gameD flag = %d", sGameDetOn);
}


extern "C" void activity_trigger_process_start(const char *name, int pid) {
    char property[PROPERTY_VALUE_MAX];
    property_get("sched.colocate.enable", property, "1");
    if (!atoi(property))
        return;

    if (!name || !pid) {
        ALOGD_IF(sEnableGTLogs,"App flattened string is NULL OR pid is 0");
        return;
    }

    /* Check if game is whitelisted to colocate*/
    if(isWhiteListed(name, GAME_COLOCATE_WHITELIST)) {

        /* flattened app string "name" may be as follows:
           packageName/activityName/versionCode */

        char fs[MAX_APPINFO_LEN],*temp_ptr=NULL,*packagename=NULL;
        strlcpy(fs,name,sizeof(fs));
        packagename = strtok_r((char*)fs, "/" , &temp_ptr);

        ALOGD_IF(sEnableGTLogs,"Request for Sched Colocation of Game package: %s", packagename ? packagename : "NULL");

        SchedColocateTaskByGrouping objGroupID(name, pid);
        if((objGroupID.setSchedGroupID())!= -1) {
            ALOGD_IF(sEnableGTLogs,"Sched Colocation enabled for Game %s", packagename ? packagename : "NULL");
        }
    }
}

extern "C" void activity_trigger_animationScalesCheck(const char *name, int type, float *scaleValue) {
    if (name && isWhiteListed(name, ANIM_TRIGGER_WHITELIST)) {
        if (NULL != scaleValue)
            *scaleValue = getAnimScaleValue(name, type);
    }
}

extern "C" void activity_trigger_networkOptsCheck(int flag, int netType, const char *name) {
    bool isMobile = (netType == 0) ? true : false;
    int buf[2];
    buf[0] = MPCTLV3_KEEP_ALIVE;
    (!flag && isMobile && isWhiteListed(name, NET_TRIGGER_WHITELIST)) ? buf[1] = 0 : buf[1] = 1;
    perfhandler.handle = perfhandler.perf_lock_acq(perfhandler.handle, 100, buf, 2);
}

extern "C" void activity_trigger_start(const char *name, int *flags) {
    int len;
    int found = 0;
    int profile_num = -1;
    int handle = 0;
    char buf[PROPERTY_VALUE_MAX];
    char property[PROPERTY_VALUE_MAX] = {0};
    ALOGD_IF(sEnableGTLogs, "Game trigger starting '%s'", (name) ? name : "<null>");

    if (!name || (NULL == flags)) {
        ALOGD_IF(sEnableGTLogs,"App flattened string or flags are NULL");
        return;
    }

    /* Gamed and the game_trigger approach needs to be mutually
     * exclusive as to avoid profile manager concurrencies. Profile
     * manager does not handle concurrencies. If they both exists
     * together, it will need a mechanism between game_trigger
     * and gamed to communicate eachother when they apply
     * game profile. This will incur lot of communication overhead.*/
    if (sGameDetOn) {
        ALOGD_IF(sEnableGTLogs,"perf:calling NotifyFgAppChange");
        GamedClient *gdObj = GamedClient::getInstance();
        if (gdObj) {
            gdObj->NotifyFgAppChange(GAMEDET_EVENT_FGAPPSTART,name);
        }
    } else {
        if (isWhiteListed(name, OVERRIDE_RES_WHITELIST))
            *flags |= FLAG_OVERRIDE_RESOLUTION;
        if (property_get(GAME_TRIGGER_PROPERTY, buf, NULL) > 0) {
            profile_num = atoi(buf);
            ALOGD_IF(sEnableGTLogs,
                    "Profile Property set for Profile Number to %d", profile_num);
        } else {
            //Profile Number not set, Do Nothing
            ALOGD_IF(sEnableGTLogs,
                    "Profile Property for Profile Number to use is NOT set, Doing Nothing");
            return;
        }

             // Find a game match
        if (isWhiteListed(name, GAME_TRIGGER_WHITELIST))
            found = 1;

        if (perfhandler.useprofile && perfhandler.prof_in_use && !found) {
            ALOGD_IF(sEnableGTLogs,
                    "Game trigger NOT found and profile is in use for '%s'", name);
            perfhandler.handle = perfhandler.useprofile(perfhandler.handle, -1);
            perfhandler.handle = 0;
            perfhandler.prof_in_use = 0;
            ALOGD_IF(sEnableGTLogs, "Profile Removed\n");
        } else if (perfhandler.useprofile && !perfhandler.prof_in_use && found) {
            perfhandler.handle = perfhandler.useprofile(perfhandler.handle, profile_num);
            ALOGD_IF(sEnableGTLogs, "profile not in use. Profile Applied for %s\n", name);
            perfhandler.prof_in_use = 1;
        }
   }
}


extern "C" void activity_trigger_resume(const char *name) {
    /* Do the same as activity start */
    int flags = 0;
    if (sGameDetOn) {
        GamedClient *gdObj = GamedClient::getInstance();
        if (gdObj) {
            gdObj->NotifyFgAppChange(GAMEDET_EVENT_FGAPPRESUME,name);
        }
    }
    else {
        activity_trigger_start(name, &flags);
    }
}

extern "C" void activity_trigger_pause(const char *name) {
    /* Do the same as activity start */
    if (sGameDetOn) {
        GamedClient *gdObj = GamedClient::getInstance();
        if (gdObj) {
            gdObj->NotifyFgAppChange(GAMEDET_EVENT_FGAPPPAUSE,name);
        }
    }
}

extern "C" void activity_trigger_stop(const char *name) {
    /* Do the same as activity start */
    if (sGameDetOn) {
        GamedClient *gdObj = GamedClient::getInstance();
        if (gdObj) {
            gdObj->NotifyFgAppChange(GAMEDET_EVENT_FGAPPSTOP,name);
        }
    }
}

extern "C" void activity_trigger_deinit(void)
{
    int handle = 0;
    ALOGD_IF(sEnableGTLogs,"%s: Profile Removed\n", __func__);

    if(perfhandler.dlhandle) {
        if(perfhandler.useprofile) {
            perfhandler.useprofile(perfhandler.handle, -1);
            perfhandler.prof_in_use = 0;
            perfhandler.useprofile = NULL;
        }
        if (perfhandler.perf_lock_acq) {
            perfhandler.perf_lock_acq = NULL;
        }
        dlclose(perfhandler.dlhandle);
        perfhandler.dlhandle = NULL;
    }

    ALOGD_IF(sEnableGTLogs,
             "Freeing up mem associated with ResolutionOverrideApps");

    for (vector<GameInfo*>::iterator it = resolutionOverrideApps.begin();
         it != resolutionOverrideApps.end(); ++it) {
        if((*it)->mPackageName) xmlFree((void*)(*it)->mPackageName);
        if((*it)->mActivityName) xmlFree((void*)(*it)->mActivityName);
        if((*it)->mVersionCode) xmlFree((void*)(*it)->mVersionCode);
    }

    ALOGD_IF(sEnableGTLogs,
             "Freeing up mem associated with GameTriggerApps");

    for (vector<GameInfo*>::iterator it = gameTriggerApps.begin();
         it != gameTriggerApps.end(); ++it) {
        if((*it)->mPackageName) xmlFree((void*)(*it)->mPackageName);
        if((*it)->mActivityName) xmlFree((void*)(*it)->mActivityName);
        if((*it)->mVersionCode) xmlFree((void*)(*it)->mVersionCode);
    }

    ALOGD_IF(sEnableGTLogs,
             "Freeing up mem associated with GameColocateApps");

    for (vector<GameInfo*>::iterator it = gameColocateApps.begin();
         it != gameColocateApps.end(); ++it) {
        if((*it)->mPackageName) xmlFree((void*)(*it)->mPackageName);
        if((*it)->mActivityName) xmlFree((void*)(*it)->mActivityName);
        if((*it)->mVersionCode) xmlFree((void*)(*it)->mVersionCode);
    }

    ALOGD_IF(sEnableGTLogs,
             "Freeing up mem associated with animTriggerApps");

    for (vector<GameInfo*>::iterator it = animTriggerApps.begin();
         it != animTriggerApps.end(); ++it) {
        if((*it)->mPackageName) xmlFree((void*)(*it)->mPackageName);
        if((*it)->mActivityName) xmlFree((void*)(*it)->mActivityName);
        if((*it)->mVersionCode) xmlFree((void*)(*it)->mVersionCode);
    }

    ALOGD_IF(sEnableGTLogs,
             "Freeing up mem associated with animScale");

    for (vector<AnimInfo*>::iterator it = animScale.begin();
         it != animScale.end(); ++it) {
        if((*it)->mActivityName) xmlFree((void*)(*it)->mActivityName);
        if((*it)->mWindowAnimationScale) xmlFree((void*)(*it)->mWindowAnimationScale);
        if((*it)->mTransitionAnimationScale) xmlFree((void*)(*it)->mTransitionAnimationScale);
        if((*it)->mAnimatorScale) xmlFree((void*)(*it)->mAnimatorScale);
    }

    ALOGD_IF(sEnableGTLogs,
             "Freeing up mem associated with netTriggerApps");

    for (vector<GameInfo*>::iterator it = netTriggerApps.begin();
         it != netTriggerApps.end(); ++it) {
        if((*it)->mPackageName) xmlFree((void*)(*it)->mPackageName);
        if((*it)->mActivityName) xmlFree((void*)(*it)->mActivityName);
        if((*it)->mVersionCode) xmlFree((void*)(*it)->mVersionCode);
    }

    //Gamed related clean up
    if (sGameDetOn) {
        GamedClient *gdObj = GamedClient::getInstance();
        if (gdObj) {
            delete gdObj;
            gdObj = NULL;
        }
    }
}


int GamedClient::NotifyFgAppChange(unsigned int state,const char *name) {
    char *ptr = NULL;
    struct gamedet_msg msg = {0, '\0'};
    unsigned int strSize = 0;

    ptr = strchr(name, '/');
    if(NULL == ptr) {
        ALOGD_IF(sEnableGTLogs,"didn't find '/' in the app name");
        return -1;
    }

    strSize = (ptr - name)+1;
    if (strSize > MAX_MSG_APP_NAME_SIZE) {
        ALOGD_IF(sEnableGTLogs,"App name too big truncating it");
        strSize = MAX_MSG_APP_NAME_SIZE;
    }
    /* store the current notified app */
    strlcpy(mCurrentApp, name, strSize);
    ALOGD_IF(sEnableGTLogs,"perf: Notif-FG-App-Change previous:%s and current:%s and state:%d",mPrevApp,mCurrentApp,state);
    /* Prepare message for sending */
    switch (state) {
        case GAMEDET_EVENT_FGAPPSTART:
        case GAMEDET_EVENT_FGAPPRESUME:
            break;
        case GAMEDET_EVENT_FGAPPPAUSE:
        case GAMEDET_EVENT_FGAPPSTOP:
            if((strncmp(mPrevApp, mCurrentApp, strSize))) {
                ALOGD_IF(sEnableGTLogs,"PAUSE or STOP received for the previous app : %s , just return",mCurrentApp);
                return -1;
            } else {
                ALOGD_IF(sEnableGTLogs,"PAUSE or STOP received for the previous app : %s ,which is already started",mPrevApp);
            }
            break;
        default:
            ALOGD_IF(sEnableGTLogs,"Bad parameter to gamed_send %d", state);
            return -1;
    }

    int lentocpy = strlen(mCurrentApp) + 1;
    if (lentocpy > MAX_MSG_APP_NAME_SIZE) {
        lentocpy = MAX_MSG_APP_NAME_SIZE;
    }
    strlcpy(msg.name, mCurrentApp, lentocpy);
    msg.opcode = ((0x0F) & GAMED_VERSION) << GAMED_SHIFT_BIT_VERSION;
    msg.opcode = (msg.opcode |((0x0F)&state));
    ALOGD_IF(sEnableGTLogs,"Client sending msg.name is %s and msg.opcode is: %d ", msg.name,msg.opcode);
    game_client_send_cmd(msg);
    strlcpy(mPrevApp, name, strSize);
    return 0;
}
int GamedClient::game_client_send_cmd(struct gamedet_msg msg)
{
    int rc, len;
    int client_comsoc = -1;
    struct sockaddr_un client_addr;

    client_comsoc = socket(PF_UNIX,SOCK_SEQPACKET, 0);
    if (client_comsoc < 0) {
        ALOGD_IF(sEnableGTLogs,"game client socket creation Failed");
    }

    fcntl(client_comsoc, F_SETFL, O_NONBLOCK);

    memset(&client_addr, 0, sizeof(struct sockaddr_un));
    client_addr.sun_family = AF_UNIX;
    snprintf(client_addr.sun_path, UNIX_PATH_MAX, GAME_SOCKET);

    len = sizeof(struct sockaddr_un);
    rc = connect(client_comsoc, (struct sockaddr *) &client_addr, len);
    if (rc == -1) {
        ALOGD_IF(sEnableGTLogs,"Failed in connect to socket: errno=%d (%s)", rc, strerror(rc));
        goto error;
    }
    rc = send(client_comsoc, &msg, sizeof(gamedet_msg), 0);
    if (rc == -1) {
        ALOGD_IF(sEnableGTLogs,"Failed in send game cmd: errno=%d (%s)", rc, strerror(rc));
        goto error;
    }
    close(client_comsoc);
    return rc;
error:
    if (client_comsoc >= 0) {
        close(client_comsoc);
    }
    return 0;
}
