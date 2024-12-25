/*
 * main.c
 *
 *  Created on: 2023. 11. 19.
 *      Author: SEONGPIL
 */

#include <stdio.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>
#include <malloc.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
#include <locale.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <termios.h>
#include <alsa/asoundlib.h>
#include <assert.h>
#include <math.h>
#include <sys/stat.h>

#include <aspl_nr.h>

#define EXPERT_TUNE
#ifdef EXPERT_TUNE
#include "../lib/menu.h"
#endif

#ifdef EXPERT_TUNE 
int debug_matlab_onoff();
#endif

#include "keyboard.h"

#define CV2_mono
#define FILE_AEC_test
#define FILE_WRITE
#define FILE_PLAY

#define TOTAL_CAP_CHANNELS  1
#define IN_CHANNELS         2
#define OUTPUT_CHANNELS 	2

#define REF_CHANNELS_dummy  1
#define WORK_CHANNELS       (IN_CHANNELS+REF_CHANNELS_dummy)
#define OUTPUT_CHANNELS 	2

#define NUM_FRAMES          512
#define SAMPLING_FREQ       16000
#define NUM_BYTE_PER_SAMPLE 2

int g_inputGain = 5;
int g_inputSource = 0; // 0: line, 1: internal 2: external
int bias_on = 0;
int g_rec_on = 0;

void CV2_change_audioInDeviceSetting(int source, int mic_bias, int gain);
static void signal_handler(int signal);
void Show_menu();
static void *proc_loop(void *arg);
static void *key_loop(void *arg);
static void signal_handler(int signal);
int init_alsa();

int Alsa_pcm_open(snd_pcm_t **CaptureHandle, char *CaptureDevice, snd_pcm_stream_t mode, uint32_t num_channel, uint32_t sample_rate, snd_pcm_uframes_t period_size) ;

int terminate;
int run_pause=1;

int g_play_mic = 0;
int g_Beam_no = 4; // center
static int phase = 0;

int file;
static int file_play_on = 0;
const char* g_filename;

#ifdef FILE_WRITE
    static int file_write_state = 0;
    int fdmic; 
    const char * filename;       
#endif

#ifndef bypass_ASPL_NR
aspl_NR_CONFIG aspl_nr_config;
#endif

snd_pcm_t *CaptureHandle;
snd_pcm_hw_params_t *CaptureParams;
snd_pcm_uframes_t CaptureFrames;

snd_pcm_t *CaptureHandleRef;
snd_pcm_hw_params_t *CaptureParamsRef;
snd_pcm_uframes_t CaptureFramesRef;

snd_pcm_t *PlayHandle;
snd_pcm_hw_params_t *PlayParams;
snd_pcm_uframes_t PlayFrames;

char * CaptureBufRef;
char * CaptureBuf;
char * PlayBuf;
char * WorkBuf;
char * FileBuf;

int CaptureBufSize;
int CaptureRefBufSize;
int PlayBufSize;
int WorkBufSize;
int FileBufSize;

aspl_NR_CONFIG aspl_nr_config;
int fourthArg=0;

int main(int argc, char *argv[])
{

    if (argc >= 3){
        g_filename = argv[2];
        // file_play_on = 1;
    }

    if (argc == 4){
        char *endptr;
        fourthArg = strtol(argv[3], &endptr, 10);

        if (*endptr != '\0') {
            printf("4번째 인자가 숫자가 아닙니다: %s\n", argv[3]);
            return 1;
        }
    } else {
        fourthArg = 0;
    }



    // file = open(g_filename, O_RDONLY);
                    
    // if (file == -1) {
    //     printf("파일을 열 수 없습니다.");
    //     return 0;
    // }

    // // 파일 크기 확인
    // struct stat st;
    // if (stat(g_filename, &st) != 0) {
    //     printf("파일 크기를 확인할 수 없습니다.");
    //     return 0;
    // }
    // off_t file_size = st.st_size;  

    // file_play_on = 1;    

    int status;
    pthread_t proc_thread, keyboard_thread;

    terminate = 0;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGABRT, signal_handler);

    init_keyboard();

    if (argc < 2) {
        printf("사용법: 프로그램명 <param bin file> <play raw file>\n");
        // return 1;
    }

    aspl_nr_config.enable = 1;
    aspl_nr_config.sensitivity = 2;
    char* file_path = argv[1]; //"./aspl_nr_params_default.bin";
    strncpy(aspl_nr_config.tuning_file_path, file_path, sizeof(aspl_nr_config.tuning_file_path));
    aspl_nr_config.tuning_file_path[sizeof(aspl_nr_config.tuning_file_path) - 1] = '\0';  // Ensure null termination    

    // int ret = aspl_NR_create(NULL);
    int ret = aspl_NR_create_2mic((void *)&aspl_nr_config);
    
    aspl_nr_config.aec_Mic_N = 2;
    ret = aspl_AEC_create(&aspl_nr_config);

    if (ret!=aspl_RET_SUCCESS){
        printf("create error\n");

        return 1;
    } else if (ret==aspl_RET_SUCCESS){
        printf("create ok\n");
    }

    const char* version = aspl_getVersionInfo();
    printf("ASPL NR Library Version Info: %s\n", version);

    pthread_create(&proc_thread, NULL, proc_loop, NULL);
    pthread_create(&keyboard_thread, NULL, key_loop, NULL);
    pthread_join(keyboard_thread, NULL);
    pthread_join(proc_thread, NULL);

    printf("\nAlsa In-out Test End\n");

    aspl_NR_destroy();

}


