/*
 * debug_file.c
 *
 *	Created on: 2019. 7. 1.
 *		Author: Seong Pil Moon
 */

#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>  
#include <unistd.h>
#include "debug_file.h"
#include "sys.h"
  
#ifdef DEBUG_SSL_VAD

FILE *fp1[DEBUGFILENUM];

void debug_file_open(){

	int i;
	char s1[10];

	for (i=0; i<DEBUGFILENUM; i++){
		sprintf(s1, "debug%d.txt", i);
		remove(s1);
		fp1[i] = fopen(s1, "w");
	}	
}

void debug_file_close(){
	for (int i=0; i<DEBUGFILENUM; i++)
	fclose(fp1[i]);
}

int debug_file_write_int(int value, int filenum){
	if (filenum>=DEBUGFILENUM) {
		printf("file num exceeds the total debug file number.\n");
		return -1;
	}
	fprintf(fp1[filenum], "%d ", value);
	return 0;
}

int debug_file_write_double(double value, int filenum){
	if (filenum>=DEBUGFILENUM) {
		printf("file num exceeds the total debug file number.\n");
		return -1;
	}
	fprintf(fp1[filenum], "%f ", value);
	return 0;
}

int debug_file_write_return(int filenum){
	if (filenum>=DEBUGFILENUM) {
		printf("file num exceeds the total debug file number.\n");
		return -1;
	}
	fprintf(fp1[filenum], "\n");
	return 0;
}

#endif

uint16_t swap_endian16(uint16_t value) {
    return (value << 8) | (value >> 8);
}


uint32_t littleEndianToBigEndian(uint32_t data) {
    return ((data >> 24) & 0xFF) | ((data >> 8) & 0xFF00) | ((data << 8) & 0xFF0000) | ((data << 24) & 0xFF000000);
}

#define DEBUG_NUM 2
#define SPECTRUM_SIZE 128
#define SPECTRUM_NUM  3

#define CCV_SIZE1 ITD_size
#define CCV_SIZE2 ITD_size2
#define CCV_SIZE_MAX 250
#define CCV_NUM  4

#define FRAMES_PER_BUFFER (512)
#define SERVER_IP "192.168.1.151"
#define SERVER_PORT (30000)
#define PACKET_SYMBOL (0xABCD)

uint32_t * matlab_buffer[DEBUG_NUM] = {NULL};
uint32_t * matlab_spectrum[SPECTRUM_NUM] = {NULL};
uint32_t * matlab_ccv[CCV_NUM] = {NULL};

typedef struct {
    uint16_t symbol;
    uint16_t data_len;
} PacketHeader;

int client_socket = 0;

static int gui_server_on = 0;

int g_vad_debug_on = 0;
int g_agc_debug_on = 0;
int g_ns_debug_on = 0;
int g_ssl_debug_on = 0;
int g_aec_debug_on = 0;


int g_vad_debug_snd_idx=0;
int g_agc_debug_snd_idx=0;
int g_ns_debug_snd_idx=0;
int g_ssl_debug_snd_idx=0;
int g_aec_debug_snd_idx=0;

int g_vad_debug_snd_period;
int g_agc_debug_snd_period;
int g_ns_debug_snd_period;
int g_ssl_debug_snd_period;
int g_aec_debug_snd_period;

int debug_matlab_onoff(){
	if (gui_server_on==0) {
		debug_matlab_open();
	} else if (gui_server_on==1) {
		debug_matlab_close();
	}
	return 0;
}

