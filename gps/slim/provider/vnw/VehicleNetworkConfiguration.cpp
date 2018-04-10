/*
Copyright (c) 2015 Qualcomm Technologies, Inc.
All Rights Reserved.
Qualcomm Technologies Proprietary and Confidential.
*/
/**
@file
@brief Vehicle Network configuration interface file.

This file defines VNW configuration object implementation.
*/
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <new>
#include <loc_cfg.h>
#include <inttypes.h>
#include <slim_os_log_api.h>
#include "VehicleNetworkConfiguration.h"

/*----------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 * -------------------------------------------------------------------------*/

#define MAX_VN_CFG_STR_LEN           256
#define VN_PROC_CLOCK_RATIO_STR_ID   "VN_PROC_CLOCK_RATIO"
#define VN_ACCEL_CFG_STR_ID          "VN_ACCEL_CFG"
#define VN_GYRO_CFG_STR_ID           "VN_GYRO_CFG"
#define VN_ODOMETRY_CFG_STR_ID       "VN_ODOMETRY_CFG"

#undef SLIM_LOG_LOCAL_MODULE
#define SLIM_LOG_LOCAL_MODULE SLIM_LOG_MOD_VNW

namespace slim
{

const char* const VehicleNetworkConfiguration::VN_CFG_FILE_NAME =
    "/etc/sap.conf";

// See header file for function documentation
VehicleNetworkConfiguration::VehicleNetworkConfiguration()
  : m_pzAccelConfig(0)
  , m_pzGyroConfig(0)
  , m_pzOdometryConfig(0)
  , m_dCanApClockRatio(0.0)
{
}

// See header file for function documentation
VehicleNetworkConfiguration::~VehicleNetworkConfiguration()
{
  delete m_pzAccelConfig;
  delete m_pzGyroConfig;
  delete m_pzOdometryConfig;
}

// See header file for function documentation
int32_t VehicleNetworkConfiguration::GetCfgVersion(const char* pCfgStr) const
{
  /* Configuration string version */
  return strchr(pCfgStr, ':') ? (int32_t)atoi(pCfgStr) : -1;
}

// See header file for function documentation
SensorConfig* VehicleNetworkConfiguration::GetAccelConfig() const
{
  return m_pzAccelConfig;
}

// See header file for function documentation
SensorConfig* VehicleNetworkConfiguration::GetGyroConfig() const
{
  return m_pzGyroConfig;
}

// See header file for function documentation
SensorConfig* VehicleNetworkConfiguration::GetOdometryConfig() const
{
  return m_pzOdometryConfig;
}

// See header file for function documentation
double VehicleNetworkConfiguration::GetCanApClockRatio() const
{
  return m_dCanApClockRatio;
}

// See header file for function documentation
SensorConfig* VehicleNetworkConfiguration::LoadAccelerometerConfig
(const char* pStr)
{
  SensorConfig* pzAC = 0;
  int v = GetCfgVersion(pStr);
  SLIM_LOGD("Accel configuration version v= %d", v);
  if (v < 0) return 0;
  switch (v)
  {
  case eACCEL_CONFIG_DEFAULT:
    {
      pzAC = new (std::nothrow) AccelConfig();
    }
    break;
  case eACCEL_CONFIG_CONF1:
    {
      pzAC = new (std::nothrow) AccelCONF1Config();
    }
    break;
  /* Other configuration versions to be added here */
  default:
    break;
  }

  if (0 != pzAC)
  {
    pzAC->Load(pStr);
  }
  return pzAC;
}

// See header file for function documentation
SensorConfig* VehicleNetworkConfiguration::LoadGyroConfig(const char* pStr)
{
  SensorConfig* pzGC = 0;
  int v = GetCfgVersion(pStr);
  if (v < 0) return 0;
  switch (v)
  {
  case eGYRO_CONFIG_DEFAULT:
    {
      pzGC = new (std::nothrow) GyroConfig();
    }
    break;
  case eGYRO_CONFIG_CONF1:
    {
      pzGC= new (std::nothrow) GyroCONF1Config();
    }
    break;
  /* Other configuration versions to be added here */
  default:
    break;
  }

  if (0 != pzGC)
  {
    pzGC->Load(pStr);
  }
  return pzGC;
}

// See header file for function documentation
SensorConfig* VehicleNetworkConfiguration::LoadOdometryConfig(const char* pStr)
{
  SensorConfig* pzOC = 0;
  int v = GetCfgVersion(pStr);
  if (v < 0) return 0;
  switch (v)
  {
  case eODOMETRY_CONFIG_DEFAULT: // default implementation
    {
      pzOC = new (std::nothrow) OdometryConfig();
    }
    break;
  case eODOMETRY_CONFIG_CONF1:
    {
      pzOC= new (std::nothrow) OdometryCONF1Config();
    }
    break;
  /* Other configuration versions to be added here */
  default:
    break;
  }

  if (0 != pzOC)
  {
    pzOC->Load(pStr);
  }

  return pzOC;
}

// See header file for function documentation
void VehicleNetworkConfiguration::Load()
{
  char pzAccelCfgStr[MAX_VN_CFG_STR_LEN] = {0};
  char pzGyroCfgStr[MAX_VN_CFG_STR_LEN] = {0};
  char pzOdometryCfgStr[MAX_VN_CFG_STR_LEN] = {0};
  char pzClockRatioCfgStr[] = {0};
  uint8_t uAccelCfgStrSet = 0;
  uint8_t uGyroCfgStrSet = 0;
  uint8_t uOdometryCfgStrSet = 0;
  uint8_t uClockRatioCfgStrSet = 0;

  loc_param_s_type z_vnParameterTable[] =
  {
  { VN_PROC_CLOCK_RATIO_STR_ID, pzClockRatioCfgStr, &uClockRatioCfgStrSet, 's'},
  { VN_ACCEL_CFG_STR_ID, pzAccelCfgStr, &uAccelCfgStrSet, 's' },
  { VN_GYRO_CFG_STR_ID, pzGyroCfgStr, &uGyroCfgStrSet, 's' },
  { VN_ODOMETRY_CFG_STR_ID, pzOdometryCfgStr, &uOdometryCfgStrSet, 's'}
  };

  UTIL_READ_CONF(VN_CFG_FILE_NAME, z_vnParameterTable);

  if (0 != uClockRatioCfgStrSet)
  {
    char* rptr = NULL;
    m_dCanApClockRatio = strtod(pzClockRatioCfgStr, &rptr);
    if ((ERANGE == errno) ||
        (pzClockRatioCfgStr == rptr))
    {
      m_dCanApClockRatio = 0.0;
    }
  }

  m_pzAccelConfig = (0 != uAccelCfgStrSet) ?
      VehicleNetworkConfiguration::LoadAccelerometerConfig(pzAccelCfgStr) : 0;
  m_pzGyroConfig = (0 != uGyroCfgStrSet) ?
      VehicleNetworkConfiguration::LoadGyroConfig(pzGyroCfgStr) : 0;
  m_pzOdometryConfig = (0 != uOdometryCfgStrSet) ?
      VehicleNetworkConfiguration::LoadOdometryConfig(pzOdometryCfgStr) : 0;
}

// See header file for function documentation
void VehicleNetworkConfiguration::ToString(char* pStr, size_t sLen) const
{
  if (0 == pStr || 0 == sLen)
  {
    return;
  }

  snprintf(pStr, sLen, "[file:'%s', m_dCanApClockRatio:%f, "
                       "m_pzAccelConfig=%"PRIxPTR", "
                       "m_pzGyroConfig=%"PRIxPTR", "
                       "m_pzOdometryConfig=%"PRIxPTR"]",
           VN_CFG_FILE_NAME,
           m_dCanApClockRatio,
           (uintptr_t)m_pzAccelConfig,
           (uintptr_t)m_pzGyroConfig,
           (uintptr_t)m_pzOdometryConfig);
}


/* -------------------------------------------------------------------------- */
// See header file for function documentation
SensorConfig::SensorConfig(uint32_t iCfgVersion)
  : m_pCfgStr(0)
  , m_qCfgVersion(iCfgVersion)
{
}

// See header file for function documentation
SensorConfig::~SensorConfig()
{
  delete m_pCfgStr;
}

// See header file for function documentation
void SensorConfig::Load(const char* pStr)
{
  if (0 == pStr || 0 == *pStr)
  {
    return;
  }

  /* Save a copy of the configuration string */
  size_t len = strlen(pStr);
  len = len < MAX_VN_CFG_STR_LEN ? (len + 1) : MAX_VN_CFG_STR_LEN;
  m_pCfgStr = new (std::nothrow) char[len];
  if (0 != m_pCfgStr)
  {
    strlcpy(m_pCfgStr, pStr, len);
  }
}

// See header file for function documentation
uint32_t SensorConfig::GetVersion() const
{
  return m_qCfgVersion;
}

// See header file for function documentation
bool SensorConfig::CfgValid() const
{
  return (0 != m_pCfgStr) &&
         (VehicleNetworkConfiguration::eCFG_VERSION_MAX >= m_qCfgVersion);
}

// See header file for function documentation
void SensorConfig::ToString(char* pStr, size_t sLen) const
{
  if (0 == pStr || 0 == sLen)
  {
    return;
  }

  snprintf(pStr, sLen, "[str:'%s', v:%d]",
           (0 != m_pCfgStr) ? m_pCfgStr : "null",
           (int)m_qCfgVersion);
}

// See header file for function documentation
void SensorConfig::GetNextFloat(float* pfX, char** pFBegin, float fDefault)
{
  if (0 == pfX)
  {
    return;
  }

  if (0 == pFBegin || 0 == *pFBegin || 0 == **pFBegin)
  {
    *pfX = fDefault;
    return;
  }

  char* pDelim = 0;
  *pfX = strtod(*pFBegin, &pDelim);
  *pFBegin = pDelim;
}

// See header file for function documentation
void SensorConfig::GetNextInt(int* piX, char** pFBegin, int iDefault)
{
  if (0 == piX)
  {
    return;
  }

  if (0 == pFBegin || 0 == *pFBegin || 0 == **pFBegin)
  {
    *piX = iDefault;
    return;
  }

  char* pDelim = 0;
  *piX = strtol(*pFBegin, &pDelim, 10);
  *pFBegin = pDelim;
}

/* -------------------------------------------------------------------------- */
// See header file for function documentation
AccelConfig::AccelConfig()
  : SensorConfig(VehicleNetworkConfiguration::eACCEL_CONFIG_DEFAULT)
  , m_fPeriod(0)
{
}

// See header file for function documentation
void AccelConfig::Load(const char* pStr)
{
  if (0 == pStr || 0 == *pStr)
  {
    return;
  }

  SensorConfig::Load(pStr);

  if (0 == m_pCfgStr)
  {
    return;
  }
  m_fPeriod = 0;
  char* pFieldbegin = strchr(m_pCfgStr, ':');
  if (!pFieldbegin) return;
  pFieldbegin++;
  GetNextFloat(&m_fPeriod, &pFieldbegin, -1);

}

// See header file for function documentation
bool AccelConfig::CfgValid() const
{
  return SensorConfig::CfgValid() &&
         VehicleNetworkConfiguration::eACCEL_CONFIG_DEFAULT == m_qCfgVersion &&
         1.0 <= m_fPeriod && 10.0 >= m_fPeriod;
}

// See header file for function documentation
void AccelConfig::ToString(char* pStr, size_t sLen) const
{
  if (0 == pStr || 0 == sLen)
  {
    return;
  }

  char pzTStr[1024];
  SensorConfig::ToString(pzTStr, sizeof(pzTStr));
  snprintf(pStr, sLen, "[%s, period:%f]", pzTStr, m_fPeriod);
}

/* -------------------------------------------------------------------------- */
// See header file for function documentation
GyroConfig::GyroConfig()
  : SensorConfig(VehicleNetworkConfiguration::eGYRO_CONFIG_DEFAULT)
  , m_fPeriod(0)
{
}

// See header file for function documentation
void GyroConfig::Load(const char* pStr)
{
  if (0 == pStr || 0 == *pStr)
  {
    return;
  }

  SensorConfig::Load(pStr);

  if (0 == m_pCfgStr)
  {
    return;
  }

  m_fPeriod = 0;
  char* pFieldbegin = strchr(m_pCfgStr, ':');
  if (!pFieldbegin) return;
  pFieldbegin++;
  GetNextFloat(&m_fPeriod, &pFieldbegin, -1);
}

// See header file for function documentation
bool GyroConfig::CfgValid() const
{
  return SensorConfig::CfgValid() &&
         VehicleNetworkConfiguration::eGYRO_CONFIG_DEFAULT == m_qCfgVersion &&
         1.0 <= m_fPeriod && 10.0 >= m_fPeriod;
}

// See header file for function documentation
void GyroConfig::ToString(char* pStr, size_t sLen) const
{
  if (0 == pStr || 0 == sLen)
  {
    return;
  }

  char pzTStr[1024];
  SensorConfig::ToString(pzTStr, sizeof(pzTStr));
  snprintf(pStr, sLen, "[%s, period:%f]", pzTStr, m_fPeriod);
}

/* -------------------------------------------------------------------------- */
// See header file for function documentation
OdometryConfig::OdometryConfig()
  : SensorConfig(VehicleNetworkConfiguration::eODOMETRY_CONFIG_DEFAULT)
  , m_fAverageSpeed(0)
  , m_fPeriod(0)
{
}

// See header file for function documentation
void OdometryConfig::Load(const char* pStr)
{
  if (0 == pStr || 0 == *pStr)
  {
    return;
  }

  SensorConfig::Load(pStr);

  if (0 == m_pCfgStr)
  {
    return;
  }

  m_fPeriod = 0;
  m_fAverageSpeed = 0;
  char* pFieldbegin = strchr(m_pCfgStr, ':');
  if (!pFieldbegin) return;
  pFieldbegin++;
  GetNextFloat(&m_fPeriod, &pFieldbegin, -1);
  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextFloat(&m_fAverageSpeed, &pFieldbegin, -1);
  }
}

