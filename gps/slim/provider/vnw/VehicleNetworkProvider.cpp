/**
 @file
 @brief Vehicle Network provider implementation file.

 This file defines VNW provider object implementation.
 */

/*
 Copyright (c) 2014-2015 Qualcomm Technologies, Inc. All Rights Reserved.
 Qualcomm Technologies Proprietary and Confidential.
 */

#include <inttypes.h>
#include <slim_os_log_api.h>
#include "VehicleNetworkProvider.h"
#include "CanFrameEvent.h"

//! @addtogroup slim_VNWProvider
//! @{

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/
#undef SLIM_LOG_LOCAL_MODULE
//! @brief Logging module for VNW provider.
#define SLIM_LOG_LOCAL_MODULE SLIM_LOG_MOD_VNW

using namespace slim;

const uint32_t VehicleNetworkProvider::s_qThreadId = THREAD_ID_VNW;
VehicleNetworkProvider *VehicleNetworkProvider::s_pzInstance;
Mutex VehicleNetworkProvider::s_zInstanceMutex;
const slimAvailableServiceMaskT VehicleNetworkProvider::s_qSupportedServices =
    (uint32_t(1) << eSLIM_SERVICE_VEHICLE_ACCEL)
        | (uint32_t(1) << eSLIM_SERVICE_VEHICLE_GYRO)
        | (uint32_t(1) << eSLIM_SERVICE_VEHICLE_ODOMETRY);

void VehicleNetworkProvider::threadMain(void *pData)
{
  SLIM_LOGD("VehicleNetworkProvider::threadMain(0x%"PRIxPTR")",
      (uintptr_t)pData);
  VehicleNetworkProvider *pzVNWProvider =
      reinterpret_cast<VehicleNetworkProvider*>(pData);
  slim_IpcStart(s_qThreadId);
  pzVNWProvider->m_pzTimer = slim_TimerCreate(eTIMER_DEFAULT, s_qThreadId);
  /* Service support is enabled */
  pzVNWProvider->m_pzCanWrapper = CanWrapper::getInstance();
  SLIM_LOGD("m_pzCanWrapper=0x%"PRIxPTR,
      (uintptr_t)pzVNWProvider->m_pzCanWrapper);
  pzVNWProvider->m_qServiceMask = s_qSupportedServices;
  pzVNWProvider->routeConfigurationChange(s_qSupportedServices);
  pzVNWProvider->runEventLoop();
}

/*!
 * @brief Returns provider instance.
 *
 * Method provides access to provider instance. If necessary, the instance
 * is created and initialized.
 *
 * @return Provider instance.
 * @retval 0 On error.
 */
slim_ServiceProviderInterfaceType *VehicleNetworkProvider::getProvider()
{
  MutexLock _l(s_zInstanceMutex);
  if (0 == s_pzInstance)
  {
    s_pzInstance = new VehicleNetworkProvider;
    if (!s_pzInstance->initialize())
    {
      SLIM_LOGE("VNW provider initialization failed;");
      delete s_pzInstance;
      s_pzInstance = 0;
      return 0;
    }
  }
  return s_pzInstance->getSlimInterface();
}

VehicleNetworkProvider::VehicleNetworkProvider()
    : MultiplexingProvider(SLIM_PROVIDER_VNW, false, 0)
    , m_zThread(), m_zMutex(), m_pzTimer(0)
    , m_pzCanWrapper(0), m_earId(0), m_qEnabledServices(0)
    , m_pzVehicleNetworkCfg(0), m_pzAccelProc(0)
    , m_pzGyroProc(0), m_pzOdometryProc(0)
    , m_tLastBusControllerSyncTime(0)
    , m_tLastBusControllerSyncTimestamp(0)
{
  memset(&m_zThread, 0, sizeof(m_zThread));
}

VehicleNetworkProvider::~VehicleNetworkProvider()
{
}

bool VehicleNetworkProvider::initialize()
{
  if (!slim_ThreadCreate(&m_zThread, threadMain, this, "VNWPoll"))
  {
    return false;
  }
  slim_TaskReadyWait(THREAD_ID_VNW);

  return true;
}

void VehicleNetworkProvider::cbOnNewCanFrame(CwFrame* pFrame, void *userData,
    int ifNo)
{
  VehicleNetworkProvider *pzVNWProvider =
      reinterpret_cast<VehicleNetworkProvider*>(userData);
  SLIM_LOGD("New frame received. id:%u, if:%d", (unsigned int )pFrame->getId(),
      ifNo);

  /* create new message data */
  CanFrameEvent* pzNewEvent = new CanFrameEvent(pFrame, ifNo);

  /* send message to event processing loop */
  if (!slim_IpcSendData(THREAD_ID_VNW, eIPC_MSG_NEW_FRAME_EVENT, &pzNewEvent,
      sizeof(pzNewEvent)))
  {
    SLIM_LOGE("Error sending IPC message to event processing loop");
  }
}