static void *proc_loop(void *arg) {

    int rc;
    int i, j, k, m;
    unsigned char *tempTxPtr, *tempRxPtr, *tempWkPtr;
    unsigned char *tempOutPtr, *tempOutPtr2, *tempMicPtr, *tempMicPtr2, *tempMicPtr3, *tempMicPtr4;

    short   *in_r;

    short   *mics_in[IN_CHANNELS] = {NULL};     /* pointers to microphone inputs */
#ifdef REF_CHANNELS    
	short   *refs_in[REF_CHANNELS] = {NULL};     /* pointers to microphone inputs */
#endif 
#ifdef REF_CHANNELS_dummy    
	short   *refs_in[REF_CHANNELS_dummy] = {NULL};     /* pointers to microphone inputs */
#endif      

    int FrameCnt=0;
    int FrameCntRef=0;

    if (init_alsa()<0) goto Err;


    snd_pcm_recover(CaptureHandle, rc, 0);
    snd_pcm_prepare(CaptureHandle);

#ifndef CV2_mono    
    snd_pcm_recover(CaptureHandleRef, rc, 0);
    snd_pcm_prepare(CaptureHandleRef);
#endif

    printf("\n Loop start. Press 'z' to end loop.\n"); 

    while (!terminate)
    {
////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////   Audio Capture          ////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////

        rc = snd_pcm_readi(CaptureHandle, CaptureBuf, CaptureFrames);

        if (rc == -EPIPE)
        {
            /* EPIPE means overrun */
            fprintf(stderr, "mic chan overrun occurred\n");
            snd_pcm_prepare(CaptureHandle);
            continue;
        }
        else if (rc < 0)
        {
            fprintf(stderr,"error from read: %s\n",snd_strerror(rc));
            continue;
        }
        else if (rc > 0)
        {
            // printf("required mic period : %d, actual : %d\n", CaptureFrames, rc);
            FrameCnt=rc;
            CaptureBufSize=FrameCnt * (TOTAL_CAP_CHANNELS) * NUM_BYTE_PER_SAMPLE;

#ifdef FILE_WRITE
            if (file_write_state == 2){
                rc=write(fdmic, CaptureBuf, CaptureBufSize);
                if (rc != CaptureBufSize)
                {
                    fprintf(stderr,"short write: wrote %d bytes\n", rc);
                }
            }
#endif

#ifdef FILE_PLAY
            if (file_play_on==1){
                    // printf("required mic period : %d, actual : %d CaptureBufSize : %d ", CaptureFrames, rc, CaptureBufSize);
                    // int CaptureBufSize2 = FrameCnt * (TOTAL_CAP_CHANNELS+1) * NUM_BYTE_PER_SAMPLE;
#ifdef REF_CHANNELS_dummy
                    CaptureBufSize=FrameCnt * (IN_CHANNELS+REF_CHANNELS_dummy) * NUM_BYTE_PER_SAMPLE;
                    ssize_t bytes_read = read(file, CaptureBuf, CaptureBufSize);  
#else  
                    ssize_t bytes_read = read(file, CaptureBuf, CaptureBufSize);
#endif                                       
                    // printf("file bytes_read : %d \r\n", bytes_read);
                    if (bytes_read == -1) {
                        printf("파일을 읽을 수 없습니다.");
                        goto Err;
                    } else if (bytes_read == 0) {
                        // 파일의 끝에 도달하면 파일 포인터를 처음으로 이동
                        printf("Raw 음원 파일 끝 도달 처음부터 시작.");
                        off_t current_pos = lseek(file, 0, SEEK_SET);
                        if (current_pos == -1) {
                            printf("파일 위치를 이동할 수 없습니다.");
                            goto Err;
                        }
                    }
            }
#endif
        }

#ifdef REF_CHANNELS
        rc = snd_pcm_readi(CaptureHandleRef, CaptureBufRef, CaptureFrames);
        
        if (rc == -EPIPE)
        {
            /* EPIPE means overrun */
            fprintf(stderr, "ref chan overrun occurred\n");
            snd_pcm_prepare(CaptureHandleRef);
            // continue;
        }
        else if (rc < 0)
        {
            fprintf(stderr,"ref chan error from read: %s\n",snd_strerror(rc));
            // continue;
        }
        else if (rc > 0)
        {
            FrameCntRef=rc;
            CaptureRefBufSize=FrameCntRef * (REF_CHANNELS) * NUM_BYTE_PER_SAMPLE;         
        }        
#endif
        ///////////////////////////   input buf sorting      ////////////////////////////////////////

        

        in_r  = (short *)WorkBuf;
        for (k = 0; k < IN_CHANNELS; k++) {
            mics_in[k] = &in_r[k*CaptureFrames];	/* find the frame start for each microphone */
        }
#ifdef REF_CHANNELS          
        for (k = 0; k < REF_CHANNELS; k++) {
            refs_in[k] = &in_r[(IN_CHANNELS+k)*CaptureFrames];   /* find the frame start for each ref channel */
        }
#endif
#ifdef REF_CHANNELS_dummy         
        for (k = 0; k < REF_CHANNELS_dummy; k++) {
            refs_in[k] = &in_r[(IN_CHANNELS+k)*CaptureFrames];   /* find the frame start for each ref channel */
        }
#endif       

        // int chanToMicMapping[6] = {3, 2, 0, 4, 1, 5}; // for SD (first version) demo
        // int chanToMicMapping[6] = {0, 3, 2, 1, 4, 5}; // for SD (second version) demo
        // int chanToMicMapping[6] = {0, 3, 2, 1, 4, 5}; // for MT8137 demo
        if (file_play_on==1){
            // for each channel, convert and copy the RX buffer to WK buffer
            for (j=0; j<(IN_CHANNELS+REF_CHANNELS_dummy); j++) {
                // set the RX start pointer
                tempRxPtr = (unsigned char *)CaptureBuf + j*sizeof(short);
                // set the WK start pointer
                tempWkPtr = (unsigned char *)WorkBuf + j*(CaptureFrames)*sizeof(short);
               
                for (i=0; i<CaptureFrames; i++){
                    memcpy(tempWkPtr, tempRxPtr, sizeof(short));
                    tempWkPtr += sizeof(short);
                    tempRxPtr += sizeof(short)*((IN_CHANNELS+REF_CHANNELS_dummy));
                }
            }            
        } else {
            // for each channel, convert and copy the RX buffer to WK buffer
            for (j=0; j<IN_CHANNELS; j++) {
                // set the RX start pointer
                tempRxPtr = (unsigned char *)CaptureBuf + j*sizeof(short);
                // set the WK start pointer
                tempWkPtr = (unsigned char *)WorkBuf + j*(CaptureFrames)*sizeof(short);

                for (i=0; i<CaptureFrames; i++){
                    memcpy(tempWkPtr, tempRxPtr, sizeof(short));
                    tempWkPtr += sizeof(short);
                    tempRxPtr += sizeof(short)*(TOTAL_CAP_CHANNELS);
                }
            }

#ifdef REF_CHANNELS
            tempRxPtr = (unsigned char *)CaptureBufRef;
            tempWkPtr = (unsigned char *)WorkBuf + IN_CHANNELS*(CaptureFrames)*sizeof(short);

            for (i=0; i<CaptureFrames; i++){
                memcpy(tempWkPtr, tempRxPtr, sizeof(short));
                tempWkPtr += sizeof(short);
                tempRxPtr += sizeof(short)*(REF_CHANNELS);
            }      
#endif  
        }


        ////////////////////////////////////////////////////////////////////////////////////////////////////
        //////////////////////////////////   Signal Processing in   ////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef FILE_WRITE
        if (file_write_state == 1){
            tempTxPtr = FileBuf;
            tempMicPtr = (unsigned char *)mics_in[1];
            tempMicPtr2 = (unsigned char *)mics_in[1];

            for (i=0; i<CaptureFrames; i++)
            {
                // memcpy(tempTxPtr, tempMicPtr, sizeof(short));
                tempTxPtr += sizeof(short);
                tempMicPtr += sizeof(short);

                // memcpy(tempTxPtr, tempMicPtr2, sizeof(short));
                tempTxPtr += sizeof(short);
                tempMicPtr2 += sizeof(short);                  
            }
        }
#endif        

        int frame_size = (int)CaptureFrames;
        int16_t * data = &mics_in[0][0];
        int16_t * ref =  &refs_in[0][0];
        double DoA;
        int Beam_no = g_Beam_no;

        if (fourthArg==0) aspl_nr_config.AEC_filter_updated = 1;
        else if (fourthArg==1) aspl_nr_config.AEC_filter_updated = 0;

        int res = aspl_RET_FAIL;
        if (aspl_nr_config.AEC_filter_updated == 0) {
            res = aspl_AEC_process_filtersave(data, ref, frame_size, 1050, 1, 0.0, &aspl_nr_config); 
            if (res == aspl_RET_SUCCESS) {
                FILE *aspl_cfg_fd = NULL;

                if (NULL == aspl_cfg_fd) {
                    aspl_cfg_fd = fopen("/mnt/setting/markTaec.cfg", "wb");
                }
            
                if (NULL != aspl_cfg_fd) {
                    for (int i=0; i<aspl_nr_config.aec_Mic_N; i++) {
                        fwrite(&aspl_nr_config.AEC_filter_len[i], sizeof(int), 1, aspl_cfg_fd);
                        fwrite(&aspl_nr_config.AEC_globaldelay[i], sizeof(int), 1, aspl_cfg_fd);
                        fwrite(aspl_nr_config.AEC_filter_save[i], sizeof(float), aspl_nr_config.AEC_filter_len[i], aspl_cfg_fd);

                    }
                }

                if (NULL != aspl_cfg_fd) {
                    fclose(aspl_cfg_fd);
                    aspl_cfg_fd = NULL;
                }
                aspl_nr_config.AEC_filter_updated = 1;
                fourthArg = 0;
                printf("aspl AEC filter save done.\r\n");
            }
        }

        if (aspl_nr_config.AEC_filter_updated == 1) {

            if (aspl_nr_config.AEC_filter_loaded == 0){
                FILE *aspl_cfg_read_fd = NULL;

                if (NULL == aspl_cfg_read_fd) {
                    aspl_cfg_read_fd = fopen("/mnt/setting/markTaec.cfg", "rb");
                    if (aspl_cfg_read_fd == NULL)
                    {
                        printf("aspl AEC filter file open error\r\n");
                        // file open error, you can not read the file.
                    }
                }

                if (NULL != aspl_cfg_read_fd) {
                    for (int i=0; i<aspl_nr_config.aec_Mic_N; i++) {
                        fread(&aspl_nr_config.AEC_filter_len[i], sizeof(int), 1, aspl_cfg_read_fd);
                        fread(&aspl_nr_config.AEC_globaldelay[i], sizeof(int), 1, aspl_cfg_read_fd);
                        fread(aspl_nr_config.AEC_filter_save[i], sizeof(float), aspl_nr_config.AEC_filter_len[i], aspl_cfg_read_fd);
                    }
                    aspl_AEC_filterload(&aspl_nr_config);
                    aspl_nr_config.AEC_filter_loaded = 1;
                    printf("aspl AEC filter load done.\r\n");
                }


                if (NULL != aspl_cfg_read_fd) {
                    fclose(aspl_cfg_read_fd);
                    aspl_cfg_read_fd = NULL;
                }                

            }

            aspl_AEC_process_2ch(data, ref, frame_size, aspl_nr_config.AEC_globaldelay[0], 0.0, &aspl_nr_config);
            aspl_NR_process_2mic(data, frame_size, Beam_no, 1, &DoA);

        }

        ////////////////////////////////////////////////////////////////////////////////////////////////////
        //////////////////////////////////   Signal Processing out   ///////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////////////////

        ///////////////////////////   output buf sorting      ////////////////////////////////////////
#ifdef FILE_WRITE
        if (file_write_state == 1){
            tempTxPtr = FileBuf;
            tempMicPtr = (unsigned char *)mics_in[0];
            tempMicPtr2 = (unsigned char *)mics_in[1];

            for (i=0; i<CaptureFrames; i++)
            {
                memcpy(tempTxPtr, tempMicPtr, sizeof(short));
                tempTxPtr += sizeof(short);
                tempMicPtr += sizeof(short);

                memcpy(tempTxPtr, tempMicPtr2, sizeof(short));
                tempTxPtr += sizeof(short);
                tempMicPtr2 += sizeof(short);                   
            }
        }
#endif        

#ifdef FILE_WRITE
        if (file_write_state == 1){
            int ResFileBufSize = FrameCnt * (OUTPUT_CHANNELS) * NUM_BYTE_PER_SAMPLE;
            rc=write(fdmic, FileBuf, ResFileBufSize);
            if (rc != ResFileBufSize)
            {
                fprintf(stderr,"short write: wrote %d bytes\n", rc);
            }
        }
#endif

        tempTxPtr = PlayBuf;
        tempOutPtr = (unsigned char *)mics_in[0];
        tempOutPtr2 = (unsigned char *)mics_in[0];

        for (i=0; i<CaptureFrames; i++)
        {
            memcpy(tempTxPtr, tempOutPtr, sizeof(short));
            // if ((file_write_state == 0)||(file_play_on==1)){
            //     *((short *)tempTxPtr) = *((short *)tempOutPtr);
            // } else {
            //     *((short *)tempTxPtr) = 0;
            // }

            // *((short *)tempTxPtr) = 0;

            tempTxPtr += sizeof(short);
            tempOutPtr += sizeof(short);

            memcpy(tempTxPtr, tempOutPtr2, sizeof(short));
            // if ((file_write_state == 0)||(file_play_on==1)){
            //     *((short *)tempTxPtr) = *((short *)tempOutPtr2);
            // } else {
            //     *((short *)tempTxPtr) = 0;
            // }

            // *((short *)tempTxPtr) = 0;

            tempTxPtr += sizeof(short);
            tempOutPtr2 += sizeof(short);
        }

        PlayFrames = snd_pcm_writei(PlayHandle, PlayBuf, FrameCnt);
        if (PlayFrames < 0) {
            printf("snd_pcm_recover \n");
            PlayFrames = snd_pcm_recover(PlayHandle, PlayFrames, 0);
        } else {
            // printf("required play period : %d, actual : %d\n", FrameCnt, PlayFrames);
        }
        if (PlayFrames < 0)
        {
            printf("snd_pcm_writei failed: %s\n", snd_strerror(PlayFrames));
            // break;
        }
        if (PlayFrames > 0 && PlayFrames < (long)sizeof(PlayBuf))
            printf("Short write (expected %li, wrote %li)\n", (long)sizeof(PlayBuf), PlayFrames);
    }

    snd_pcm_drain(CaptureHandle);  
Err:
    if (CaptureHandle) snd_pcm_close(CaptureHandle);
#ifdef REF_CHANNELS       
    if (CaptureHandleRef) snd_pcm_close(CaptureHandleRef);
#endif    
    if (PlayHandle) snd_pcm_close(PlayHandle);
    if (PlayBuf) free(PlayBuf);
    if (CaptureBuf) free(CaptureBuf);
#ifdef REF_CHANNELS       
    if (CaptureBufRef) free(CaptureBufRef);
#endif    
    if (FileBuf) free(FileBuf);

    printf("proc_loop close\n");
}