int debug_matlab_open(){

	g_vad_debug_snd_period = (int)(0.2 * (16000.0) / 256.0);
	g_agc_debug_snd_period = (int)(0.2 * (16000.0) / 512.0);
	g_ns_debug_snd_period = (int)(0.2 * (16000.0) / 512.0);
	g_ssl_debug_snd_period = (int)(0.5 * (16000.0) / (256.0/4));
	g_aec_debug_snd_period = (int)(0.2 * (16000.0) / 1600.0);

    // Setup TCP/IP client
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(SERVER_PORT);
    server_address.sin_addr.s_addr = inet_addr(SERVER_IP); // Change this to your server IP

	if (connect(client_socket, (struct sockaddr *) &server_address, sizeof(server_address)) < 0) {
		printf("\n Server connection failed, continuing with audio input \n");

		gui_server_on = 0;
		
		return aspl_RET_FAIL;
	} else {

		gui_server_on = 1;
		printf("\n Server connected \n");

		for (int i=0; i<DEBUG_NUM; i++){
			matlab_buffer[i] = malloc(sizeof(uint32_t)*DEBUGFILENUM);

			if (matlab_buffer[i] == NULL) {
				if (gui_server_on){
					close(client_socket);
					gui_server_on = 0;
				}
				printf("matlab debug malloc failed (AGC, VAD)\r\n");
				debug_matlab_close();
				
				return aspl_RET_FAIL_MALLOC;
			}
		}

		for (int i=0; i<SPECTRUM_NUM; i++){
			matlab_spectrum[i] = malloc(sizeof(uint32_t)*SPECTRUM_SIZE);

			if (matlab_spectrum[i] == NULL) {
				if (gui_server_on){
					close(client_socket);
					gui_server_on = 0;
				}

				printf("matlab debug malloc failed (spectrum debug(%d)\r\n", i);
				debug_matlab_close();

				return aspl_RET_FAIL_MALLOC;
			}
		}

		for (int i=0; i<CCV_NUM; i++){
			matlab_ccv[i] = malloc(sizeof(uint32_t)*CCV_SIZE_MAX);

			if (matlab_ccv[i] == NULL) {
				if (gui_server_on){
					close(client_socket);
					gui_server_on = 0;
				}

				printf("matlab debug malloc failed (ccv debug(%d)\r\n", i);
				debug_matlab_close();

				return aspl_RET_FAIL_MALLOC;
			}
		}


		return aspl_RET_SUCCESS;
	}
}

void debug_matlab_close(){

	for (int i=0; i<DEBUG_NUM; i++){
		if (matlab_buffer[i] != NULL) {
			free(matlab_buffer[i]);
		}
	}

	for (int i=0; i<SPECTRUM_NUM; i++){
		if (matlab_spectrum[i]  != NULL) {
			free(matlab_spectrum[i] );
		}
	}

	for (int i=0; i<CCV_NUM; i++){
		if (matlab_ccv[i]  != NULL) {
			free(matlab_ccv[i] );
		}
	}	

	if (gui_server_on){
		close(client_socket);
		gui_server_on = 0;
		printf("\n Server closed \n");
	}

}

int debug_matlab_int(int debugnum, int value, int num){
	if ((num>=DEBUGFILENUM) || (debugnum>=DEBUG_NUM)) {
		printf("num exceeds the total debug number.\n");
		return aspl_RET_FAIL;
	}

	if (matlab_buffer[debugnum] == NULL) {
		// printf("debug buffer not malloced\n");
		return aspl_RET_FAIL;
	}
	
	uint32_t temp;

	memcpy(&temp, &value, sizeof(uint32_t));
	matlab_buffer[debugnum][num] = littleEndianToBigEndian(temp);

	return aspl_RET_SUCCESS;
}

int debug_matlab_float(int debugnum, float value, int num){
	if ((num>=DEBUGFILENUM) || (debugnum>=DEBUG_NUM)) {
		printf("num exceeds the total debug number.\n");
		return aspl_RET_FAIL;
	}

	if (matlab_buffer[debugnum] == NULL) {
		// printf("debug buffer not malloced\n");
		return aspl_RET_FAIL;
	}

	uint32_t temp;
	memcpy(&temp, &value, sizeof(float));
	matlab_buffer[debugnum][num] = littleEndianToBigEndian(temp);

	return aspl_RET_SUCCESS;
}

int debug_matlab_send(int debugnum){

	if (matlab_buffer[debugnum] == NULL) {
		// printf("debug buffer not malloced\n");
		return aspl_RET_FAIL;
	}
	
	if (gui_server_on==0){
		// printf("Not connected with the server.\n");
		return aspl_RET_FAIL;
	}

	uint32_t frametype[2];
	frametype[0] = littleEndianToBigEndian((uint32_t)debugnum);
	frametype[1] = littleEndianToBigEndian(sizeof(uint32_t)*DEBUGFILENUM);

	send(client_socket, &frametype[0], 2*sizeof(uint32_t), 0);

	send(client_socket, &matlab_buffer[debugnum][0], sizeof(uint32_t)*DEBUGFILENUM, 0);
	// printf("debug buffer sent %d bytes\n", sizeof(uint32_t)*DEBUGFILENUM);

	return aspl_RET_SUCCESS;
}


