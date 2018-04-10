#ifndef __MUX_MP2BASEFile_H__
#define __MUX_MP2BASEFILE_H__

/* =======================================================================
                              MP2BaseFile.h
DESCRIPTION

  Copyright (c) 2011 - 2015 Qualcomm Technologies, Inc.All Rights Reserved.
  Qualcomm Technologies Proprietary and Confidential.
========================================================================== */


/* =======================================================================
                             Edit History
$Source: //source/qcom/qct/multimedia2/Video/Source/FileMux/Sink/FileMux/MP2BaseFileLib/main/latest/inc/MP2BaseFile.h

========================================================================== */

/* =======================================================================
**               Includes and Public Data Declarations
** ======================================================================= */

/* ==========================================================================

                     INCLUDE FILES FOR MODULE

========================================================================== */
#include "sys/uio.h"
#include "MMTimer.h"
#include "oscl_file_io.h"
#include "filemuxtypes.h"
#include "muxbase.h"
#include "MMCriticalSection.h"
/* =======================================================================

                        DATA DECLARATIONS

========================================================================== */
/* -----------------------------------------------------------------------
** Constant / Define Declarations
** ----------------------------------------------------------------------- */
#define FS_FILE_EXTENSION_DOT              '.'
#define ASYNC_WRITER_FRAGMENT_BUF_SIZE(x)     (1024 * 1024)
#define ASYNC_WRITER_BUF_SIZE(x)              (256 * 1024)
#define MUX_MEM_MARGIN                        65536
#define EFS_MARGIN                            65536
#define CHUNK_TRANSFER_SIZE                   1000
#define TS_RTP_PAYLOAD_SIZE                   (7 * 188)
#define TS_HDR_POOL_SIZE                      7
#define IOV_VEC_POOL_SIZE                     TS_HDR_POOL_SIZE * 2 + 1

/* -----------------------------------------------------------------------
** Type Declarations
** ----------------------------------------------------------------------- */

//! MEM file object type
typedef struct MP2_MUX_mem_object_struct {
    uint8     *buf_ptr;
    uint32    buf_size;
    uint32    length;
    uint32    pos;
    boolean   free_buf;
} MP2_MUX_mem_object_type;


#ifdef FILEMUX_WRITE_ASYNC
typedef struct mp2_stream_handler_info
{
    uint8     *pBuffer;        //!Buffer pointer
    uint32     pHead;          //!Head of the buf
    uint32     pTail;          //!Tail of the buf
    uint32     nSize;          //!Total Size of the buf
    uint32     nFlushThreshold;//!Threshold to start flushing the buffer
    boolean    bOverflow;      //!Queue overflow
    uint32     nBytesOverflow; //!Number of bytes that is overflown
    boolean    bFlushing;      //!Buffer is being drained
}Mux_mp2_stream_handler_info_type;
#endif /*FILEMUX_WRITE_ASYNC*/

typedef struct MP2_MUX_file_struct
{
  MP2_MUX_mem_object_type *mem_file;
  OSCL_FILE *efs_file;
}MP2_MUX_file;

typedef struct MP2OutputParamStruct
{
    uint8         *pMP2TSHdr;          //!Pointer to TS header
    uint32         nMP2TSHdrLen;       //!TS header length
    uint8         *pPESHdr;            //!Pointer to PES header
    uint32         nPESHdrLen;         //!PES header length
    uint8          *pData;             //!Pointer to data
    uint32         nDataLen;           //!Data length
    bool           bEOF;               //!End of stream flag
}MP2OutputParam;