// See header file for function documentation
bool OdometryConfig::CfgValid() const
{
  return SensorConfig::CfgValid() &&
       VehicleNetworkConfiguration::eODOMETRY_CONFIG_DEFAULT == m_qCfgVersion &&
       1.0 <= m_fPeriod && 10.0 >= m_fPeriod &&
       1.0 <= m_fAverageSpeed && 25.0 >= m_fAverageSpeed;
}

// See header file for function documentation
void OdometryConfig::ToString(char* pStr, size_t sLen) const
{
  if (0 == pStr || 0 == sLen)
  {
    return;
  }

  char pzTStr[1024];
  SensorConfig::ToString(pzTStr, sizeof(pzTStr));
  snprintf(pStr, sLen, "[%s, period:%f, avrSpeed:%f]",
           pzTStr, m_fPeriod, m_fAverageSpeed);
}

/* -------------------------------------------------------------------------- */
// See header file for function documentation
AccelCONF1Config::AccelCONF1Config()
  : SensorConfig(VehicleNetworkConfiguration::eACCEL_CONFIG_CONF1)
  , m_iFrameId(-1)
  , m_iOffsetX(0)
  , m_iLenX(0)
  , m_fScalingX(0)
  , m_iOffsetY(0)
  , m_iLenY(0)
  , m_fScalingY(0)
  , m_iOffsetZ(0)
  , m_iLenZ(0)
  , m_fScalingZ(0)
{
}