int init_alsa(){

    int rc;
    int dir = 1;
    unsigned int val;

    char * CaptureDevice = "default";
    char * PlayDevice = "default";

	CaptureFrames = 512;

    rc = Alsa_pcm_open(&CaptureHandle, CaptureDevice, SND_PCM_STREAM_CAPTURE, TOTAL_CAP_CHANNELS, SAMPLING_FREQ, CaptureFrames);

    CaptureBufSize = (CaptureFrames) * (TOTAL_CAP_CHANNELS) * NUM_BYTE_PER_SAMPLE;
#ifdef FILE_AEC_test
    CaptureBufSize = (CaptureFrames) * (IN_CHANNELS+REF_CHANNELS_dummy) * NUM_BYTE_PER_SAMPLE;
    printf("\n Capture Buff, Num of Chan : %d, Framesize : %d, Bufsize : %d\n", IN_CHANNELS+REF_CHANNELS_dummy, CaptureFrames, CaptureBufSize);
#else     
    printf("\n Capture Buff, Num of Chan : %d, Framesize : %d, Bufsize : %d\n", TOTAL_CAP_CHANNELS, CaptureFrames, CaptureBufSize);
#endif
    CaptureBuf = (char *) malloc(CaptureBufSize);
    if (NULL == CaptureBuf)
    {
        printf("malloc ....failed\n");
        return -1;
    }

#ifndef CV2_mono 
    rc = Alsa_pcm_open(&CaptureHandleRef, CaptureDeviceRef, SND_PCM_STREAM_CAPTURE, REF_CHANNELS, SAMPLING_FREQ, CaptureFrames);    
    CaptureRefBufSize = (CaptureFrames) * REF_CHANNELS * NUM_BYTE_PER_SAMPLE;
    printf("\n Capture Ref Buff, Num of Chan : %d, Framesize : %d, Bufsize : %d\n", REF_CHANNELS, CaptureFrames, CaptureRefBufSize);
    CaptureBufRef = (char *) malloc(CaptureRefBufSize);
    if (NULL == CaptureBufRef)
    {
        printf("malloc ....failed\n");
        return -1;
    }
#endif

    WorkBufSize = CaptureFrames * (WORK_CHANNELS) * NUM_BYTE_PER_SAMPLE;
    printf("\n WorkBuff,         Num of Chan : %d, Framesize : %d, Bufsize : %d",(WORK_CHANNELS), CaptureFrames, WorkBufSize);
    WorkBuf = (char *) malloc(WorkBufSize);
    if (NULL == WorkBuf)
    {
        printf("malloc ....failed\n");
        return -1;
    }   

    if ((rc = snd_pcm_prepare(CaptureHandle)) < 0) {
        fprintf(stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(rc));
        return -1;
    }    

#ifndef CV2_mono 
    if ((rc = snd_pcm_prepare(CaptureHandleRef)) < 0) {
        fprintf(stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(rc));
        return -1;
    }     
#endif    


    ////////////// Alsa play settings /////////////
    rc=snd_pcm_open(&PlayHandle, PlayDevice, SND_PCM_STREAM_PLAYBACK, 0);
    if ( rc< 0)
    {
        fprintf(stderr, "Error snd_pcm_open [ %s]\n", PlayDevice);
        return -1;
    }

    rc=snd_pcm_set_params(PlayHandle,SND_PCM_FORMAT_S16,SND_PCM_ACCESS_RW_INTERLEAVED,OUTPUT_CHANNELS,SAMPLING_FREQ,1,500000);
    if (rc< 0)
    {
        printf("Playback open error: %s\n", snd_strerror(rc));
        return -1;
    }

    PlayBufSize = (CaptureFrames) * OUTPUT_CHANNELS * NUM_BYTE_PER_SAMPLE;
    printf("\n PlayBuff         ,Num of Chan : %d, Framesize : %d, Bufsize : %d",OUTPUT_CHANNELS, CaptureFrames, PlayBufSize);

    PlayBuf = (char*)malloc(PlayBufSize);
    if (NULL == PlayBuf)
    {
        printf("malloc ....failed\n");
        return -1;
    }

    FileBufSize = CaptureFrames * OUTPUT_CHANNELS  * NUM_BYTE_PER_SAMPLE;    

    printf("\n FileBuff, Num of Chan : %d, Framesize : %d, Bufsize : %d", OUTPUT_CHANNELS, CaptureFrames, FileBufSize);

    FileBuf = (char *) malloc(FileBufSize);
    if (NULL == FileBuf)
    {
        printf("malloc ....failed\n");
        return -1;
    }

    return 0;
}

