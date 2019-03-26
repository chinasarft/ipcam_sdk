#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <errno.h>
#include "fileinput.h"
#include "adts.h"

#define THIS_IS_AUDIO 1
#define THIS_IS_VIDEO 2

static int aacfreq[13] = {96000, 88200,64000,48000,44100,32000,24000, 22050 , 16000 ,12000,11025,8000,7350};
typedef struct ADTS{
        ADTSFixheader fix;
        ADTSVariableHeader var;
}ADTS;

static inline int64_t getCurrentMilliSecond(){
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec*1000 + tv.tv_usec/1000);
}

static int getFileAndLength(const char *_pFname, FILE **_pFile, int *_pLen)
{
        FILE * f = fopen(_pFname, "r");
        if ( f == NULL ) {
                return -1;
        }
        *_pFile = f;
        fseek(f, 0, SEEK_END);
        long nLen = ftell(f);
        fseek(f, 0, SEEK_SET);
        *_pLen = (int)nLen;
        return 0;
}

static int readFileToBuf(const char * _pFilename, char ** _pBuf, int *_pLen)
{
        int ret;
        FILE * pFile;
        int nLen = 0;
        ret = getFileAndLength(_pFilename, &pFile, &nLen);
        if (ret != 0) {
                fprintf(stderr, "open file %s fail\n", _pFilename);
                return -1;
        }
        char *pData = malloc(nLen);
        assert(pData != NULL);
        ret = fread(pData, 1, nLen, pFile);
        if (ret <= 0) {
                fprintf(stderr, "open file %s fail\n", _pFilename);
                fclose(pFile);
                free(pData);
                return -2;
        }
        fclose(pFile);
        *_pBuf = pData;
        *_pLen = nLen;
        return 0;
}

static int dataCallback(void *opaque, void *pData, int nDataLen, int nFlag, int64_t timestamp, int nIsKeyFrame)
{
        Stream *pStream = (Stream*)opaque;
        int ret = 0;
        if (nFlag == THIS_IS_AUDIO){
                //fprintf(stderr, "push audio ts:%lld\n", timestamp);
                //(char *frame, int len, double timestatmp, unsigned long frame_index, void *pcontext);
                ret = pStream->audioCb(pData, nDataLen, timestamp, pStream->pUserContext);
        } else {
                //printf("------->push video key:%d ts:%lld size:%d\n",nIsKeyFrame, timestamp, nDataLen);
                //(int streamno, char *frame, int len, int iskey, double timestatmp, unsigned long frame_index, unsigned long keyframe_index, void *pcontext);
                ret = pStream->videoCb(pData, nDataLen, nIsKeyFrame, timestamp, pStream->pUserContext);
        }
        return ret;
}

char h264Aud3[3]={0, 0, 1};
char h264Aud4[4]={0, 0, 0, 1};
void * start_video_file_test(void *opaque)
{
        Stream *pStream = (Stream*)opaque;
        fprintf(stderr, "----------_>start_video_file_test:%s %s\n", pStream->pVFile, pStream->pVFmt);
        int ret;
        
        char * pVideoData = NULL;
        int nVideoDataLen = 0;
        ret = readFileToBuf(pStream->pVFile, &pVideoData, &nVideoDataLen);
        if (ret != 0) {
                free(pVideoData);
                fprintf(stderr, "map data to buffer fail:%s", pStream->pVFile);
                return NULL;
        }
        
        int64_t nSysTimeBase = getCurrentMilliSecond();
        int64_t nNextVideoTime = nSysTimeBase;
        int64_t nNow = nSysTimeBase;
        
        int bVideoOk = 1;
        int videoOffset = 0;
        int cbRet = 0;
        int nIDR = 0;
        int nNonIDR = 0;
        
        while (!pStream->_isStop && bVideoOk) {
                int nLen;
                int type = -1;
                
                if (videoOffset+4 < nVideoDataLen) {
                        memcpy(&nLen, pVideoData+videoOffset, 4);
                        if (videoOffset + 4 + nLen < nVideoDataLen) {
                                if (!pStream->isH265) {
                                        if (memcmp(h264Aud3, pVideoData + videoOffset + 4, 3) == 0) {
                                                type = pVideoData[videoOffset + 7] & 0x1F;
                                        } else {
                                                type = pVideoData[videoOffset + 8] & 0x1F;
                                        }
                                        if (type == 1) {
                                                nNonIDR++;
                                        } else {
                                                nIDR++;
                                        }
                                        
                                        cbRet = dataCallback(opaque, pVideoData + videoOffset + 4, nLen, THIS_IS_VIDEO, pStream->nRolloverTestBase+nNextVideoTime-nSysTimeBase, !(type == 1));
                                        if (cbRet != 0) {
                                                bVideoOk = 0;
                                        }
                                        videoOffset = videoOffset + 4 + nLen;
                                }else {
                                        if (memcmp(h264Aud3, pVideoData + videoOffset + 4, 3) == 0) {
                                                type = pVideoData[videoOffset + 7] & 0x7F;
                                        } else {
                                                type = pVideoData[videoOffset + 8] & 0x7F;
                                        }
                                        type = (type >> 1);
                                        
                                        if (type == 32){
                                                nIDR++;
                                        } else {
                                                nNonIDR++;
                                        }
                                        
                                        cbRet = dataCallback(opaque, pVideoData + videoOffset + 4, nLen, THIS_IS_VIDEO,pStream->nRolloverTestBase+nNextVideoTime-nSysTimeBase, type == 32);//hevctype == HEVC_I);
                                        if (cbRet != 0) {
                                                bVideoOk = 0;
                                        }
                                        videoOffset = videoOffset + 4 + nLen;
                                }
                        }  else {
                                videoOffset = 0;
                                continue;
                        }
                } else {
                        videoOffset = 0;
                        continue;
                }
                nNextVideoTime += 40;
                
                
                int64_t nSleepTime = 0;
                nSleepTime = (nNextVideoTime - nNow - 1) * 1000;
                if (nSleepTime > 0) {
                        //printf("sleeptime:%lld\n", nSleepTime);
                        if (nSleepTime > 40 * 1000) {
                                fprintf(stderr, "abnormal time diff:%lld", nSleepTime);
                        }
                        usleep(nSleepTime);
                }
                nNow = getCurrentMilliSecond();
        }
        
        if (pVideoData) {
                free(pVideoData);
                fprintf(stderr, "IDR:%d nonIDR:%d\n", nIDR, nNonIDR);
        }
        return NULL;
}