typedef enum Mux_AAC_Format
{
    AAC_FORMAT_ADTS,
    AAC_FORMAT_LOAS
}Mux_AAC_Format_Type;
/*!
  @brief   MP2BaseFile Module.

  @detail This module is to record the MP2 streams
*/
class MP2BaseFile : public MUXBase
{
public:
    //! constructor
    MP2BaseFile( MUX_create_params_type *,
                 MUX_fmt_type file_format,
                 MUX_brand_type file_brand,
                 MUX_handle_type *output_handle
                 );
    //! destructor
    virtual ~MP2BaseFile();


/* =======================================================================
**                          Function Declarations
** ======================================================================= */
virtual MUX_STATUS MUX_Process_Sample( uint32 stream_number,
                                        uint32 num_samples,
                                        const MUX_sample_info_type  *sample_info,
                                        const uint8  *sample_data);
virtual MUX_STATUS MUX_write_header (uint32 stream_id, uint32 header_size, const uint8 *header);

virtual MUX_STATUS MUX_update_AVC_Timing_HRD_Params
                                    (MUX_AVC_TimingHRD_params_type *pHRDParams);
virtual MUX_STATUS MUX_update_streamport(uint64_t port);

virtual MUX_STATUS MUX_end_Processing();

virtual MUX_STATUS MUX_get_current_PTS(uint64 *pnPTS);

virtual MUX_STATUS MUX_toggle_PMT_version();

#if 0 //def FILEMUX_WRITE_ASYNC
virtual int FlushStream(void);
#endif /* FILEMUX_WRITE_ASYNC */
protected:
void MUX_Modify_AVC_Start_Code(
                            const uint8 * pBuffer,
                            uint32*  nSize,
                            bool bHeader);

uint32 MUX_FindNextPattern(const uint8 *streamBuf,
                           uint32 bufSize,
                           uint32 startcode);

uint32 FindCheckSum(uint8 *pData,uint32 nSize);


void InitData ();
void OpenFiles();
uint32 GenerateTablesAndPCR(uint32   nStreamNumber,
                            MUX_sample_info_type  *pSampleInfo);
void GeneratePATPacket(uint8    *pMP2TSPacket);
void GeneratePMTPacket(uint8    *pMP2TSPacket);
uint16 GeneratePESPacket( uint32   stream_number,
                         const MUX_sample_info_type   *pSampleInfo,
                         uint8  *PES_Header );
uint32 GenerateMP2TSPacket(uint32                nStreamNumber,
                           MUX_sample_info_type  *pSampleInfo,
                           const  uint16         nPESHeaderSize,
                           const  uint32         nSampleDataOffset,
                           uint8                 *pMP2TSPacket,
                           uint32                *pTSHdrSz);

bool Destination_Mmc(const WCHAR *);
boolean OutputTS(MP2OutputParam         *pOpParam,
                 struct iovec           *pIOV,
                 uint32                 *pIOVnIndex,
                 uint32                 *pTSStreamBufSz,
                 uint32                 nOffset);
MUX_STATUS packetizeStream(uint32 nStreamNumber,
                                MUX_sample_info_type  *pCurrentSampleInfo,
                                const uint8           *pSampleData);

#ifdef FILEMUX_WRITE_ASYNC
    bool   OpenFileHandler();
    void   CloseFileHandler();
    uint32 GetStreamBufferOccupancy(Mux_mp2_stream_handler_info_type  *m_sStreamHandler);
    uint32 PushStream(const uint8  *data, uint32 offset, uint32 len);
#endif /*  FILEMUX_WRITE_ASYNC */

uint32 Space_Check(void);
void handle_close ();
void close_writer (void);
static void timerCallback(void *pMp2File);

bool GenerateAVCHRDTimingDescriptor
                        (MUX_AVC_TimingHRD_params_type *pAVCHrdParams);

#ifdef FILEMUX_WRITE_ASYNC
    mux_write_file_cb_func_ptr_type               pFnFileWrite;
    void*                                         pClientData;
    uint32                                        output_unit_size;
    Mux_mp2_stream_handler_info_type              m_sStreamHandler;
    bool                                          closeissued;
    uint8                                         mdatAtomSize[4];
    MM_HANDLE                                     MP2BaseFileMux_CS;
    MM_HANDLE                                     pBuffAvailSig;
    MM_HANDLE                                     pBuffManSigQ;
    static const uint32                           BUF_AVAIL_EVENT;
#endif /* FILEMUX_WRITE_ASYNC  */
MUX_create_params_type                        *mpParams;
MUX_brand_type                                meFileBrand;
boolean                                       mbAudioPresent;
boolean                                       mbVideoPresent;
boolean                                       mbGenericPresent;
bool                                          mbToggleVersionChange;
bool                                          mbAdjustTSforCompliance;
uint8                                         mnAudioStreamNum;
Mux_AAC_Format_Type                           meAACFormat;
uint32                                        mnAudioFrameRate;
uint32                                        mnAudioFrameDuration90Khz;
uint64                                        mnCurrAudioTime;
uint8                                         mnVideoStreamNum;
uint32                                        mnVideoFrameRate;
uint64                                        mnCurrVideoTime;
bool                                          mbSendFillerNalu;
bool                                          mbLookForIframe;
bool                                          mbFirstFrameDrop;
uint8*                                        mpAVCHrdDescr;
uint8                                         mnAVCHrdDescrLen;
MUX_AVC_TimingHRD_params_type                 msHRDParams;
uint8                                         mnGenericStreamNum;
uint64                                        mnPrevTimeStamp;
bool                                          mbBaseTimeStampTaken;
uint64                                        mnBaseTimestamp;
uint64                                        mllVideoDelayCorrection;
uint64                                        mnCurrPCR;
uint8                                         mpMP2TSContinutyCounter[MUX_MAX_MEDIA_STREAMS];
uint8                                         mMP2TSTableContinutyCounter[2];
boolean                                       mDataAlignmentIndicator;
uint8*                                        mpHeader[MUX_MAX_MEDIA_STREAMS];
uint32                                        mnHeaderSize[MUX_MAX_MEDIA_STREAMS];
bool                                          mbHeaderSent[MUX_MAX_MEDIA_STREAMS];
MM_HANDLE                                     mhVideoFrameTimer;
MP2_MUX_file                                  msFilePtr;
MUX_handle_type                               msOutputHandle;
boolean                                       mbOutputOpen;
MM_HANDLE                                     mhCritSect;
MM_HANDLE                                     mpStatTimer;
uint32                                        mnDuration;
bool                                          mbSaveToMmc;
uint32                                        mnOutputOffset;
uint32                                        mnOutputSize;
uint32                                        mnMmcFreeSpace;
uint32                                        mnFlashFreeSpace;
uint64                                        mllTimeBase;

private:

#ifdef ENABLE_MUX_STATS
typedef struct mux_stats_struct
{
    uint32  nStatCount;
    unsigned long nStartTime;
    unsigned long nEndTime;
    unsigned long  nMaxTime;
    bool    bEnableMuxStat;
}mux_stats;
mux_stats msMuxStats;

static void readStatTimerHandler(void * ptr);
#endif

};
#endif  //__MUX_MP2BASEFILE_H__

