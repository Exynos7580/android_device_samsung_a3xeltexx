/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __EXYNOS_AUDIOCONF_H__
#define __EXYNOS_AUDIOCONF_H__



/********************************************************************/
/** ALSA Framework Sound Card & Sound Device Information            */
/**                                                                 */
/** You can find Sound Device Name from /dev/snd.                   */
/** Sound Device Name consist of Card Number & Device Number.       */
/**                                                                 */
/********************************************************************/

/* Sound Card Number based on Target Device */
/* You have to match this number with real kernel information */
#define SOUND_CARD0 0

/* You have to select this number based on your AudioHAL's service */
#define PRIMARY_SOUND_CARD       SOUND_CARD0
#define LOW_LATENCY_SOUND_CARD   SOUND_CARD0
#define DEEP_BUFFER_SOUND_CARD   SOUND_CARD0
#define COMPR_OFFLOAD_SOUND_CARD SOUND_CARD0
#define VOICE_CALL_SOUND_CARD    SOUND_CARD0
#define AUX_DIGITAL_SOUND_CARD   SOUND_CARD0   // Not Defined
#define BT_SCO_SOUND_CARD        SOUND_CARD0
#define FM_SOUND_CARD            SOUND_CARD0


/* You have to match this number with real kernel information */
#define PRIMARY_SOUND_DEVICE       5   // Primary Device
#define SECONDARY_SOUND_DEVICE     1   // Secondary Device
#define VOICE_CALL_SOUND_DEVICE    2   // CP Voice Call Device

#define FM_SOUND_DEVICE            4   // EAX1 for SoundCamp
#define EAX2_SOUND_DEVICE          5   // EXA2 for deep buffer playback
#define EAX3_SOUND_DEVICE          6   // EAX3 for reserved

#define COMPR_OFFLOAD_SOUND_DEVICE 7   // Compress Offload Playback

#define CAPTURE_SOUND_DEVICE   0   // Not defined
#define BT_SCO_SOUND_DEVICE        3   // Not defined

/* You have to select this number based on your AudioHAL's service */
// Playback Devices
#define PRIMARY_PLAYBACK_DEVICE       PRIMARY_SOUND_DEVICE
#define LOW_LATENCY_PLAYBACK_DEVICE   PRIMARY_SOUND_DEVICE
#define DEEP_BUFFER_PLAYBACK_DEVICE   SECONDARY_SOUND_DEVICE
#define COMPR_OFFLOAD_PLAYBACK_DEVICE COMPR_OFFLOAD_SOUND_DEVICE
#define VOICE_CALL_PLAYBACK_DEVICE    VOICE_CALL_SOUND_DEVICE
#define AUX_DIGITAL_PLAYBACK_DEVICE   AUX_DIGITAL_SOUND_DEVICE
#define BT_SCO_PLAYBACK_DEVICE        BT_SCO_SOUND_DEVICE


// Capture Devices
#define LOW_LATENCY_CAPTURE_DEVICE    CAPTURE_SOUND_DEVICE
#define VOICE_CALL_CAPTURE_DEVICE     VOICE_CALL_SOUND_DEVICE



/**
 ** PCM Configuration for Stop_Threshold
 **/
// EAX Mixer has 480 Frames(10ms) Buffer. SO limitation is under 480 Frames
#define EAX_MIXER_PERIOD_SIZE     480
#define EAX_STOP_PERIOD_SIZE      EAX_MIXER_PERIOD_SIZE / 4
#define NORMAL_STOP_PERIOD_SIZE   EAX_MIXER_PERIOD_SIZE

/**
 ** Default PCM Device Configurations
 **/
#define DEFAULT_OUTPUT_CHANNELS             2       // Stereo
#define DEFAULT_OUTPUT_SAMPLING_RATE        48000   // 48KHz

#define FM_OUTPUT_PERIOD_SIZE          256     // 480 frames, 10ms in case of 48KHz Stream
#define FM_OUTPUT_PERIOD_COUNT         4

