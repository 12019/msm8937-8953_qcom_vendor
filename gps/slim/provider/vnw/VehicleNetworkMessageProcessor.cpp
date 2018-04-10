/*
 Copyright (c) 2015 Qualcomm Technologies, Inc.
 All Rights Reserved.
 Qualcomm Technologies Proprietary and Confidential.
 */
/**
 @file
 @brief Vehicle Network provider implementation file.

 This file defines VNW message processing classes.
 */

#include <stdio.h>
#include <loc_cfg.h>
#include <slim_os_api.h>
#include <slim_os_log_api.h>
#include "VehicleNetworkProvider.h"
#include "VehicleNetworkMessageProcessor.h"
#include "BitStreamReader.h"

#undef SLIM_LOG_LOCAL_MODULE
#define SLIM_LOG_LOCAL_MODULE SLIM_LOG_MOD_VNW

namespace slim
{

/* -------------------------------------------------------------------------- */
VehicleNetworkMessageProcessor::VehicleNetworkMessageProcessor(
    VehicleNetworkProvider* pzParent) :
    m_pzParent(pzParent)
{
}

VehicleNetworkMessageProcessor::~VehicleNetworkMessageProcessor()
{
}

VehicleNetworkMessageProcessor* VehicleNetworkMessageProcessor::CreateProcessor
(
  VehicleNetworkProvider* pzParent,
  SensorConfig* pzCfg,
  slimServiceEnumT eService
)
{
  if(0 == pzParent || 0 == pzCfg)
  {
    return 0;
  }

  uint32_t iVersion = pzCfg->GetVersion();

  SLIM_LOGD("Creating processor for configuration version: %d, service: %d",
      iVersion, (int)eService);

  if (eSLIM_SERVICE_VEHICLE_ACCEL == eService &&
      VehicleNetworkConfiguration::eACCEL_CONFIG_DEFAULT == iVersion)
  {
    return new VehicleNetworkDefaultAccelMP(pzParent,
        static_cast<AccelConfig*>(pzCfg));
  }

  if (eSLIM_SERVICE_VEHICLE_GYRO == eService &&
      VehicleNetworkConfiguration::eGYRO_CONFIG_DEFAULT == iVersion)
  {
    return new VehicleNetworkDefaultGyroMP(pzParent,
        static_cast<GyroConfig*>(pzCfg));
  }

  if (eSLIM_SERVICE_VEHICLE_ODOMETRY == eService &&
      VehicleNetworkConfiguration::eODOMETRY_CONFIG_DEFAULT == iVersion)
  {
    return new VehicleNetworkDefaultOdometryMP(pzParent,
        static_cast<OdometryConfig*>(pzCfg));
  }

  if (eSLIM_SERVICE_VEHICLE_ACCEL == eService &&
      VehicleNetworkConfiguration::eACCEL_CONFIG_CONF1 == iVersion)
  {
    return new VehicleNetworkCONF1AccelMP(pzParent,
        static_cast<AccelCONF1Config*>(pzCfg));
  }

  if (eSLIM_SERVICE_VEHICLE_GYRO == eService &&
      VehicleNetworkConfiguration::eGYRO_CONFIG_CONF1 == iVersion)
  {
    return new VehicleNetworkCONF1GyroMP(pzParent,
        static_cast<GyroCONF1Config*>(pzCfg));
  }

  if (eSLIM_SERVICE_VEHICLE_ODOMETRY == eService &&
      VehicleNetworkConfiguration::eODOMETRY_CONFIG_CONF1 == iVersion)
  {
    return new VehicleNetworkCONF1OdometryMP(pzParent,
        static_cast<OdometryCONF1Config*>(pzCfg));
  }

  return 0;
}

void VehicleNetworkMessageProcessor::OnNewCanFrameEvent(CanFrameEvent* pzEvent)
{
  /* Just drop it by default */
  (void) pzEvent;
}

void VehicleNetworkMessageProcessor::OnNewTimerExpiryEvent(
    os_TimerExpiryType* pzEvent)
{
  /* Just drop it by default */
  (void) pzEvent;
}

void VehicleNetworkMessageProcessor::OnNewIpcMessage(slim_IpcMsgT* pzMsg)
{
  /* Just drop it by default */
  (void) pzMsg;
}

uint64_t VehicleNetworkMessageProcessor::VnTimeToApTime(uint64_t tVnTime)
{
  if(0 == m_pzParent)
  {
    return tVnTime;
  }
  double dR = m_pzParent->m_pzVehicleNetworkCfg->GetCanApClockRatio();
  return (uint64_t)((double)tVnTime / (0 == dR ? 1.0 : dR) );
}

void VehicleNetworkMessageProcessor::UpdateCanControllerTimeDelta
(
    uint64_t tMCTime
)
{
  if(0 == m_pzParent)
  {
    return;
  }

  m_pzParent->m_tLastBusControllerSyncTime = slim_TimeTickGetMilliseconds();
  m_pzParent->m_tLastBusControllerSyncTimestamp = VnTimeToApTime(tMCTime);
  SLIM_LOGD("Time delta update: %"PRIu64", %"PRIu64", %"PRIu64,
      m_pzParent->m_tLastBusControllerSyncTime,
      m_pzParent->m_tLastBusControllerSyncTimestamp,
      tMCTime);
}

/* -------------------------------------------------------------------------- */
VehicleNetworkCONF1AccelMP::VehicleNetworkCONF1AccelMP(
    VehicleNetworkProvider* pzParent, AccelCONF1Config* pzCfg) :
    VehicleNetworkMessageProcessor(pzParent), m_pzCfg(pzCfg)
{
}

void VehicleNetworkCONF1AccelMP::OnNewCanFrameEvent(CanFrameEvent* pzEvent)
{
  CwFrame* pzFrame = pzEvent->GetFrame();
  if (0 == pzFrame)
  {
    return;
  }

  if (0 < m_pzCfg->m_iFrameId
      && pzFrame->getId() == (uint32_t) m_pzCfg->m_iFrameId)
  {
    if (0
        != SLIM_MASK_IS_SET(m_pzParent->m_qEnabledServices,
            eSLIM_SERVICE_VEHICLE_ACCEL))
    {
      SLIM_LOGD("Accel CONF1 MP, event:%p", pzEvent);

      /* Update difference between AP and CAN bus mictocontroller clock.
       * TODO: This code has to be moved to processing of DR_SYNC CAN frame */
      UpdateCanControllerTimeDelta(pzFrame->getTimestamp());

      BitDivider bitStream(pzFrame->getDataPtr(), pzFrame->getDataLen());
      bitStream.SetPos(m_pzCfg->m_iOffsetX);
      if (bitStream.GetStatus() != 0)
        return;
      float x_Value = (float) (BitDivider::ExpandSign(
          bitStream.Get(m_pzCfg->m_iLenX), m_pzCfg->m_iLenX)
          * m_pzCfg->m_fScalingX);
      if (bitStream.GetStatus() != 0)
        return;
      bitStream.SetPos(m_pzCfg->m_iOffsetY);
      if (bitStream.GetStatus() != 0)
        return;
      float y_Value = (float) (BitDivider::ExpandSign(
          bitStream.Get(m_pzCfg->m_iLenY), m_pzCfg->m_iLenY)
          * m_pzCfg->m_fScalingY);
      if (bitStream.GetStatus() != 0)
        return;
      bitStream.SetPos(m_pzCfg->m_iOffsetZ);
      if (bitStream.GetStatus() != 0)
        return;
      float z_Value = (float) (BitDivider::ExpandSign(
          bitStream.Get(m_pzCfg->m_iLenZ), m_pzCfg->m_iLenZ)
          * m_pzCfg->m_fScalingZ);
      if (bitStream.GetStatus() != 0)
        return;

      slimVehicleSensorDataStructT zData;
      memset(&zData, 0, sizeof(zData));
      zData.timeBase = VnTimeToApTime(pzFrame->getTimestamp());
      zData.timeSource = eSLIM_TIME_SOURCE_UNSPECIFIED;
      zData.flags = 0;
      zData.provider = eSLIM_SERVICE_PROVIDER_NATIVE;
      zData.sensorType = eSLIM_VEHICLE_SENSOR_TYPE_ACCEL;
      zData.axesValidity = 0;
      zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_X_AXIS;
      zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_Y_AXIS;
      zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_Z_AXIS;
      zData.samples_len = 1;
      zData.samples[0].sampleTimeOffset = 0;
      zData.samples[0].sample[0] = x_Value;
      zData.samples[0].sample[1] = y_Value;
      zData.samples[0].sample[2] = z_Value;
      m_pzParent->routeIndication(eSLIM_SERVICE_VEHICLE_ACCEL, zData);
    }
  }
}

void VehicleNetworkCONF1AccelMP::ToString(char* pStr, size_t sLen)
{
  snprintf(pStr, sLen, "[VehicleNetworkCONF1AccelMP]");
}

/* -------------------------------------------------------------------------- */
VehicleNetworkDefaultAccelMP::VehicleNetworkDefaultAccelMP(
    VehicleNetworkProvider* pzParent, AccelConfig* pzCfg) :
    VehicleNetworkMessageProcessor(pzParent), m_pzCfg(pzCfg)
{
}

void VehicleNetworkDefaultAccelMP::OnNewTimerExpiryEvent(
    os_TimerExpiryType* pzEvent)
{
  VehicleNetworkMessageProcessor::OnNewTimerExpiryEvent(pzEvent);
  if (0
      != SLIM_MASK_IS_SET(m_pzParent->m_qEnabledServices,
          eSLIM_SERVICE_VEHICLE_ACCEL))
  {
    slimVehicleSensorDataStructT zData;
    memset(&zData, 0, sizeof(zData));
    zData.timeBase = slim_TimeTickGetMilliseconds();
    zData.timeSource = eSLIM_TIME_SOURCE_COMMON;
    zData.flags = 0;
    zData.provider = eSLIM_SERVICE_PROVIDER_NATIVE;
    zData.sensorType = eSLIM_VEHICLE_SENSOR_TYPE_ACCEL;
    zData.axesValidity = 0;
    zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_X_AXIS;
    zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_Y_AXIS;
    zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_Z_AXIS;
    zData.samples_len = 1;
    zData.samples[0].sampleTimeOffset = 10;
    zData.samples[0].sample[0] = .5f;
    zData.samples[0].sample[1] = .3f;
    zData.samples[0].sample[2] = -.2f;
    m_pzParent->routeIndication(eSLIM_SERVICE_VEHICLE_ACCEL, zData);
  }
}

void VehicleNetworkDefaultAccelMP::ToString(char* pStr, size_t sLen)
{
  snprintf(pStr, sLen, "[VehicleNetworkDefaultAccelMP]");
}

/* -------------------------------------------------------------------------- */
VehicleNetworkDefaultGyroMP::VehicleNetworkDefaultGyroMP(
    VehicleNetworkProvider* pzParent, GyroConfig* pzCfg) :
    VehicleNetworkMessageProcessor(pzParent), m_pzCfg(pzCfg)
{
}

void VehicleNetworkDefaultGyroMP::OnNewTimerExpiryEvent(
    os_TimerExpiryType* pzEvent)
{
  VehicleNetworkMessageProcessor::OnNewTimerExpiryEvent(pzEvent);
  if (0
      != SLIM_MASK_IS_SET(m_pzParent->m_qEnabledServices,
          eSLIM_SERVICE_VEHICLE_GYRO))
  {
    slimVehicleSensorDataStructT zData;
    memset(&zData, 0, sizeof(zData));
    zData.timeBase = slim_TimeTickGetMilliseconds();
    zData.timeSource = eSLIM_TIME_SOURCE_COMMON;
    zData.flags = 0;
    zData.provider = eSLIM_SERVICE_PROVIDER_NATIVE;
    zData.sensorType = eSLIM_VEHICLE_SENSOR_TYPE_GYRO;
    zData.axesValidity = 0;
    zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_X_AXIS;
    zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_Y_AXIS;
    zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_Z_AXIS;
    zData.samples_len = 1;
    zData.samples[0].sampleTimeOffset = 10;
    zData.samples[0].sample[0] = .3f;
    zData.samples[0].sample[1] = .5f;
    zData.samples[0].sample[2] = -.1f;
    m_pzParent->routeIndication(eSLIM_SERVICE_VEHICLE_GYRO, zData);
  }
}

void VehicleNetworkDefaultGyroMP::ToString(char* pStr, size_t sLen)
{
  //TODO: output full configuration
  snprintf(pStr, sLen, "[VehicleNetworkDefaultGyroMP]");
}

/* -------------------------------------------------------------------------- */
VehicleNetworkCONF1GyroMP::VehicleNetworkCONF1GyroMP(
    VehicleNetworkProvider* pzParent, GyroCONF1Config* pzCfg) :
    VehicleNetworkMessageProcessor(pzParent), m_pzCfg(pzCfg)
{
}

void VehicleNetworkCONF1GyroMP::OnNewCanFrameEvent(CanFrameEvent* pzEvent)
{
  CwFrame* pzFrame = pzEvent->GetFrame();
  if (0 == pzFrame)
  {
    return;
  }

  if (0 < m_pzCfg->m_iFrameId
      && pzFrame->getId() == (uint32_t) m_pzCfg->m_iFrameId)
  {
    if (0
        != SLIM_MASK_IS_SET(m_pzParent->m_qEnabledServices,
            eSLIM_SERVICE_VEHICLE_GYRO))
    {
      /* Update difference between AP and CAN bus mictocontroller clock.
       * TODO: This code has to be moved to processing of DR_SYNC CAN frame */
      UpdateCanControllerTimeDelta(pzFrame->getTimestamp());

      BitDivider bitStream(pzFrame->getDataPtr(), pzFrame->getDataLen());
      bitStream.SetPos(m_pzCfg->m_iOffsetX);
      if (bitStream.GetStatus() != 0)
        return;
      float x_Value = (float) (BitDivider::ExpandSign(
          bitStream.Get(m_pzCfg->m_iLenX), m_pzCfg->m_iLenX)
          * m_pzCfg->m_fScalingX);
      if (bitStream.GetStatus() != 0)
        return;
      bitStream.SetPos(m_pzCfg->m_iOffsetY);
      if (bitStream.GetStatus() != 0)
        return;
      float y_Value = (float) (BitDivider::ExpandSign(
          bitStream.Get(m_pzCfg->m_iLenY), m_pzCfg->m_iLenY)
          * m_pzCfg->m_fScalingY);
      if (bitStream.GetStatus() != 0)
        return;
      bitStream.SetPos(m_pzCfg->m_iOffsetZ);
      if (bitStream.GetStatus() != 0)
        return;
      float z_Value = (float) (BitDivider::ExpandSign(
          bitStream.Get(m_pzCfg->m_iLenZ), m_pzCfg->m_iLenZ)
          * m_pzCfg->m_fScalingZ);
      if (bitStream.GetStatus() != 0)
        return;

      slimVehicleSensorDataStructT zData;
      memset(&zData, 0, sizeof(zData));
      zData.timeBase = VnTimeToApTime(pzFrame->getTimestamp());
      zData.timeSource = eSLIM_TIME_SOURCE_UNSPECIFIED;
      zData.flags = 0;
      zData.provider = eSLIM_SERVICE_PROVIDER_NATIVE;
      zData.sensorType = eSLIM_VEHICLE_SENSOR_TYPE_GYRO;
      zData.axesValidity = 0;
      zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_X_AXIS;
      zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_Y_AXIS;
      zData.axesValidity |= SLIM_MASK_VEHICLE_SENSOR_Z_AXIS;
      zData.samples_len = 1;
      zData.samples[0].sampleTimeOffset = 0;
      zData.samples[0].sample[0] = x_Value;
      zData.samples[0].sample[1] = y_Value;
      zData.samples[0].sample[2] = z_Value;
      m_pzParent->routeIndication(eSLIM_SERVICE_VEHICLE_GYRO, zData);
    }
  }
}

void VehicleNetworkCONF1GyroMP::ToString(char* pStr, size_t sLen)
{
  snprintf(pStr, sLen, "[VehicleNetworkCONF1GyroMP]");
}

/* -------------------------------------------------------------------------- */
VehicleNetworkDefaultOdometryMP::VehicleNetworkDefaultOdometryMP(
    VehicleNetworkProvider* pzParent, OdometryConfig* pzCfg) :
    VehicleNetworkMessageProcessor(pzParent), m_pzCfg(pzCfg)
{
}

void VehicleNetworkDefaultOdometryMP::OnNewTimerExpiryEvent(
    os_TimerExpiryType* pzEvent)
{
  VehicleNetworkMessageProcessor::OnNewTimerExpiryEvent(pzEvent);
  if (0
      != SLIM_MASK_IS_SET(m_pzParent->m_qEnabledServices,
          eSLIM_SERVICE_VEHICLE_ODOMETRY))
  {
    slimVehicleOdometryDataStructT zData;
    memset(&zData, 0, sizeof(zData));
    zData.timeBase = slim_TimeTickGetMilliseconds();
    zData.timeSource = eSLIM_TIME_SOURCE_COMMON;
    zData.provider = eSLIM_SERVICE_PROVIDER_NATIVE;
    zData.wheelFlags = 0;
    zData.wheelFlags |= SLIM_MASK_VEHICLE_ODOMETRY_LEFT_AND_RIGHT_AVERAGE;
    zData.wheelFlags |= SLIM_MASK_VEHICLE_ODOMETRY_LEFT;
    zData.wheelFlags |= SLIM_MASK_VEHICLE_ODOMETRY_RIGHT;
    zData.samples_len = 1;
    zData.samples[0].sampleTimeOffset = 10;
    zData.samples[0].distanceTravelled[0] = 1;
    zData.samples[0].distanceTravelled[1] = 2;
    zData.samples[0].distanceTravelled[2] = 3;
    m_pzParent->routeIndication(zData);
  }
}

void VehicleNetworkDefaultOdometryMP::ToString(char* pStr, size_t sLen)
{
  snprintf(pStr, sLen, "[VehicleNetworkDefaultOdometryMP]");
}

/* -------------------------------------------------------------------------- */
VehicleNetworkCONF1OdometryMP::VehicleNetworkCONF1OdometryMP(
    VehicleNetworkProvider* pzParent, OdometryCONF1Config* pzCfg) :
    VehicleNetworkMessageProcessor(pzParent), m_pzCfg(pzCfg)
{
}

void VehicleNetworkCONF1OdometryMP::OnNewCanFrameEvent(CanFrameEvent* pzEvent)
{
  CwFrame* pzFrame = pzEvent->GetFrame();
  if (0 == pzFrame)
  {
    return;
  }

  if (0 < m_pzCfg->m_iFrameId
      && pzFrame->getId() == (uint32_t) m_pzCfg->m_iFrameId)
  {
    if (0
        != SLIM_MASK_IS_SET(m_pzParent->m_qEnabledServices,
            eSLIM_SERVICE_VEHICLE_ODOMETRY))
    {
      /* Update difference between AP and CAN bus mictocontroller clock.
       * TODO: This code has to be moved to processing of DR_SYNC CAN frame */
      UpdateCanControllerTimeDelta(pzFrame->getTimestamp());

      BitDivider bitStream(pzFrame->getDataPtr(), pzFrame->getDataLen());
      bitStream.SetPos(m_pzCfg->m_iReverseBitOffset);
      if (bitStream.GetStatus() != 0)
        return;
      uint32_t reverse = bitStream.Get(1);
      if (bitStream.GetStatus() != 0)
        return;
      bitStream.SetPos(m_pzCfg->m_iSourceOffset);
      if (bitStream.GetStatus() != 0)
        return;
      uint32_t src = bitStream.Get(m_pzCfg->m_iSourceLen);
      if (bitStream.GetStatus() != 0)
        return;
      bitStream.SetPos(m_pzCfg->m_iDistanceOffset);
      if (bitStream.GetStatus() != 0)
        return;
      uint32_t distance = bitStream.Get(m_pzCfg->m_iDistanceLen);
      if (bitStream.GetStatus() != 0)
        return;

      slimVehicleOdometryDataStructT zData;
      memset(&zData, 0, sizeof(zData));
      zData.timeBase = VnTimeToApTime(pzFrame->getTimestamp());
      zData.timeSource = eSLIM_TIME_SOURCE_UNSPECIFIED;
      zData.provider = eSLIM_SERVICE_PROVIDER_NATIVE;
      zData.distanceTravelledBase = 0;
      zData.odometryFlags = 0;
      if (reverse != 0)
      {
        zData.odometryFlags |= SLIM_MASK_VEHICLE_ODOMETRY_REVERSE_MOVEMENT;
      }
      zData.wheelFlags = 0;
      if (src & OdometryCONF1Config::eODO_CFG1_SRC_LEFT)
      {
        zData.wheelFlags |= SLIM_MASK_VEHICLE_ODOMETRY_LEFT;
        zData.samples[0].distanceTravelled[1] = distance;
      }
      else if (src & OdometryCONF1Config::eODO_CFG1_SRC_RIGHT)
      {
        zData.wheelFlags |= SLIM_MASK_VEHICLE_ODOMETRY_RIGHT;
        zData.samples[0].distanceTravelled[2] = distance;
      }
      else if (src & OdometryCONF1Config::eODO_CFG1_SRC_LR_AVR)
      {
        zData.wheelFlags |= SLIM_MASK_VEHICLE_ODOMETRY_LEFT_AND_RIGHT_AVERAGE;
        zData.samples[0].distanceTravelled[0] = distance;
      }

      if (zData.wheelFlags == 0)
      {
        zData.samples_len = 0;
      }
      else
      {
        zData.samples_len = 1;
        zData.samples[0].sampleTimeOffset = 0;
      }
      m_pzParent->routeIndication(zData);
    }
  }
}

void VehicleNetworkCONF1OdometryMP::ToString(char* pStr, size_t sLen)
{
  snprintf(pStr, sLen, "[VehicleNetworkCONF1OdometryMP]");
}

}


