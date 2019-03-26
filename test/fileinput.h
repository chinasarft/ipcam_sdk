#include <stdint.h>
#include <pthread.h>

typedef int (*VideoCallback)(char *frame, int len, int iskey, int64_t timestatmp, void *pcontext);
typedef int (*AudioCallback)(char *frame, int len, int64_t timestatmp, void *pcontext);

typedef struct {
        unsigned char isAac;
        unsigned char isH265;
        unsigned char isLoop;
        volatile unsigned char _isStop;
        const char *pAFile;
        const char *pVFile;
        const char *pAFmt;
        const char *pVFmt;
        VideoCallback videoCb;
        AudioCallback audioCb;
        int nChannels;
        int nSampleRate;
        void *pUserContext;
        int nRolloverTestBase;
        uint8_t IsTestAACWithoutAdts;
        pthread_t _audioThread;
        pthread_t _videoThread;
}Stream;

int start_stream(Stream *pStream);
int stop_stream(Stream *pStream);