#define PRIMARY_OUTPUT_PERIOD_SIZE          256     // 480 frames, 10ms in case of 48KHz Stream
#define PRIMARY_OUTPUT_PERIOD_COUNT         2       // Total 7,680 Bytes(40ms) = 480 * 2(Stereo) * 2(16bit PCM) * 4(Buffers)
#define PRIMARY_OUTPUT_MAX_PERIOD_SIZE      PRIMARY_OUTPUT_PERIOD_SIZE * PRIMARY_OUTPUT_PERIOD_COUNT
// Primary Stream use EAX Mixer
#define PRIMARY_OUTPUT_STOP_THREASHOLD      PRIMARY_OUTPUT_MAX_PERIOD_SIZE - EAX_STOP_PERIOD_SIZE

#define LOW_LATENCY_OUTPUT_PERIOD_SIZE      256     // 480 frames, 10ms in case of 48KHz Stream
#define LOW_LATENCY_OUTPUT_PERIOD_COUNT     2       // Total 3,840 Bytes(20ms) = 480 * 2(Stereo) * 2(16bit PCM) * 2(Buffers)
#define LOW_LATENCY_OUTPUT_MAX_PERIOD_SIZE  LOW_LATENCY_OUTPUT_PERIOD_SIZE * LOW_LATENCY_OUTPUT_PERIOD_COUNT
// LOW LATENCY(Fast) Stream doesn't use EAX Mixer
#define LOW_LATENCY_OUTPUT_STOP_THREASHOLD  INT_MAX

#define DEEP_BUFFER_OUTPUT_PERIOD_SIZE      480     // 960 frames, 20ms in case of 48KHz Stream
#define DEEP_BUFFER_OUTPUT_PERIOD_COUNT     8       // Total 16,200 Bytes(100ms) = 960 * 2(Stereo) * 2(16bit PCM) * 5(Buffers)
#define DEEP_BUFFER_OUTPUT_MAX_PERIOD_SIZE  DEEP_BUFFER_OUTPUT_PERIOD_SIZE * DEEP_BUFFER_OUTPUT_PERIOD_COUNT
// DEEP BUFFER Stream use EAX Mixer
#define DEEP_BUFFER_OUTPUT_STOP_THREASHOLD  DEEP_BUFFER_OUTPUT_MAX_PERIOD_SIZE - EAX_STOP_PERIOD_SIZE


#define DEFAULT_INPUT_CHANNELS              2       // Stereo
#define DEFAULT_INPUT_SAMPLING_RATE         48000   // 48KHz

#define AUDIO_CAPTURE_PERIOD_SIZE           960     // 960 frames, 20ms in case of 48KHz Stream
#define AUDIO_CAPTURE_PERIOD_COUNT          2       // Total 7,680 Bytes = 960 * 2(Stereo) * 2(16bit PCM) * 2(Buffers)

#define DEFAULT_VOICE_CHANNELS              2       // Stereo
#define NB_VOICE_SAMPLING_RATE              8000    // 8KHz
#define WB_VOICE_SAMPLING_RATE              16000   // 16KHz

#define WB_VOICE_PERIOD_SIZE                160     //
#define WB_VOICE_PERIOD_COUNT               2       //

#define DEFAULT_BT_SCO_CHANNELS             1
#define BT_SCO_PERIOD_SIZE                  128
#define BT_SCO_PERIOD_COUNT                 2


/**
 ** These values are based on HW Decoder: Max Buffer Size = FRAGMENT_SIZE * NUM_FRAGMENTS
 ** 0 means that we will use the predefined value by device driver
 **/
#define COMPRESS_OFFLOAD_FRAGMENT_SIZE 0
#define COMPRESS_OFFLOAD_NUM_FRAGMENTS 0

#define COMPRESS_PLAYBACK_BUFFER_SIZE  (1024 * 4)  // min.fragment_size is fixed 4KBytes = 4 * 1024
#define COMPRESS_PLAYBACK_BUFFER_COUNT 5    // max.fragment is fixed 5

#define COMPRESS_PLAYBACK_VOLUME_MAX   8192

// Need to change to Exynos name, not Kiwi name
#define OFFLOAD_VOLUME_CONTROL_NAME "Compress Playback 3 Volume"

#define AUDIO_CAPTURE_PERIOD_DURATION_MSEC 20//Capture audio data during 20ms periods = 960 Samples for 48KHz


/*********************************************************************************/


#endif  // __EXYNOS_AUDIOCONF_H__
