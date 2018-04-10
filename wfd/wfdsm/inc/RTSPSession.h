/*==============================================================================
*        @file RTSPSession.h
*
*  @par DESCRIPTION:
*        RTSPSession class.
*
*
*Copyright (c) 2011-2016 Qualcomm Technologies, Inc.
*All Rights Reserved.
*Confidential and Proprietary - Qualcomm Technologies, Inc.
==============================================================================*/


#ifndef _RTSPSESSION_H
#define _RTSPSESSION_H

#include "rtsp_server.h"
#include "rtsp_client.h"
#include "MMTimer.h"
#include <pthread.h>


/* Forward Declarations */
class WFDSession;
class RTSPSession;
class Device;



typedef enum RTSPState {
    STOPPED,
    CAP_NEGOTIATING,
    CAP_NEGOTIATED,
    SESS_ESTABLISHING,
    SESS_ESTABLISHED,
    STANDBY,
    STARTING,
    TEARING_DOWN
} RTSPState;

typedef enum StreamCtrlType {
    PLAY,
    PLAY_DONE,
    PAUSE,
    PAUSE_DONE,
    TEARDOWN
} StreamCtrlType;

typedef enum MMSessionType{
    MIRROR,
    DS
} MMSessionType;


class cback : public rtspCallbacks {
private:
    RTSPSession* pRtspSession;
    MM_HANDLE    m_hTimer;
public:
    cback(RTSPSession*);
    void setupCallback(rtspApiMesg &mesg);
    void playCallback(rtspApiMesg &mesg);
    void pauseCallback(rtspApiMesg &mesg);
    void teardownCallback(rtspApiMesg &mesg);
    void closeCallback(rtspApiMesg &mesg);
    void openCallback(rtspApiMesg &mesg);
    void intersectCallback(rtspApiMesg &mesg);
    void getCallback(rtspApiMesg &mesg);
    void setCallback(rtspApiMesg &mesg);
    void finishCallback();
    static void keepAliveTimerCallback(void *);
};

typedef struct
{
    int rtpPort;
    int rtcpPort;
}localTransportInfo;

typedef struct
{
    int inPause;
    int inPlay;
    int mode;
}currentState;

typedef struct
{
    int width;
    int height;
    int orientation;
}presentationSurface;

/**----------------------------------------------------------------------------
   RTSPSession class
-------------------------------------------------------------------------------
*/

class RTSPSession
{
private:
    rtspCallbacks *events;
    rtspServer *server;
    rtspClient *client;
    pthread_t session_thread;
    bool bThreadCreated;

    WFDSession* pWFDSession;

    void play();
    void pause();
    void teardown();

    bool enableUIBC(bool);

    static void *rtspServerLoopFunc(void *s);
    static void *rtspClientLoopFunc(void *s);

public:
    RTSPState                rtspState;
    int                      rtspSessionId;
    Device*                  pPeerDevice;
    localTransportInfo       m_sLocalTransportInfo;
    static AVPlaybackMode    m_eplayMode;
    bool                     m_bUIBCSupported;
    bool                     m_bTCPSupportedAtSink;
    bool                     m_bTCPSupportQueried;
    bool                     m_bTCPSupportStatusRequested;
    bool                     m_bAuxStreamSupported;

    StreamCtrlType           m_bSessionState;

    currentState             m_CurrState;
    bool                     m_bDirStrSupported;
    presentationSurface      m_PreSurfProp;
    /* This variable is to track pending standby requests
     * to be resent while RTSP was busy serving other request
     */
    bool bStandbyInProgress = false;

    RTSPSession(WFDSession*, Device*);
    ~RTSPSession();

    bool startServer(string, int, int);
    bool startClient(string, int, int, int,int);
    void stop();
    void rtspStateTransition(RTSPState, bool notify = true);
    //This function is to query the currently negotiated resolution for the WFD session
    static bool getNegotiatedResolution(int32_t* , int32_t*);
    static unsigned int  getNegotiatedUIBCCategory ();
    MMEventStatusType updateHdcpSupportedInConnectedDevice(void *pCfgCababilities);

    static void streamControl(int, StreamCtrlType);
    static bool uibcControl(int, bool);
    static void Cleanup();
    static bool setUIBC(int);
    static bool standby(int);
    static bool setResolution(int ,int, int* resParams = NULL);
    static void setTransport(int);
    static void queryTCPTransportSupport();
    static void sendBufferingControlRequest(int cmdType, int cmdVal);
    static void sendTransportChangeRequest(int TransportType, int BufferLenMs, int portNum);
    static void setDecoderLatency(int latency);
    static void sendIDRRequest();
    static void UpdateLocalTransportInfo(localTransportInfo *);
    static void sendAVFormatChangeRequest();
    static bool sendAVFormatChangeInformation(bool mode,int codec);
    static void sendWFDKeepAliveMsg();
    static void reSendStandbyMsg();
    static bool setAVPlaybackMode (AVPlaybackMode);
    static void setSinkSurface (void* surface);
    static void setSurfaceProp(int width,int height,int orientation);
    static void getNegotiatedResolution();
    static uint32_t* getCommonResloution(int*);
    static uint32_t* getNegotiatedResloution();
    static int  DISPhdcpCallBack(int type, void *param);
    static int getCfgItems(int** configItems, size_t* len);
    static void StreamingSupportReq(bool bFlag, void* pData);
    static bool executeRuntimeCommand(int);

    static bool updateCurrentState(int mode);
    bool prepareForTransportChange(int mode);
    void getAuxStreamIntersection();
    int prepareNegotiatedAuxBitmap(int localBM, int peerBM);
    int getBitSetPosition(int num);

};

#endif /*_RTSPSESSION_H*/