int Alsa_pcm_open(snd_pcm_t **CaptureHandle, char *CaptureDevice, snd_pcm_stream_t mode, uint32_t num_channel, uint32_t sample_rate, snd_pcm_uframes_t period_size) {
    int rc;
    int dir ;
    unsigned int val;

    snd_pcm_uframes_t CaptureFrames = period_size;
    snd_pcm_uframes_t BufferSize = period_size*5;
    snd_pcm_hw_params_t *CaptureParams;

    int getperiod_time;
    snd_pcm_uframes_t getperiod_size;
    snd_pcm_uframes_t getbuffer_size;

    // Open Alsa device for Capture
    rc = snd_pcm_open(CaptureHandle, CaptureDevice, mode, 0);
    if (rc < 0) {
        fprintf(stderr, "Error snd_pcm_open [%s]\n", CaptureDevice);
        return -1;
    }

    // Allocate a hardware parameters object.
    snd_pcm_hw_params_alloca(&CaptureParams);

    // Fill it in with default values.
    snd_pcm_hw_params_any(*CaptureHandle, CaptureParams);

    // Set the desired hardware parameters.
    // Interleaved mode
    snd_pcm_hw_params_set_access(*CaptureHandle, CaptureParams, SND_PCM_ACCESS_RW_INTERLEAVED);

    // Signed 16-bit little-endian format
    snd_pcm_hw_params_set_format(*CaptureHandle, CaptureParams, SND_PCM_FORMAT_S16_LE);

    // 2 channels
    snd_pcm_hw_params_set_channels(*CaptureHandle, CaptureParams, num_channel);

    // 16000 sample/second sampling rate
    val = sample_rate;
    snd_pcm_hw_params_set_rate_near(*CaptureHandle, CaptureParams, &val, &dir);

    snd_pcm_hw_params_set_period_size_near(*CaptureHandle, CaptureParams, &CaptureFrames, &dir);

    BufferSize = period_size*5;

    // Buffer 크기 설정 (6400 샘플)
    snd_pcm_hw_params_set_buffer_size_near(*CaptureHandle, CaptureParams, &BufferSize);

    // Write the parameters to the driver
    rc = snd_pcm_hw_params(*CaptureHandle, CaptureParams);
    if (rc < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        return -1;
    }

    snd_pcm_hw_params_get_period_time(CaptureParams, &getperiod_time, 0);

    printf("[%s] required period time : %f, actual set time : %d\r\n", CaptureDevice, (float)period_size/(float)sample_rate*1000000.0, getperiod_time);

    snd_pcm_hw_params_get_period_size(CaptureParams, &getperiod_size, &dir);

    printf("[%s] required period size : %d, actual set size : %d\r\n", CaptureDevice, period_size, getperiod_size);

    snd_pcm_hw_params_get_buffer_size(CaptureParams, &getbuffer_size);

    BufferSize = period_size*5;

    printf("[%s] required buffer size : %d, actual set size : %d\r\n", CaptureDevice, BufferSize, getbuffer_size);

    return rc;
}


