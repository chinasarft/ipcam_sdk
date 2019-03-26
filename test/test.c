#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <string.h>
#include <inttypes.h>
#include "flag.h"
#include "fileinput.h"
#include "rtmp_publish.h"

#define VERSION "0.0.1"

//#define DEFAULT_H265 1
typedef struct {
        const char* pRtmpUrl;
        Stream stream;
        int isLoop;
        int isTestWithAdts;
        RtmpPubContext *pRtmpCtx;
        int nRtmpTimeout;
        RtmpPubAudioType inType;
        RtmpPubAudioType outType;
        const char * outFmt;
}CmdArg;
CmdArg cmdArg;

int checkCmdArgAndInitStream() {
        if (cmdArg.pRtmpUrl == NULL) {
                fprintf(stderr, "should input rtmp url\n");
                return 0;
        }
        if (cmdArg.stream.nChannels > 2) {
                fprintf(stderr, "cmdArg.nChannels:%d\n", cmdArg.stream.nChannels);
                return 0;
        }
        if(memcmp(cmdArg.stream.pAFmt, "aac", 3) == 0) {
                cmdArg.stream.isAac = 1;
        } else {
                if (cmdArg.isTestWithAdts) {
                        fprintf(stderr, "adts just should set with aac\n");
                        return 0;
                }
        }
        
        if ( cmdArg.stream.audioCb == NULL || cmdArg.stream.videoCb == NULL) {
                fprintf(stderr, "a/v callback is NULL\n");
                return 0;
        }
        
        cmdArg.stream.IsTestAACWithoutAdts = cmdArg.isTestWithAdts;
        if(memcmp(cmdArg.stream.pAFmt, "h265", 4) == 0)
                cmdArg.stream.isH265 = 1;
        cmdArg.stream.isLoop = cmdArg.isLoop;
        cmdArg.stream.pUserContext = &cmdArg;
        
        if(memcmp(cmdArg.stream.pAFmt, "aac", 3) == 0) {
                cmdArg.inType = RTMP_PUB_AUDIO_AAC;
        } else if(memcmp(cmdArg.stream.pAFmt, "g711u", 5) == 0) {
                cmdArg.inType = RTMP_PUB_AUDIO_G711U;
        } else if(memcmp(cmdArg.stream.pAFmt, "g711a", 5) == 0) {
                cmdArg.inType = RTMP_PUB_AUDIO_G711A;
        }
        cmdArg.outType = RTMP_PUB_AUDIO_AAC;
        if(memcmp(cmdArg.stream.pAFmt, "g711u", 5) == 0) {
                cmdArg.outType = RTMP_PUB_AUDIO_G711U;
        } else if(memcmp(cmdArg.stream.pAFmt, "g711a", 5) == 0) {
                cmdArg.outType = RTMP_PUB_AUDIO_G711A;
        }
        
        return 1;
}

int videoCallback(char *frame, int len, int iskey, int64_t timestatmp, void *pcontext) {
        CmdArg *pArg = (CmdArg *)pcontext;
        int ret = 0;
        //fprintf(stderr, "video frame:%d %d %"PRId64"\n", len, iskey, timestatmp);
        if(iskey) {
                ret = RtmpPubSendVideoKeyframe(pArg->pRtmpCtx, frame, len, (uint32_t)timestatmp);
        } else {
                ret = RtmpPubSendVideoInterframe(pArg->pRtmpCtx, frame, len, (uint32_t)timestatmp);
        }
        if (ret != 0) {
                pArg->stream._isStop = 1;
                fprintf(stderr, "send video frame fail:%d iskey:%d\n", ret, iskey);
        }
        return ret;
}

int audioCallback(char *frame, int len, int64_t timestatmp, void *pcontext) {
        CmdArg *pArg = (CmdArg *)pcontext;
        int ret = 0;
        //fprintf(stderr, "audio frame:%d %"PRId64"\n", len, timestatmp);
        ret = RtmpPubSendAudioFrame(pArg->pRtmpCtx, frame, len, (uint32_t)timestatmp);
        if (ret != 0) {
                pArg->stream._isStop = 1;
                fprintf(stderr, "send audio frame fail:%d\n", ret);
        }
        return ret;
}