void VehicleNetworkProvider::runEventLoop()
{
  SLIM_LOGD("Starting event loop");

  /* Register callback to CanWrapper */
  /* Configure listener to get all frames on all interfaces*/
  m_earId = m_pzCanWrapper->registerListener(CwBase::MASK11, 0, cbOnNewCanFrame,
      (void *)(this), CwBase::IFACE_ANY);
  SLIM_LOGD("m_earId=%"PRIu32, (uint32_t)m_earId);
  if (0 == m_earId)
  {
    SLIM_LOGE("Error registering CanWrapper listener");
  }

  //slim_TimerStart(m_pzTimer, 500, 0);
  m_pzVehicleNetworkCfg = new VehicleNetworkConfiguration();
  m_pzVehicleNetworkCfg->Load();

  char pzTStr[2048];
  m_pzVehicleNetworkCfg->ToString(pzTStr, sizeof(pzTStr));
  SLIM_LOGD("Configuration: %s", pzTStr);

  if (0 != m_pzVehicleNetworkCfg->GetAccelConfig())
  {
    m_pzVehicleNetworkCfg->GetAccelConfig()->ToString(pzTStr, sizeof(pzTStr));
    SLIM_LOGD("m_pzAccelConfig: %s", pzTStr);

    if (m_pzVehicleNetworkCfg->GetAccelConfig()->CfgValid())
    {
      m_pzAccelProc = VehicleNetworkMessageProcessor::CreateProcessor(
          this,
          m_pzVehicleNetworkCfg->GetAccelConfig(),
          eSLIM_SERVICE_VEHICLE_ACCEL);
      if (0 != m_pzAccelProc)
      {
        m_pzAccelProc->ToString(pzTStr, sizeof(pzTStr));
        SLIM_LOGD("m_pzAccelProc: %s", pzTStr);
      }
      else
      {
        SLIM_LOGD("m_pzAccelProc was NOT created");
      }
    }
  }
  else
  {
    SLIM_LOGD("m_pzAccelConfig was NOT loaded");
  }

  if (0 != m_pzVehicleNetworkCfg->GetGyroConfig())
  {
    m_pzVehicleNetworkCfg->GetGyroConfig()->ToString(pzTStr, sizeof(pzTStr));
    SLIM_LOGD("m_pzGyroConfig: %s", pzTStr);

    if (m_pzVehicleNetworkCfg->GetGyroConfig()->CfgValid())
    {
      m_pzGyroProc = VehicleNetworkMessageProcessor::CreateProcessor(
          this,
          m_pzVehicleNetworkCfg->GetGyroConfig(),
          eSLIM_SERVICE_VEHICLE_GYRO);
      if (0 != m_pzGyroProc)
      {
        m_pzGyroProc->ToString(pzTStr, sizeof(pzTStr));
        SLIM_LOGD("m_pzGyroProc: %s", pzTStr);
      }
      else
      {
        SLIM_LOGD("m_pzGyroProc was NOT created");
      }
    }
  }
  else
  {
    SLIM_LOGD("m_pzGyroConfig was NOT loaded");
  }

  if (0 != m_pzVehicleNetworkCfg->GetOdometryConfig())
  {
    if (m_pzVehicleNetworkCfg->GetOdometryConfig()->CfgValid())
    {
      m_pzVehicleNetworkCfg->GetOdometryConfig()->ToString(pzTStr,
          sizeof(pzTStr));
      SLIM_LOGD("m_pzOdometryConfig: %s", pzTStr);
      m_pzOdometryProc = VehicleNetworkMessageProcessor::CreateProcessor(
          this,
          m_pzVehicleNetworkCfg->GetOdometryConfig(),
          eSLIM_SERVICE_VEHICLE_ODOMETRY);
      if (0 != m_pzOdometryProc)
      {
        m_pzOdometryProc->ToString(pzTStr, sizeof(pzTStr));
        SLIM_LOGD("m_pzOdometryProc: %s", pzTStr);
      }
      else
      {
        SLIM_LOGD("m_pzOdometryProc was NOT created");
      }
    }
  }
  else
  {
    SLIM_LOGD("m_pzOdometryConfig was NOT loaded");
  }

  // slim_TimerStart(m_pzTimer, 500, 0);
  // Notify controller that the task is ready to run.
  slim_TaskReadyAck();
  while (1)
  {
    slim_IpcMsgT* pz_Msg = NULL;
    while (NULL == (pz_Msg = slim_IpcReceive()))
      ;

    SLIM_LOGD("IPC message received. q_MsgId:%" PRIu32 ", q_SrcThreadId:%" PRIu32,
        (uint32_t)pz_Msg->q_MsgId,
        (uint32_t)pz_Msg->q_SrcThreadId);

    switch (pz_Msg->q_MsgId)
    {
    case eIPC_MSG_NEW_FRAME_EVENT:
    {
      SLIM_LOGD("New frame event received");
      CanFrameEvent* pzNewEvent;
      slim_Memscpy(&pzNewEvent, sizeof(pzNewEvent), pz_Msg->p_Data,
          pz_Msg->q_Size);
      MutexLock _l(m_zMutex);
      if (m_pzAccelProc)
        m_pzAccelProc->OnNewCanFrameEvent(pzNewEvent);
      if (m_pzGyroProc)
        m_pzGyroProc->OnNewCanFrameEvent(pzNewEvent);
      if (m_pzOdometryProc)
        m_pzOdometryProc->OnNewCanFrameEvent(pzNewEvent);
      delete pzNewEvent;
    }
      break;

    case eIPC_MSG_TIMER:
    {
      SLIM_LOGD("Timer signal received");
      os_TimerExpiryType z_TimerMsg;
      slim_Memscpy(&z_TimerMsg, sizeof(z_TimerMsg), pz_Msg->p_Data,
          pz_Msg->q_Size);
      MutexLock _l(m_zMutex);
      if (m_pzAccelProc)
        m_pzAccelProc->OnNewTimerExpiryEvent(&z_TimerMsg);
      if (m_pzGyroProc)
        m_pzGyroProc->OnNewTimerExpiryEvent(&z_TimerMsg);
      if (m_pzOdometryProc)
        m_pzOdometryProc->OnNewTimerExpiryEvent(&z_TimerMsg);
      if (!slim_TimerStart(m_pzTimer, 500, 0))
      {
        SLIM_LOGE("Error starting timer");
      }
    }
      break;
    default:
    {
      MutexLock _l(m_zMutex);
      if (m_pzAccelProc)
        m_pzAccelProc->OnNewIpcMessage(pz_Msg);
      if (m_pzGyroProc)
        m_pzGyroProc->OnNewIpcMessage(pz_Msg);
      if (m_pzOdometryProc)
        m_pzOdometryProc->OnNewIpcMessage(pz_Msg);
    }
      break;
    }

    slim_IpcDelete(pz_Msg);
  }
}