static void *key_loop(void *arg) {

    char s1[10];    // 크기가 10인 char형 배열을 선언

    
    Show_menu();

    while (!terminate) {

        if (_kbhit())
        {
            int ch = _getch();
            _putch(ch);

            if(ch =='q') {

                printf("\n\r");
                terminate = 1;
                break;
            }           
            else if (ch == '0'){
                printf("ASPL NR set as 0 \r\n");
                aspl_nr_config.enable = 0;
                aspl_nr_config.sensitivity = 0;
                char* file_path = aspl_nr_config.tuning_file_path;

                run_pause = 0;

                usleep(100);

                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&aspl_nr_config);

                usleep(100);

                run_pause = 1;

                // g_play_mic = 0;


            }
            else if (ch == '1'){
                printf("ASPL NR set as 1 \r\n");
                aspl_nr_config.enable = 1;
                aspl_nr_config.sensitivity = 1;
                char* file_path = aspl_nr_config.tuning_file_path;

                run_pause = 0;

                usleep(100);

                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&aspl_nr_config);

                usleep(100);

                run_pause = 1;    
                // g_play_mic = 1;          
            }
            else if (ch == '2'){
                printf("ASPL NR set as 2 \r\n");
                aspl_nr_config.enable = 1;
                aspl_nr_config.sensitivity = 2;
                char* file_path = aspl_nr_config.tuning_file_path;

                run_pause = 0;

                usleep(100);

                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&aspl_nr_config);

                usleep(100);

                run_pause = 1;
                // g_play_mic = 2;
            }
            else if (ch == '3'){
                printf("ASPL NR set as 3 \r\n");
                aspl_nr_config.enable = 1;
                aspl_nr_config.sensitivity = 3;
                char* file_path = aspl_nr_config.tuning_file_path;

                run_pause = 0;

                usleep(100);

                aspl_NR_set(aspl_NR_CMD_SET_NR, (void *)&aspl_nr_config);

                usleep(100);

                run_pause = 1;   
                // g_play_mic = 3;            
            }                     
                                        
            else if (ch == 'e'){

                time_t current_time;
                struct tm* time_info;
                char filename[100];

                // 현재 시간 획득
                time(&current_time);
                time_info = localtime(&current_time);

                // 파일명 생성
                strftime(filename, sizeof(filename), "./MarkT_nr_params_%y%m%d_%H%M.bin", time_info);
                printf("ASPL expert param save to %s \r\n", filename);
                aspl_NR_total_param_write_to_file(filename);   

            }         