void * start_audio_file_test(void *opaque)
{
        Stream *pStream = (Stream*)opaque;
        fprintf(stderr, "----------_>start_audio_file_test:%s %s\n", pStream->pAFile, pStream->pAFmt);
        int ret;
        
        char * pAudioData = NULL;
        int nAudioDataLen = 0;
        ret = readFileToBuf(pStream->pAFile, &pAudioData, &nAudioDataLen);
        if (ret != 0) {
                fprintf(stderr, "map data to buffer fail:%s", pStream->pAFile);
                return NULL;
        }
        
        int bAudioOk = 1;
        int64_t nSysTimeBase = getCurrentMilliSecond();
        int64_t nNextAudioTime = nSysTimeBase;
        int64_t nNow = nSysTimeBase;
        
        int audioOffset = 0;
        int isAAC = 1;
        int64_t aacFrameCount = 0;
        if (!pStream->isAac)
                isAAC = 0;
        int cbRet = 0;
        
        int duration = 0;
        
        while (!pStream->_isStop && bAudioOk) {
                if (isAAC) {
                        ADTS adts;
                        if(audioOffset+7 <= nAudioDataLen) {
                                ParseAdtsfixedHeader((unsigned char *)(pAudioData + audioOffset), &adts.fix);
                                int hlen = adts.fix.protection_absent == 1 ? 7 : 9;
                                ParseAdtsVariableHeader((unsigned char *)(pAudioData + audioOffset), &adts.var);
                                if (audioOffset+hlen+adts.var.aac_frame_length <= nAudioDataLen) {
                                        
                                        if (pStream->IsTestAACWithoutAdts)
                                                cbRet = dataCallback(opaque, pAudioData + audioOffset + hlen, adts.var.aac_frame_length - hlen,
                                                                     THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase+pStream->nRolloverTestBase, 0);
                                        else
                                                cbRet = dataCallback(opaque, pAudioData + audioOffset, adts.var.aac_frame_length,
                                                                     THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase+pStream->nRolloverTestBase, 0);
                                        if (cbRet != 0) {
                                                bAudioOk = 0;
                                                continue;
                                        }
                                        audioOffset += adts.var.aac_frame_length;
                                        aacFrameCount++;
                                        int64_t d = ((1024*1000.0)/aacfreq[adts.fix.sampling_frequency_index]) * aacFrameCount;
                                        nNextAudioTime = nSysTimeBase + d;
                                } else {
                                        aacFrameCount++;
                                        int64_t d = ((1024*1000.0)/aacfreq[adts.fix.sampling_frequency_index]) * aacFrameCount;
                                        nNextAudioTime = nSysTimeBase + d;
                                        audioOffset = 0;
                                }
                                if (duration == 0) {
                                        duration = ((1024*1000.0)/aacfreq[adts.fix.sampling_frequency_index]);
                                }
                        } else {
                                aacFrameCount++;
                                int64_t d = ((1024*1000.0)/aacfreq[adts.fix.sampling_frequency_index]) * aacFrameCount;
                                nNextAudioTime = nSysTimeBase + d;
                                audioOffset = 0;
                                continue;
                        }
                } else {
                        duration = 20;
                        if(audioOffset+160 <= nAudioDataLen) {
                                cbRet = dataCallback(opaque, pAudioData + audioOffset, 160, THIS_IS_AUDIO, nNextAudioTime-nSysTimeBase+pStream->nRolloverTestBase, 0);
                                if (cbRet != 0) {
                                        bAudioOk = 0;
                                        continue;
                                }
                                audioOffset += 160;
                                nNextAudioTime += 20;
                        } else {
                                audioOffset = 0;
                                continue;
                        }
                }
                
                int64_t nSleepTime = (nNextAudioTime - nNow - 1) * 1000;
                if (nSleepTime > 0) {
                        //printf("sleeptime:%lld\n", nSleepTime);
                        if (nSleepTime > duration * 1000) {
                                printf("abnormal time diff:%lld", nSleepTime);
                        }
                        usleep(nSleepTime);
                }
                nNow = getCurrentMilliSecond();
        }
        
        if (pAudioData) {
                free(pAudioData);
                fprintf(stderr, "quit audio test\n");
        }
        return NULL;
}

int start_stream(Stream *pStream) {
        
        int ret = pthread_create(&pStream->_audioThread, NULL, start_video_file_test, pStream);
        if (ret < 0) {
                fprintf(stderr, "pthread_create fail:%s", strerror(errno));
                return ret;
        }
        
        ret = pthread_create(&pStream->_audioThread, NULL, start_audio_file_test, pStream);
        if (ret < 0) {
                pStream->_isStop = 1;
                pthread_join(pStream->_videoThread, NULL);
                fprintf(stderr, "pthread_create fail:%s", strerror(errno));
                return ret;
        }

        return 0;
}

int stop_stream(Stream *pStream) {
        pStream->_isStop = 1;
        pthread_join(pStream->_audioThread, NULL);
        pthread_join(pStream->_videoThread, NULL);
        return 0;
}
