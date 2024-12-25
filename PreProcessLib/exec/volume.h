
#ifdef QCORE_4mic
int g_inputGain = 220;
#endif
#ifdef QCORE
int g_inputGain = 4;
int analog_index_mt8137[10] = {0, 3, 1, 2, 7, 0, 3, 7, 6, 7}; // 4db, 7db, 12db, 15db, 19db, 23db, 27db, 31db, 35db, 39db
int digital_gain_mt8137[10] = {65536, 65536, 327680, 327680, 65536, 589824, 655360, 262144, 589824, 655360}; 
#endif
#ifdef WN7
int g_inputGain = 4;
int analog_index_wn7_line[11] = {0, 20, 36, 44, 52, 60, 68, 76, 86, 92, 52};
int analog_index_wn7_internal[11] = {12, 35, 47, 55, 63, 72, 81, 89, 97, 105, 63};
int analog_index_wn7_external[11] = {0, 59, 63, 71, 80, 90, 99, 107, 115, 119, 80};
#endif
#ifdef CV2
int g_inputGain = 201;
#endif
#ifdef CV2_SD
int g_inputGain = 0;


int gain_ctrl_map[10] = {
          210,  215,  220,  225,  230,  235,  240,  245,  250,  255	 // 2.5dB gap, from 4.5dB to 27dB
  };  //  4.5dB 7.0dB 9.5dB 12dB 14.5dB 17dB  19.5dB 22dB 24.5dB 27dB
      //  gain1 gain2 gain3 gain4 g5    g6    g7     g8   g9    g10

double gain_lin_map[10] = {
    1.67880401812256, // gain1, 4.5dB
	2.23872113856834,// gain2, 7.0dB
    2.98538261891796,// gain3, 9.5dB
    3.98107170553497,// gain4, 12.0dB
    5.30884444230988,// gain5, 14.5dB
    7.07945784384138,// gain6, 17.0dB
    9.44060876285923,// gain7, 19.5dB
    12.5892541179417,// gain8, 22.0dB
    16.7880401812256,// gain9, 24.5dB
    22.3872113856834// gain10, 27.0dB
    };


#endif
#ifdef CV2_mono
int g_inputGain = 5;
int g_inputSource = 0; // 0: line, 1: internal 2: external
#endif
int bias_on = 0;
int g_rec_on = 0;