#ifdef FILE_WRITE         
            else if (ch == 'f'){

                time_t current_time;
                struct tm* time_info;
                char filename[100];

                // 현재 시간 획득
                time(&current_time);
                time_info = localtime(&current_time);

                if (file_write_state == 0) {
                    // 파일 이름 생성
                    char filename[100];
                    char time_buffer[80];
                    
                    // 시간 정보를 포맷에 맞게 문자열로 변환
                    strftime(time_buffer, sizeof(time_buffer), "%m%d-%H%M", time_info);
#ifdef CV2_mono
                    snprintf(filename, sizeof(filename), "CV2_1mic_res_NR%d_%s.raw", aspl_nr_config.sensitivity , time_buffer);
#endif
                    if ((fdmic = open(filename, O_WRONLY | O_CREAT, 0644)) == -1) {  
                        fprintf(stderr, "Error open: [%s]\n", filename);  
                    }

                    printf("file : %s open\n", filename);

                    file_write_state = 1;

                } else if (file_write_state != 0) {
                    file_write_state = 0;
                    close(fdmic); 
                    printf("file : %s closed\n", filename);
                }
            }                      
            else if (ch == 'g'){

                time_t current_time;
                struct tm* time_info;
                char filename[100];

                // 현재 시간 획득
                time(&current_time);
                time_info = localtime(&current_time);

                if (file_write_state == 0) {
                    // 파일 이름 생성
                    char filename[100];
                    char time_buffer[80];
                    
                    // 시간 정보를 포맷에 맞게 문자열로 변환
                    strftime(time_buffer, sizeof(time_buffer), "%m%d-%H%M", time_info);
#ifdef CV2_mono
                    snprintf(filename, sizeof(filename), "CV2_1mic_raw_%s.raw", time_buffer);
#endif

                    if ((fdmic = open(filename, O_WRONLY | O_CREAT, 0644)) == -1) {  
                        fprintf(stderr, "Error open: [%s]\n", filename);  
                    }

                    printf("file : %s open\n", filename);

                    file_write_state = 2;

                } else if (file_write_state != 0) {
                    file_write_state = 0;
                    close(fdmic); 
                    printf("file : %s closed\n", filename);
                }
            }      