/**
 @brief Method for enabling or disabling vehicle services.

 @param[in] uEnable Flag that indicates if the service shall be enabled or
 disabled.
 @param[in] eService      Service to control.

 @return eSLIM_SUCCESS is operation succeeded.
 */
slimErrorEnumT VehicleNetworkProvider::doUpdateVehicleDataStatus(
bool uEnable, slimServiceEnumT eService)
{
  MutexLock _l(m_zMutex);
  if (uEnable)
  {
    SLIM_MASK_SET(m_qEnabledServices, eService);
  }
  else
  {
    SLIM_MASK_CLEAR(m_qEnabledServices, eService);
  }
  if (0 == m_qEnabledServices)
  {
    slim_TimerStop(m_pzTimer);
  }
  else
  {
    slim_TimerStart(m_pzTimer, 500, eTIMER_DEFAULT);
  }
  return eSLIM_SUCCESS;
}

/**
 @brief Initiates time offset request.

 Function for making the time request. Successful response enable SLIM to
 calculate the offset between modem time and sensor time.

 @param[in] lTxnId Service transaction id.
 @return eSLIM_SUCCESS if time request is made successfully.
 */
slimErrorEnumT VehicleNetworkProvider::getTimeUpdate(int32_t lTxnId)
{
/*  return MultiplexingProvider::getTimeUpdate(lTxnId); */
  SLIM_LOG_ENTRY();

  SLIM_LOGD("Requesting time update: txn=%"PRId32, lTxnId);

  slimErrorEnumT retVal = eSLIM_ERROR_INTERNAL;

  if ( 0 != m_tLastBusControllerSyncTimestamp &&
       0 != m_tLastBusControllerSyncTime)
  {
    uint64_t t_Time = m_tLastBusControllerSyncTimestamp;
    t_Time += (slim_TimeTickGetMilliseconds() - m_tLastBusControllerSyncTime);

    routeTimeResponse(lTxnId, eSLIM_SUCCESS, t_Time, t_Time);
    SLIM_LOGD("Time update reported:%"PRIu64, t_Time);
    retVal = eSLIM_SUCCESS;
  }

  SLIM_LOGD("retVal= %d", retVal);
  return retVal;
}