// See header file for function documentation
void AccelCONF1Config::Load(const char* pStr)
{
  if (0 == pStr || 0 == *pStr)
  {
    return;
  }

  SensorConfig::Load(pStr);

  if (0 == m_pCfgStr)
  {
    return;
  }

  char* pFieldbegin = strchr(m_pCfgStr, ':');
  if (!pFieldbegin)
  {
    return;
  }

  pFieldbegin++;
  GetNextInt(&m_iFrameId, &pFieldbegin, -1);

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iOffsetX, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iLenX, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextFloat(&m_fScalingX, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iOffsetY, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iLenY, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextFloat(&m_fScalingY, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iOffsetZ, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iLenZ, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextFloat(&m_fScalingZ, &pFieldbegin, -1);
  }
}

// See header file for function documentation
bool AccelCONF1Config::CfgValid() const
{
  return SensorConfig::CfgValid() &&
         VehicleNetworkConfiguration::eACCEL_CONFIG_CONF1 == m_qCfgVersion &&
         0 <= m_iOffsetX && 0 <= m_iOffsetY && 0 <= m_iOffsetZ &&
         0 < m_iLenX && 0 < m_iLenY && 0 < m_iLenZ;
}

// See header file for function documentation
void AccelCONF1Config::ToString(char* pStr, size_t sLen) const
{
  if (0 == pStr || 0 == sLen)
  {
    return;
  }

  char pzTStr[1024];
  SensorConfig::ToString(pzTStr, sizeof(pzTStr));
  snprintf(pStr, sLen, "[%s, %d, %d, %d, %.6E, %d, %d, %.6E, %d, %d, %.6E]",
           pzTStr, m_iFrameId, m_iOffsetX, m_iLenX, m_fScalingX, m_iOffsetY,
           m_iLenY, m_fScalingY, m_iOffsetZ, m_iLenZ, m_fScalingZ);
}

/* -------------------------------------------------------------------------- */
// See header file for function documentation
GyroCONF1Config::GyroCONF1Config()
  : SensorConfig(VehicleNetworkConfiguration::eGYRO_CONFIG_CONF1)
  , m_iFrameId(-1)
  , m_iOffsetX(0)
  , m_iLenX(0)
  , m_fScalingX(0)
  , m_iOffsetY(0)
  , m_iLenY(0)
  , m_fScalingY(0)
  , m_iOffsetZ(0)
  , m_iLenZ(0)
  , m_fScalingZ(0)
{
}

// See header file for function documentation
void GyroCONF1Config::Load(const char* pStr)
{
  if (0 == pStr || 0 == *pStr)
  {
    return;
  }

  SensorConfig::Load(pStr);

  if (0 == m_pCfgStr)
  {
    return;
  }

  m_iOffsetX = 0;
  m_iLenX = 0;
  m_fScalingX = 0;
  m_iOffsetY = 0;
  m_iLenY = 0;
  m_fScalingY = 0;
  m_iOffsetZ = 0;
  m_iLenZ = 0;
  m_fScalingZ = 0;

  char* pFieldbegin = strchr(m_pCfgStr, ':');
  if (0 == pFieldbegin)
  {
    return;
  }

  pFieldbegin++;
  GetNextInt(&m_iFrameId, &pFieldbegin, -1);

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iOffsetX, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iLenX, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextFloat(&m_fScalingX, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iOffsetY, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iLenY, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextFloat(&m_fScalingY, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iOffsetZ, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iLenZ, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextFloat(&m_fScalingZ, &pFieldbegin, -1);
  }
}

// See header file for function documentation
bool GyroCONF1Config::CfgValid() const
{
  return SensorConfig::CfgValid() &&
         VehicleNetworkConfiguration::eGYRO_CONFIG_CONF1 == m_qCfgVersion &&
         0 <= m_iOffsetX && 0 <= m_iOffsetY && 0 <= m_iOffsetZ &&
         0 < m_iLenX && 0 < m_iLenY && 0 < m_iLenZ;
}

// See header file for function documentation
void GyroCONF1Config::ToString(char* pStr, size_t sLen) const
{
  if (0 == pStr || 0 == sLen)
  {
    return;
  }

  char pzTStr[1024];
  SensorConfig::ToString(pzTStr, sizeof(pzTStr));
  snprintf(pStr, sLen, "[%s, %d, %d, %d, %.6E, %d, %d, %.6E, %d, %d, %.6E]",
           pzTStr, m_iFrameId, m_iOffsetX, m_iLenX, m_fScalingX, m_iOffsetY,
           m_iLenY, m_fScalingY, m_iOffsetZ, m_iLenZ, m_fScalingZ);
}

/* -------------------------------------------------------------------------- */
// See header file for function documentation
OdometryCONF1Config::OdometryCONF1Config()
  : SensorConfig(VehicleNetworkConfiguration::eODOMETRY_CONFIG_CONF1)
  , m_iFrameId(-1)
  , m_iReverseBitOffset(0)
  , m_iSourceOffset(0)
  , m_iSourceLen(0)
  , m_iDistanceOffset(0)
  , m_iDistanceLen(0)
{
}

// See header file for function documentation
void OdometryCONF1Config::Load(const char* pStr)
{
  if (0 == pStr || 0 == *pStr)
  {
    return;
  }

  SensorConfig::Load(pStr);

  if (0 == m_pCfgStr)
  {
    return;
  }

  char* pFieldbegin = strchr(m_pCfgStr, ':');
  if (0 == pFieldbegin)
  {
    return;
  }

  pFieldbegin++;
  GetNextInt(&m_iFrameId, &pFieldbegin, -1);

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iReverseBitOffset, &pFieldbegin, -1);
  }

  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iSourceOffset, &pFieldbegin, -1);
  }
  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iSourceLen, &pFieldbegin, -1);
  }
  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iDistanceOffset, &pFieldbegin, -1);
  }
  if (',' == *pFieldbegin)
  {
    pFieldbegin++;
    GetNextInt(&m_iDistanceLen, &pFieldbegin, -1);
  }
}

// See header file for function documentation
bool OdometryCONF1Config::CfgValid() const
{
  return SensorConfig::CfgValid() &&
      VehicleNetworkConfiguration::eODOMETRY_CONFIG_CONF1 == m_qCfgVersion &&
      0 <= m_iReverseBitOffset && 0 <= m_iSourceOffset && 0 < m_iSourceLen &&
      0 <= m_iDistanceOffset && 0 < m_iDistanceLen;
}

// See header file for function documentation
void OdometryCONF1Config::ToString(char* pStr, size_t sLen) const
{
  if (0 == pStr || 0 == sLen)
  {
    return;
  }

  char pzTStr[1024];
  SensorConfig::ToString(pzTStr, sizeof(pzTStr));
  snprintf(pStr, sLen, "[%s, %d, %d, %d, %d, %d, %d]",
           pzTStr, m_iFrameId, m_iReverseBitOffset, m_iSourceOffset,
           m_iSourceLen, m_iDistanceOffset, m_iDistanceLen);
}

}