#endif  
            else if (ch == 'h'){
                if (file_play_on == 0) {
                    // const char* filename_read = "gain5_female_nonoise.raw";
                    file = open(g_filename, O_RDONLY);
                    
                    if (file == -1) {
                        printf("파일을 열 수 없습니다.");
                        break;
                    }

                    // 파일 크기 확인
                    struct stat st;
                    if (stat(g_filename, &st) != 0) {
                        printf("파일 크기를 확인할 수 없습니다.");
                        break;
                    }
                    off_t file_size = st.st_size;
                    int frame_size;
                    // 프레임 단위로 읽기

                    frame_size = ((OUTPUT_CHANNELS) * NUM_BYTE_PER_SAMPLE);  // 예시로 4바이트로 가정

                    int frames = file_size / frame_size;    

                    file_play_on = 2;

                } else if (file_play_on != 0) {
                    file_play_on = 0;
                    close(file); 
                    printf("file : %s closed\n", g_filename);
                }
            }
            else if (ch == 'r'){
                if (file_play_on == 0) { 
                    file_play_on = 3;
                } else if (file_play_on == 3) {
                    file_play_on = 4;
                } else if (file_play_on != 0) {
                    file_play_on = 0;
                }
            }   
     
#ifdef FILE_PLAY
            else if (ch == 's'){
                if (file_play_on == 0) {
                    // const char* filename_read = "gain5_female_nonoise.raw";
                    file = open(g_filename, O_RDONLY);
                    
                    if (file == -1) {
                        printf("파일을 열 수 없습니다.");
                        break;
                    }

                    // 파일 크기 확인
                    struct stat st;
                    if (stat(g_filename, &st) != 0) {
                        printf("파일 크기를 확인할 수 없습니다.");
                        break;
                    }
                    off_t file_size = st.st_size;
                    int frame_size;
                    // 프레임 단위로 읽기

                    frame_size = ((OUTPUT_CHANNELS) * NUM_BYTE_PER_SAMPLE);  // 예시로 4바이트로 가정

                    int frames = file_size / frame_size;    

                    file_play_on = 1;

                } else if (file_play_on != 0) {
                    file_play_on = 0;
                    close(file); 
                    printf("file : %s closed\n", g_filename);
                }
            }         

#endif            
#ifdef EXPERT_TUNE
            else if (ch == 'i'){
                param_set();
            }
#endif
      
#ifdef EXPERT_TUNE            
            else if (ch == 'p'){
                debug_matlab_onoff();
            }     
#endif                      
            else if (ch == 'u'){
                printf(" Gain up\r\n");
                g_Beam_no++;
                if (g_Beam_no>=8) {
                    g_Beam_no = 8;
                } 
                printf(" g_Beam_no up : %d\n", g_Beam_no); 
// #ifdef CV2_mono
//                 g_inputGain++;
//                 if (g_inputGain>=10) {
//                     g_inputGain = 10;
//                 }
//                 CV2_change_audioInDeviceSetting(g_inputSource, bias_on, g_inputGain);
//                 printf(" Gain up : %d\n", g_inputGain);
// #endif    
            }
            else if (ch == 'v'){
                printf(" Gain down\r\n");

                g_Beam_no--;
                if (g_Beam_no<=0) {
                    g_Beam_no = 0;
                }    
                printf(" g_Beam_no up : %d\n", g_Beam_no);             
// #ifdef CV2_mono
//                 g_inputGain--;
//                 if (g_inputGain<=0) {
//                     g_inputGain = 1;
//                 }
//                 CV2_change_audioInDeviceSetting(g_inputSource, bias_on, g_inputGain);
//                 printf(" Gain up : %d\n", g_inputGain);
// #endif    
            }

            // else if (ch == 't'){

            //     printf("NS On / Off\r\n");

            //     if (g_NS_enable == 1) {
            //         g_NS_enable = 0;
            //         printf("NS off \r\n");

            //     } else if (g_NS_enable == 0) {
            //         g_NS_enable = 1;
            //         printf("NS on \r\n");
            //     }
            // }  

            // else if (ch == 'w'){

            //     printf("AGC On / Off\r\n");

            //     if (g_AGC_enable == 1) {
            //         g_AGC_enable = 0;
            //         printf("AGC off \r\n");

            //     } else if (g_AGC_enable == 0) {
            //         g_AGC_enable = 1;
            //         printf("AGC on \r\n");
            //     }
            // }  
            else if (ch == 'x'){

                printf("Mic Bias On / Off\r\n");

                if (bias_on == 1) {
                    bias_on = 0;
#ifdef CV2_mono
                    g_inputSource = 1;
                    CV2_change_audioInDeviceSetting(g_inputSource, bias_on, g_inputGain);
                    printf("g_inputSource : 1 (internal) bias_on : %d (off)\n", bias_on);
#endif    

                } else if (bias_on == 0) {
                    bias_on = 1;
#ifdef CV2_mono
                    g_inputSource = 2;
                    CV2_change_audioInDeviceSetting(g_inputSource, bias_on, g_inputGain);
                    printf("g_inputSource : 2 (external) bias_on : %d (on)\n", bias_on);
#endif    
                }
            }             
            else {
                Show_menu();
            }

            Show_menu();
        }
        usleep(100);
    }

    printf("key_loop close\n");
    return NULL;
}