int main(int argc, const char **argv)
{
        cmdArg.stream.nChannels = 1;
        cmdArg.stream.nSampleRate = 16000;
        cmdArg.stream.pVFile = "./len.h264";
        cmdArg.stream.pAFile = "./1_16000_a.aac";
        cmdArg.stream.pAFmt = "aac";
        cmdArg.stream.pVFmt = "h264";
        cmdArg.nRtmpTimeout = 10;
        cmdArg.outFmt = "aac";
        cmdArg.stream.audioCb = audioCallback;
        cmdArg.stream.videoCb = videoCallback;
        
        flag_str(&cmdArg.pRtmpUrl, "url", "rtmp push url");
        flag_str(&cmdArg.stream.pVFmt, "vfmt", "video format(h264/h265) default h264");
        flag_str(&cmdArg.stream.pAFmt, "afmt", "audio format(g711u/g711a/aac) default is aac");
        flag_str(&cmdArg.stream.pVFile, "vpath", "video file path. default ./len.h264");
        flag_str(&cmdArg.stream.pAFile, "apath", "audio file path. default ./1_16000_a.aac");
        flag_str(&cmdArg.outFmt, "aoutfmt", "audio out format(g711u/g711a/aac)");
        flag_int(&cmdArg.stream.nChannels, "ac", "audio channel. default 1)");
        flag_int(&cmdArg.stream.nSampleRate, "ar", "audio samplerate. default 16000");
        flag_int(&cmdArg.isLoop, "loop", "loop file");
        flag_int(&cmdArg.stream.nRolloverTestBase, "tbase", "time base");
        flag_int(&cmdArg.isTestWithAdts, "withadts", "aac frame with adts");
        flag_int(&cmdArg.nRtmpTimeout, "timeout", "rtmp timeout. default 10");
        
        if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "-help") == 0)) {
                flag_write_usage(argv[0]);
                return 0;
        }
        
        flag_parse(argc, argv, VERSION);
        
        if (!checkCmdArgAndInitStream()) {
                return -1;
        }
        
        cmdArg.pRtmpCtx = RtmpPubNew(cmdArg.pRtmpUrl, cmdArg.nRtmpTimeout , cmdArg.inType, cmdArg.outType,
                                     RTMP_PUB_TIMESTAMP_ABSOLUTE);
                                     //RTMP_PUB_TIMESTAMP_RELATIVE);
        if (cmdArg.pRtmpCtx == NULL) {
                fprintf(stderr, "RtmpPubNew fail\n");
                return -2;
        }
        int ret = RtmpPubInit(cmdArg.pRtmpCtx);
        if (ret < 0) {
                fprintf(stderr, "RtmpPubInit fail:%d\n", ret);
                return -3;
        }
        if (memcmp(cmdArg.stream.pVFmt, "h265", 4) == 0) {
                fprintf(stderr, "RTMP_PUB_VIDEOTYPE_HEVC\n");
                RtmpPubSetVideoType(cmdArg.pRtmpCtx, RTMP_PUB_VIDEOTYPE_HEVC);
        }
        RtmpPubSetAudioConfig(cmdArg.pRtmpCtx, cmdArg.stream.nSampleRate, cmdArg.stream.nChannels);
        
        ret = RtmpPubConnect(cmdArg.pRtmpCtx);
        if ( ret != 0) {
                RtmpPubDel(cmdArg.pRtmpCtx);
                cmdArg.pRtmpCtx = NULL;
                fprintf(stderr, "RtmpPubConnect fail:%s %d\n", cmdArg.pRtmpUrl, ret);
                return -4;
        }
        
        start_stream(&cmdArg.stream);
        
        while(cmdArg.stream._isStop == 0) {
                sleep(1);
        }
        fprintf(stderr, "stop_stream\n");
        stop_stream(&cmdArg.stream);
        return 0;
}