int debug_matlab_spectrum_int(int* p_spec, int spec_num, int spec_len){
	if (spec_num>=SPECTRUM_NUM) {
		printf("spec_num exceeds the total spectrum number.\n");
		return aspl_RET_FAIL;
	}

	if (spec_len>SPECTRUM_SIZE) {
		printf("spec_len exceeds the total spectrum size.\n");
		return aspl_RET_FAIL;
	}

	if (matlab_spectrum[spec_num] == NULL) {
		printf("spectrum debug buffer not malloced\n");
		return aspl_RET_FAIL;
	}
	
	for (int i=0; i<spec_len; i++){
		matlab_spectrum[spec_num][i] = littleEndianToBigEndian((uint32_t)p_spec[i]);
	}
	
	// matlab_buffer[num] = temp;

	return aspl_RET_SUCCESS;
}

int debug_matlab_spectrum_send(int spec_num){

	if (spec_num>SPECTRUM_NUM) {
		printf("spec_num exceeds the total spectrum number.\n");
		return aspl_RET_FAIL;
	}

	for (int i=0; i<spec_num; i++){
		if (matlab_spectrum[i] == NULL) {
			// printf("spectrum debug buffer not malloced\n");
			return aspl_RET_FAIL;
		}		
	}
	
	if (gui_server_on==0){
		// printf("Not connected with the server.\n");
		return aspl_RET_FAIL;
	}

	uint32_t frametype = littleEndianToBigEndian((uint32_t)2);
	uint32_t frameleng = littleEndianToBigEndian(sizeof(uint32_t)*SPECTRUM_SIZE*spec_num);

	send(client_socket, &frametype, sizeof(uint32_t), 0);
	send(client_socket, &frameleng, sizeof(uint32_t), 0);	

	for (int i=0; i<spec_num; i++){
		send(client_socket, matlab_spectrum[i], sizeof(uint32_t)*SPECTRUM_SIZE, 0);
	}

	return aspl_RET_SUCCESS;
}

#define FFT_SIZE 256
#define SAMPLE_RATE 16000

// 1/3 옥타브 대표 주파수
const float third_octave_freqs[] = {31, 93, 156, 218, 281, 343, 406, 468, 531, 593, 656, 800, 1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 7500};

void calculate_third_octave_spectrum(const int32_t *spectrum, int32_t *result) {
    size_t freq_count = sizeof(third_octave_freqs) / sizeof(float);
    for(size_t i = 0; i < freq_count - 1; i++) {
        float start_freq = third_octave_freqs[i];
        float end_freq = third_octave_freqs[i + 1];
        int start_idx = (start_freq / (SAMPLE_RATE / 2)) * (FFT_SIZE / 2);
        int end_idx = (end_freq / (SAMPLE_RATE / 2)) * (FFT_SIZE / 2);

        int32_t sum = 0;
        for(int j = start_idx; j < end_idx; j++) {
            sum += spectrum[j];
        }

        result[i] = (int32_t)(sum / (end_idx - start_idx));
    }
}

int debug_matlab_spectrum_oct_int(int* p_spec, int spec_num, int spec_len){
	if (spec_num>=SPECTRUM_NUM) {
		printf("spec_num exceeds the total spectrum number.\n");
		return aspl_RET_FAIL;
	}

	if (spec_len>SPECTRUM_SIZE) {
		printf("spec_len exceeds the total spectrum size.\n");
		return aspl_RET_FAIL;
	}

	if (matlab_spectrum[spec_num] == NULL) {
		// printf("spectrum debug buffer not malloced\n");
		return aspl_RET_FAIL;
	}
	
	size_t freq_count = sizeof(third_octave_freqs) / sizeof(float) - 1;

	int32_t third_octave_spectrum[freq_count];

	calculate_third_octave_spectrum(p_spec, third_octave_spectrum);

	for (int i=0; i<freq_count; i++){
		matlab_spectrum[spec_num][i] = littleEndianToBigEndian((uint32_t)third_octave_spectrum[i]);
	}
	
	// matlab_buffer[num] = temp;

	return aspl_RET_SUCCESS;
}

int debug_matlab_spectrum_oct_send(int spec_num){

	if (spec_num>SPECTRUM_NUM) {
		printf("spec_num exceeds the total spectrum number.\n");
		return aspl_RET_FAIL;
	}

	for (int i=0; i<spec_num; i++){
		if (matlab_spectrum[i] == NULL) {
			// printf("spectrum debug buffer not malloced\n");
			return aspl_RET_FAIL;
		}		
	}
	
	if (gui_server_on==0){
		// printf("Not connected with the server.\n");
		return aspl_RET_FAIL;
	}

	size_t freq_count = sizeof(third_octave_freqs) / sizeof(float) - 1;

	uint32_t frametype[2];
	frametype[0] = littleEndianToBigEndian((uint32_t)2);
	frametype[1] = littleEndianToBigEndian(sizeof(uint32_t)*freq_count*spec_num);

	send(client_socket, &frametype[0], 2*sizeof(uint32_t), 0);

	for (int i=0; i<spec_num; i++){
		send(client_socket, matlab_spectrum[i], sizeof(uint32_t)*freq_count, 0);
	}

	return aspl_RET_SUCCESS;
}