void Show_menu(){
 
        printf( "\r\n================== Main Menu ==========================\r\n");
        printf( "  0. NR off                                                \r\n");   
        printf( "  1. NR low                                                \r\n");   
        printf( "  2. NR mid                                                \r\n");   
        printf( "  3. NR high                                               \r\n");    
        printf( "  e. Param save file                                       \r\n");
      
#ifdef FILE_WRITE  
        printf( "  f. record a result audiofile                                \r\n");                  
        printf( "  g. record a raw audiofile                             \r\n");     
#endif              
        printf( "  h. play audiofile                                        \r\n");     
        printf( "  r. play test signal(impulse / sine)                      \r\n");
#ifdef FILE_PLAY             
        printf( "  s. play raw input file                                   \r\n");
#endif  
        
#ifdef EXPERT_TUNE                     
        printf( "  i. expert param tuning                                   \r\n");  
#endif              
        // printf( "  n. apply a profile                                       \r\n");     
        // printf( "  o. save a profile as current setting                     \r\n");   
#ifdef EXPERT_TUNE        
        printf( "  p. matlab debug connect / disconnect                     \r\n");  
#endif        
        printf( "  u. gain up                                               \r\n");
        printf( "  v. gain down                                             \r\n");
        printf( "  t. ns on/off                                             \r\n");
        printf( "  w. agc on/off                                            \r\n");        
  
#ifdef CV2_mono        
        printf( "  x. internal/external mic toggle                              \r\n");
#endif      
        printf( "  q. Exit Application                                      \r\n");
        printf( "  To print this memu, press any other key                  \r\n");
        printf(   "=======================================================\r\n\n");
}


static void signal_handler(int signal) {
    printf("\n\r");
    printf("Signal Caught.\n");

    terminate = 1;

    // sem_post(&log_sem);
}

#ifdef CV2_mono

void CV2_change_audioInDeviceSetting(int source, int mic_bias, int gain) {

  char cmd[128] = { 0, };

  switch (source) {
  default:
  case 0: // Line-in
    snprintf(cmd, sizeof(cmd), "amixer sset 'Mic Bias' 0");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Right PGA Mixer Line1L' on");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Left PGA Mixer Line1L' on");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Right PGA Mixer Mic2L' off");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Left PGA Mixer Mic2L' off");
    system(cmd);
    break;
  case 1: // Internal Mic
    snprintf(cmd, sizeof(cmd), "amixer sset 'Mic Bias' 0");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Right PGA Mixer Line1L' off");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Left PGA Mixer Line1L' off");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Right PGA Mixer Mic2L' on");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Left PGA Mixer Mic2L' on");
    system(cmd);
    break;
  case 2: // External Mic
    if (mic_bias) {
      snprintf(cmd, sizeof(cmd), "amixer sset 'Mic Bias' 3");
      system(cmd);
    } else {
      snprintf(cmd, sizeof(cmd), "amixer sset 'Mic Bias' 0");
      system(cmd);
    }
    snprintf(cmd, sizeof(cmd), "amixer sset 'Right PGA Mixer Line1L' on");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Left PGA Mixer Line1L' on");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Right PGA Mixer Mic2L' off");
    system(cmd);
    snprintf(cmd, sizeof(cmd), "amixer sset 'Left PGA Mixer Mic2L' off");
    system(cmd);
    break;
  }

  int gain_ctrl_map[3][10] = {
      {   0,  20,  36,  44,  52,  60,  68,  76,  86,  92 }, // Line-in
      {   0,  24,  36,  44,  52,  60,  68,  77,  85,  93 }, // Int. Mic
      {   0,  59,  63,  71,  80,  90,  99, 107, 115, 119 }, // Ext. Mic
  };

  snprintf(cmd, sizeof(cmd), "amixer sset 'PGA' %d", gain_ctrl_map[source][(gain - 1) % 10]);
  system(cmd);


  int output_gain_ctrl_map[1][10] = {
      {  84,  87,  90,  94,  98, 104, 110, 115, 121, 127 },
  };

  int outputgain = 5;

  snprintf(cmd, sizeof(cmd), "amixer sset 'PCM' %d", output_gain_ctrl_map[0][(outputgain - 1) % 10]);  
  system(cmd);
}
#endif