#ifndef __RTMP_PUBLISH__
#define __RTMP_PUBLISH__

#ifdef __cplusplus    
extern "C" {         
#endif
#include "rtmp.h"
#include <pthread.h>

        enum HEVCNALUnitType {
                HEVC_NAL_TRAIL_N    = 0,
                HEVC_NAL_TRAIL_R    = 1,
                HEVC_NAL_TSA_N      = 2,
                HEVC_NAL_TSA_R      = 3,
                HEVC_NAL_STSA_N     = 4,
                HEVC_NAL_STSA_R     = 5,
                HEVC_NAL_RADL_N     = 6,
                HEVC_NAL_RADL_R     = 7,
                HEVC_NAL_RASL_N     = 8,
                HEVC_NAL_RASL_R     = 9,
                HEVC_NAL_VCL_N10    = 10,
                HEVC_NAL_VCL_R11    = 11,
                HEVC_NAL_VCL_N12    = 12,
                HEVC_NAL_VCL_R13    = 13,
                HEVC_NAL_VCL_N14    = 14,
                HEVC_NAL_VCL_R15    = 15,
                HEVC_NAL_BLA_W_LP   = 16,
                HEVC_NAL_BLA_W_RADL = 17,
                HEVC_NAL_BLA_N_LP   = 18,
                HEVC_NAL_IDR_W_RADL = 19,
                HEVC_NAL_IDR_N_LP   = 20,
                HEVC_NAL_CRA_NUT    = 21,
                HEVC_NAL_IRAP_VCL22 = 22,
                HEVC_NAL_IRAP_VCL23 = 23,
                HEVC_NAL_RSV_VCL24  = 24,
                HEVC_NAL_RSV_VCL25  = 25,
                HEVC_NAL_RSV_VCL26  = 26,
                HEVC_NAL_RSV_VCL27  = 27,
                HEVC_NAL_RSV_VCL28  = 28,
                HEVC_NAL_RSV_VCL29  = 29,
                HEVC_NAL_RSV_VCL30  = 30,
                HEVC_NAL_RSV_VCL31  = 31,
                HEVC_NAL_VPS        = 32,
                HEVC_NAL_SPS        = 33,
                HEVC_NAL_PPS        = 34,
                HEVC_NAL_AUD        = 35,
                HEVC_NAL_EOS_NUT    = 36,
                HEVC_NAL_EOB_NUT    = 37,
                HEVC_NAL_FD_NUT     = 38,
                HEVC_NAL_SEI_PREFIX = 39,
                HEVC_NAL_SEI_SUFFIX = 40,
        };

typedef enum {
        RTMP_PUB_AUDIO_NONE = 0,
        RTMP_PUB_AUDIO_AAC = 1,
        RTMP_PUB_AUDIO_G711A,
        RTMP_PUB_AUDIO_G711U,
        RTMP_PUB_AUDIO_PCM,
} RtmpPubAudioType;

typedef enum {
        RTMP_PUB_TIMESTAMP_ABSOLUTE = 1,
        RTMP_PUB_TIMESTAMP_RELATIVE
} RtmpPubTimeStampPolicy;
        
typedef enum {
        RTMP_PUB_VIDEOTYPE_AVC = 1,
        RTMP_PUB_VIDEOTYPE_HEVC
} RtmpPubVideoType;

typedef struct {
        int m_nType;
        char * m_pData;
        unsigned int m_nSize;
} RtmpPubNalUnit;

typedef struct {
        struct RTMP * m_pRtmp;        
        char * m_pPubUrl;
        unsigned int m_nTimeout;
        RtmpPubNalUnit m_pPps;
        RtmpPubNalUnit m_pSps;
        RtmpPubNalUnit m_pVps;
        RtmpPubNalUnit m_pSei;
        unsigned int m_nAudioTimebase;
        unsigned int m_nVideoTimebase;

        long long int m_nLastVideoTimeStamp;
        long long int m_nLastAudioTimeStamp;
        long long int m_nLastMediaTimeStamp;

        int m_bIsMediaPktSmall;         
        long long int m_nMediaTimebase;
        unsigned int m_nAdtsSize;
        void * m_pAudioEncoderContext;
        int m_nIsAudioConfigSent;
        int m_nIsVideoConfigSent;
        RtmpPubAudioType m_nAudioInputType;
        RtmpPubAudioType m_nAudioOutputType;
        RtmpPubTimeStampPolicy m_nTimePolicy;
        int m_nAudioSampleRate;
        int m_nAudioChannel;
        RtmpPubNalUnit m_aac;
        RtmpPubVideoType m_videoType;
        pthread_mutex_t m_mutex;
} RtmpPubContext;

RtmpPubContext * RtmpPubNew(const char * _url, unsigned int _nTimeout, RtmpPubAudioType _nInputAudioType, RtmpPubAudioType m_nOutputAudioType, 
                                                                RtmpPubTimeStampPolicy _nTimePolicy);
int RtmpPubInit(RtmpPubContext * _pRtmp);
void RtmpPubSetVideoType(RtmpPubContext * _pRtmp, RtmpPubVideoType _type);
void RtmpPubSetAudioConfig(RtmpPubContext * _pRtmp, int nSampleRate, int nChannels);
void RtmpPubDel(RtmpPubContext * _pRtmp);

int RtmpPubConnect(RtmpPubContext * _pRtmp);

void RtmpPubSetAudioTimebase(RtmpPubContext * _pRtmp, unsigned int _nTimeStamp);
void RtmpPubSetVideoTimebase(RtmpPubContext * _pRtmp, unsigned int _nTimeStamp);


int RtmpPubSendVideoKeyframe(RtmpPubContext * _pRtmp, const char * _pData, unsigned int _nSize, unsigned int _presentationTime);

int RtmpPubSendVideoInterframe(RtmpPubContext * _pRtmp, const char * _pData, unsigned int _nSize, unsigned int _presentationTime);

void RtmpPubSetPps(RtmpPubContext * _pRtmp, const char * _pData, unsigned int _nSize);
void RtmpPubSetSps(RtmpPubContext * _pRtmp, const char * _pData, unsigned int _nSize);
void RtmpPubSetSei(RtmpPubContext * _pRtmp, const char * _pData, unsigned int _nSize);
void RtmpPubSetVps(RtmpPubContext * _pRtmp, const char * _pData, unsigned int _nSize);


int RtmpPubSendAudioFrame(RtmpPubContext * _pRtmp, const char * _pData, unsigned int _nSize, int _nTimeStamp);


//If the input and output format are botch aac, call this function before the first audio packet
void RtmpPubSetAac(RtmpPubContext * _pRtmp, const char * _pAacCfgRecord, unsigned int _nSize);





#ifdef __cplusplus
}
#endif
#endif