int debug_matlab_ccv_double(double* p_ccv, int ccv_num, int ccv_len){

	if (ccv_num>=CCV_NUM) {
		printf("ccv_num exceeds the total ccv number.\n");
		return aspl_RET_FAIL;
	}

	// if (ccv_num==0){
	// 	if (ccv_len>CCV_SIZE2) {
	// 		printf("ccv_len exceeds the total ccv size2.\n");
	// 		return aspl_RET_FAIL;
	// 	}
	// } else if ((ccv_num==1)||(ccv_num==2)){
	// 	if (ccv_len>CCV_SIZE1) {
	// 		printf("ccv_len exceeds the total ccv size2.\n");
	// 		return aspl_RET_FAIL;
	// 	}
	// }

	if (matlab_ccv[ccv_num] == NULL) {
		printf("ccv debug buffer not malloced\n");
		return aspl_RET_FAIL;
	}
	
	for (int i=0; i<ccv_len; i++){

		float temp_f = (float)(p_ccv[i]);
		uint32_t temp;

		memcpy(&temp, &temp_f, sizeof(float));
		matlab_ccv[ccv_num][i] = littleEndianToBigEndian(temp);

		// printf("p_ccv[%d]=%f, temp_f=%f, temp=%x, matlab_ccv[ccv_num][i]=%x\n", i, p_ccv[i], temp_f, temp, matlab_ccv[ccv_num][i]);

	}

	return aspl_RET_SUCCESS;
}

int debug_matlab_ccv_double2(double* p_ccv, double peak, int ccv_num, int ccv_len){

	if (ccv_num>=CCV_NUM) {
		printf("ccv_num exceeds the total ccv number.\n");
		return aspl_RET_FAIL;
	}

	if (matlab_ccv[ccv_num] == NULL) {
		printf("ccv debug buffer not malloced\n");
		return aspl_RET_FAIL;
	}
	
	for (int i=0; i<ccv_len; i++){

		float temp_f = (float)(p_ccv[i]*peak);
		uint32_t temp;

		memcpy(&temp, &temp_f, sizeof(float));
		matlab_ccv[ccv_num][i] = littleEndianToBigEndian(temp);
	}

	return aspl_RET_SUCCESS;
}

int debug_matlab_ccv_send(int ccv_num, int ccv_leng){

	if (ccv_num>=CCV_NUM) {
		printf("ccv_num exceeds the total ccv number.\n");
		return aspl_RET_FAIL;
	}

	if (matlab_ccv[ccv_num] == NULL) {
		// printf("spectrum debug buffer not malloced\n");
		return aspl_RET_FAIL;
	}		
	
	if (gui_server_on==0){
		// printf("Not connected with the server.\n");
		return aspl_RET_FAIL;
	}

	uint32_t frametype = littleEndianToBigEndian((uint32_t)3+ccv_num);
	uint32_t frameleng = littleEndianToBigEndian(sizeof(uint32_t)*ccv_leng);

	send(client_socket, &frametype, sizeof(uint32_t), 0);
	send(client_socket, &frameleng, sizeof(uint32_t), 0);	

	send(client_socket, matlab_ccv[ccv_num], sizeof(uint32_t)*ccv_leng, 0);

	return aspl_RET_SUCCESS;
}

int debug_matlab_doa_send(int num, double DoA){

	if (gui_server_on==0){
		// printf("Not connected with the server.\n");
		return aspl_RET_FAIL;
	}

	uint32_t frametype = littleEndianToBigEndian((uint32_t)num);
	uint32_t frameleng = littleEndianToBigEndian(sizeof(uint32_t));

	send(client_socket, &frametype, sizeof(uint32_t), 0);
	send(client_socket, &frameleng, sizeof(uint32_t), 0);	

	float temp_f = (float)(DoA);
	uint32_t temp;

	memcpy(&temp, &temp_f, sizeof(float));
	temp = littleEndianToBigEndian(temp);

	send(client_socket, &temp, sizeof(uint32_t), 0);

	return aspl_RET_SUCCESS;
}

int debug_matlab_ssl_vad_send(int32_t vad){

	if (gui_server_on==0){
		// printf("Not connected with the server.\n");
		return aspl_RET_FAIL;
	}

	uint32_t frametype = littleEndianToBigEndian((uint32_t)8);
	uint32_t frameleng = littleEndianToBigEndian(sizeof(uint32_t));

	send(client_socket, &frametype, sizeof(uint32_t), 0);
	send(client_socket, &frameleng, sizeof(uint32_t), 0);	

	uint32_t temp = (uint32_t)vad;

	temp = littleEndianToBigEndian(temp);

	send(client_socket, &temp, sizeof(uint32_t), 0);

	return aspl_RET_SUCCESS;
}


int debug_matlab_ssl_float_send(int num, float val){

	if (gui_server_on==0){
		// printf("Not connected with the server.\n");
		return aspl_RET_FAIL;
	}

	uint32_t frametype = littleEndianToBigEndian((uint32_t)num);
	uint32_t frameleng = littleEndianToBigEndian(sizeof(uint32_t));

	send(client_socket, &frametype, sizeof(uint32_t), 0);
	send(client_socket, &frameleng, sizeof(uint32_t), 0);	

	uint32_t temp;
	memcpy(&temp, &val, sizeof(float));

	temp = littleEndianToBigEndian(temp);

	send(client_socket, &temp, sizeof(uint32_t), 0);

	return aspl_RET_SUCCESS;
}

int debug_matlab_float_array_send(int type, float* val, int num){

	if (gui_server_on==0){
		// printf("Not connected with the server.\n");
		return aspl_RET_FAIL;
	}

	uint32_t frametype = littleEndianToBigEndian((uint32_t)type);
	uint32_t frameleng = littleEndianToBigEndian(num*sizeof(uint32_t));

	send(client_socket, &frametype, sizeof(uint32_t), 0);
	send(client_socket, &frameleng, sizeof(uint32_t), 0);	

	uint32_t temp[1024];
	memcpy(&temp[0], val, num*sizeof(float));

	for (int i=0; i<num; i++){
		temp[i] = littleEndianToBigEndian(temp[i]);
	}

	send(client_socket, &temp[0], num*sizeof(uint32_t), 0);

	return aspl_RET_SUCCESS;
}

int debug_matlab_int_array_send(int type, int32_t* val, int num){

	if (gui_server_on==0){
		// printf("Not connected with the server.\n");
		return aspl_RET_FAIL;
	}

	uint32_t frametype = littleEndianToBigEndian((uint32_t)type);
	uint32_t frameleng = littleEndianToBigEndian(num*sizeof(uint32_t));

	send(client_socket, &frametype, sizeof(uint32_t), 0);
	send(client_socket, &frameleng, sizeof(uint32_t), 0);	

	uint32_t temp[2000];
	memcpy(&temp[0], &val[0], num*sizeof(int32_t));

	for (int i=0; i<num; i++){
		temp[i] = littleEndianToBigEndian(temp[i]);
	}

	send(client_socket, &temp[0], num*sizeof(uint32_t), 0);

	return aspl_RET_SUCCESS;
}


int debug_matlab_ssl_float_packet_send(int num, FloatPacket packet) {
    if (gui_server_on == 0) {
        // printf("Not connected with the server.\n");
        return aspl_RET_FAIL;
    }

    // Frame type과 length 설정
    uint32_t frametype = littleEndianToBigEndian((uint32_t)num);
    uint32_t frameleng = littleEndianToBigEndian(sizeof(FloatPacket));

    // Frame type과 length 전송
    send(client_socket, &frametype, sizeof(uint32_t), 0);
    send(client_socket, &frameleng, sizeof(uint32_t), 0);	

    // 패킷 내 float 값들 변환 및 전송
    uint32_t temp;
    for (int i = 0; i < 6; i++) {
        memcpy(&temp, &((float*)&packet)[i], sizeof(float));
        temp = littleEndianToBigEndian(temp);
        send(client_socket, &temp, sizeof(uint32_t), 0);
    }

    return aspl_RET_SUCCESS;
}


void debug_matlab_audio (short* audiobuffer, short len){
            // 데이터 길이 및 패킷 심볼 추가
            PacketHeader header;
            header.symbol = htons(PACKET_SYMBOL);
            header.data_len = htonl(len * sizeof(short));

            // 패킷 헤더 전송
            ssize_t header_sent = send(client_socket, &header, sizeof(PacketHeader), 0);
            if (header_sent < 0) {
                printf("Header send error");
            }

            // 오디오 데이터 전송
            ssize_t data_sent = send(client_socket, audiobuffer, len * sizeof(short), 0);
            if (data_sent < 0) {
                printf("Data send error");
            }	
}