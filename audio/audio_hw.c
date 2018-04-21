/*
 * Copyright (C) 2015 The Android Open Source Project
 * Copyright (C) 2018 Evgeniy Stenkin <stenkinevgeniy@gmail.com>
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


#define LOG_TAG "audio_hw_primary"
//#define LOG_NDEBUG 0
//#define VERY_VERY_VERBOSE_LOGGING
#ifdef VERY_VERY_VERBOSE_LOGGING
#define ALOGVV ALOGV
#else
#define ALOGVV(a...) do { } while(0)
#endif

#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <sys/time.h>
#include <stdlib.h>
#include <math.h>
#include <dlfcn.h>
#include <sys/resource.h>
#include <sys/prctl.h>

#include <cutils/log.h>
#include <cutils/str_parms.h>
#include <cutils/properties.h>
#include <cutils/atomic.h>
#include <cutils/sched_policy.h>

#include <hardware/audio_effect.h>
#include <system/thread_defs.h>
#include <audio_effects/effect_aec.h>
#include <audio_effects/effect_ns.h>
#include <audio_utils/channels.h>
#include "audio_hw.h"


struct pcm_config pcm_config_voice = {
    .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
    .rate = 16000,
    .period_size = 2048,
    .period_count = 6,
    .format = PCM_FORMAT_S16_LE,
};


struct pcm_config pcm_config_sco = {
    .channels = 1,
    .rate = 8000,
    .period_size = 128,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_sco_wide = {
    .channels = 1,
    .rate = 16000,
    .period_size = 128,
    .period_count = 2,
    .format = PCM_FORMAT_S16_LE,
};

struct pcm_config pcm_config_fm = {
    .channels = 2,
    .rate = 48000,
    .period_size = 256,
    .period_count = 4,
    .format = PCM_FORMAT_S16_LE,
};


struct pcm_config pcm_config_deep_buffer = {
    .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
    .rate = PLAYBACK_DEFAULT_SAMPLING_RATE,
    .period_size = DEEP_BUFFER_OUTPUT_PERIOD_SIZE,
    .period_count = DEEP_BUFFER_OUTPUT_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = DEEP_BUFFER_OUTPUT_PERIOD_SIZE / 4,
    .stop_threshold = INT_MAX,
    .avail_min = DEEP_BUFFER_OUTPUT_PERIOD_SIZE / 4,
};

struct pcm_config pcm_config_low_latency = {
    .channels = PLAYBACK_DEFAULT_CHANNEL_COUNT,
    .rate = PLAYBACK_DEFAULT_SAMPLING_RATE,
    .period_size = LOW_LATENCY_OUTPUT_PERIOD_SIZE,
    .period_count = LOW_LATENCY_OUTPUT_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = LOW_LATENCY_OUTPUT_PERIOD_SIZE / 4,
    .stop_threshold = INT_MAX,
    .avail_min = LOW_LATENCY_OUTPUT_PERIOD_SIZE / 4,
};


struct pcm_config pcm_config_audio_capture = {
    .channels = DEFAULT_CHANNEL_COUNT,
    .rate = DEFAULT_OUTPUT_SAMPLING_RATE,
    .period_size = AUDIO_CAPTURE_PERIOD_SIZE,
    .period_count = AUDIO_CAPTURE_PERIOD_COUNT,
    .format = PCM_FORMAT_S16_LE,
    .start_threshold = 0,
    .stop_threshold = INT_MAX,
    .silence_threshold = 0,
    .silence_size = 0,
    .avail_min = 0,
};

#define PCM_CARD 0

#define PCM_DEVICE 5       /* Playback link */
#define PCM_DEVICE_VOICE 2 /* Baseband link */
#define PCM_DEVICE_SCO 3   /* Bluetooth link */
#define PCM_DEVICE_DEEP 1  /* Deep buffer */
#define PCM_DEVICE_FM 4  /* FM link */
#define PCM_DEVICE_CAPTURE 0       /* Capture link */


static const int pcm_device_table[AUDIO_USECASE_MAX][2] = {
    [USECASE_AUDIO_PLAYBACK_DEEP_BUFFER] = {PCM_DEVICE_DEEP, PCM_DEVICE_DEEP},
    [USECASE_AUDIO_PLAYBACK_LOW_LATENCY] = {PCM_DEVICE, PCM_DEVICE},
    [USECASE_AUDIO_CAPTURE] = {PCM_DEVICE_CAPTURE, PCM_DEVICE_CAPTURE},
    [USECASE_AUDIO_CAPTURE_LOW_LATENCY] = {PCM_DEVICE_CAPTURE, PCM_DEVICE_CAPTURE},
    [USECASE_VOICE_CALL] = {PCM_DEVICE_VOICE, PCM_DEVICE_VOICE},
};


static const char * const use_case_table[AUDIO_USECASE_MAX] = {
    [USECASE_AUDIO_PLAYBACK_DEEP_BUFFER] = "deep-buffer-playback",
    [USECASE_AUDIO_PLAYBACK_LOW_LATENCY] = "low-latency-playback",
    [USECASE_AUDIO_PLAYBACK_OFFLOAD] = "compress-offload-playback",

    [USECASE_AUDIO_CAPTURE] = "audio-record",
    [USECASE_AUDIO_CAPTURE_LOW_LATENCY] = "low-latency-record",

    [USECASE_VOICE_CALL] = "voice-call",
};


#define STRING_TO_ENUM(string) { #string, string }

struct string_to_enum {
    const char *name;
    uint32_t value;
};

static const struct string_to_enum out_channels_name_to_enum_table[] = {
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_STEREO),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_5POINT1),
    STRING_TO_ENUM(AUDIO_CHANNEL_OUT_7POINT1),
};


static audio_usecase_t get_voice_usecase_id_from_list(struct audio_device *adev)
{
    struct audio_usecase *usecase;
    struct listnode *node;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->type == VOICE_CALL) {
            ALOGV("%s: usecase id %d", __func__, usecase->id);
            return usecase->id;
        }
    }
    return USECASE_INVALID;
}

int platform_get_pcm_device_id(audio_usecase_t usecase, int device_type)
{
    int device_id;
    if (device_type == PCM_PLAYBACK)
        device_id = pcm_device_table[usecase][0];
    else
        device_id = pcm_device_table[usecase][1];
    return device_id;
}


static bool is_supported_format(audio_format_t format)
{
    if (format == AUDIO_FORMAT_MP3 ||
            ((format & AUDIO_FORMAT_MAIN_MASK) == AUDIO_FORMAT_AAC))
        return true;

    return false;
}

static int get_snd_codec_id(audio_format_t format)
{
    int id = 0;

    switch (format & AUDIO_FORMAT_MAIN_MASK) {
    default:
        ALOGE("%s: Unsupported audio format", __func__);
    }

    return id;
}

/* Array to store sound devices */
static const char * const device_table[SND_DEVICE_MAX] = {
    [SND_DEVICE_NONE] = "none",
    /* Playback sound devices */
    [SND_DEVICE_OUT_EARPIECE] = "earpiece",
    [SND_DEVICE_OUT_SPEAKER] = "speaker",
    [SND_DEVICE_OUT_HEADPHONES] = "headphones",
    [SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES] = "speaker-and-headphones",
    [SND_DEVICE_OUT_VOICE_EARPIECE] = "voice-earpiece",
    [SND_DEVICE_OUT_VOICE_EARPIECE_WB] = "voice-earpiece-wb",
    [SND_DEVICE_OUT_VOICE_SPEAKER] = "voice-speaker",
    [SND_DEVICE_OUT_VOICE_SPEAKER_WB] = "voice-speaker-wb",
    [SND_DEVICE_OUT_VOICE_HEADPHONES] = "voice-headphones",
    [SND_DEVICE_OUT_VOICE_HEADPHONES_WB] = "voice-headphones-wb",
    [SND_DEVICE_OUT_VOICE_BT_SCO] = "voice-bt-sco-headset",
    [SND_DEVICE_OUT_VOICE_BT_SCO_WB] = "voice-bt-sco-headset-wb",
    [SND_DEVICE_OUT_HDMI] = "hdmi",
    [SND_DEVICE_OUT_SPEAKER_AND_HDMI] = "speaker-and-hdmi",
    [SND_DEVICE_OUT_BT_SCO] = "bt-sco-headset",

    /* Capture sound devices */
    [SND_DEVICE_IN_EARPIECE_MIC] = "earpiece-mic",
    [SND_DEVICE_IN_SPEAKER_MIC] = "speaker-mic",
    [SND_DEVICE_IN_HEADSET_MIC] = "headset-mic",
    [SND_DEVICE_IN_EARPIECE_MIC_AEC] = "earpiece-mic",
    [SND_DEVICE_IN_SPEAKER_MIC_AEC] = "voice-speaker-mic",
    [SND_DEVICE_IN_HEADSET_MIC_AEC] = "headset-mic",
    [SND_DEVICE_IN_VOICE_MIC] = "voice-mic",
    [SND_DEVICE_IN_VOICE_EARPIECE_MIC] = "voice-earpiece-mic",
    [SND_DEVICE_IN_VOICE_EARPIECE_MIC_WB] = "voice-earpiece-mic-wb",
    [SND_DEVICE_IN_VOICE_SPEAKER_MIC] = "voice-speaker-mic",
    [SND_DEVICE_IN_VOICE_SPEAKER_MIC_WB] = "voice-speaker-mic-wb",
    [SND_DEVICE_IN_VOICE_HEADSET_MIC] = "voice-headset-mic",
    [SND_DEVICE_IN_VOICE_HEADSET_MIC_WB] = "voice-headset-mic-wb",
    [SND_DEVICE_IN_VOICE_BT_SCO_MIC] = "voice-bt-sco-mic",
    [SND_DEVICE_IN_VOICE_BT_SCO_MIC_WB] = "voice-bt-sco-mic-wb",
    [SND_DEVICE_IN_HDMI_MIC] = "hdmi-mic",
    [SND_DEVICE_IN_BT_SCO_MIC] = "bt-sco-mic",
    [SND_DEVICE_IN_CAMCORDER_MIC] = "camcorder-mic",
    [SND_DEVICE_IN_VOICE_REC_HEADSET_MIC] = "voice-rec-headset-mic",
    [SND_DEVICE_IN_VOICE_REC_MIC] = "voice-rec-mic",
    [SND_DEVICE_IN_LOOPBACK_AEC] = "loopback-aec",
};


// Treblized config files will be located in /odm/etc or /vendor/etc.
static const char *kConfigLocationList[] =
        {"/vendor/etc", "/system/etc"};
static const int kConfigLocationListSize =
        (sizeof(kConfigLocationList) / sizeof(kConfigLocationList[0]));

bool resolveConfigFile(char file_name[MIXER_PATH_MAX_LENGTH]) {
    char full_config_path[MIXER_PATH_MAX_LENGTH];
    for (int i = 0; i < kConfigLocationListSize; i++) {
        snprintf(full_config_path,
                 MIXER_PATH_MAX_LENGTH,
                 "%s/%s",
                 kConfigLocationList[i],
                 file_name);
        if (F_OK == access(full_config_path, 0)) {
            strcpy(file_name, full_config_path);
            return true;
        }
    }
    return false;
}


int mixer_init(struct audio_device *adev)
{
    struct audio_route *audio_route;
    char mixer_path[PATH_MAX];
    char mixer_xml_file[MIXER_PATH_MAX_LENGTH] = MIXER_XML_PATH;


    adev->mixer = mixer_open(MIXER_CARD);

    if (!adev->mixer) {
        ALOGE("Unable to open the mixer, aborting.");
        return -1;
    }

    resolveConfigFile(mixer_xml_file);
    adev->audio_route = audio_route_init(MIXER_CARD, mixer_xml_file);

    if (!adev->audio_route) {
        ALOGE("%s: Failed to init audio route controls, aborting.", __func__);
        return -1;
    }

    return 0;
}

const char *get_snd_device_name(snd_device_t snd_device)
{
    const char *name = NULL;

    if (snd_device >= SND_DEVICE_MIN && snd_device < SND_DEVICE_MAX)
        return device_table[snd_device];
    else
        return "none";

}

const char *get_snd_device_display_name(snd_device_t snd_device)
{
    const char *name = get_snd_device_name(snd_device);

    if (name == NULL)
        name = "SND DEVICE NOT FOUND";

    return name;
}


//##########################


/**********************************************************
 * FM functions
 **********************************************************/

/* must be called with the hw device mutex locked, OK to hold other mutexes */


static void check_dualmic(struct audio_device *adev, struct audio_usecase *uc_info)
{

    switch (uc_info->devices) {
    case AUDIO_DEVICE_OUT_EARPIECE:
    case AUDIO_DEVICE_OUT_SPEAKER:
        adev->voice_two_mic = true;
        break;
    default:
        adev->voice_two_mic = false;
        break;
    }
}

static void start_fm(struct audio_device *adev)
{

    
    if (adev->pcm_fm_rx != NULL || adev->pcm_fm_tx != NULL) {
        ALOGW("%s: FM PCMs already open!\n", __func__);
        return;
    }

    adev->fm_mode=true;

    ALOGV("%s: Opening FM PCMs", __func__);

    adev->pcm_fm_tx = pcm_open(PCM_CARD,
                                PCM_DEVICE_FM,
                                PCM_OUT | PCM_MONOTONIC,
                                &pcm_config_fm);
    if (adev->pcm_fm_tx && !pcm_is_ready(adev->pcm_fm_tx)) {
        ALOGE("%s: cannot open PCM FM TX stream: %s",
              __func__, pcm_get_error(adev->pcm_fm_tx));
        goto err_fm_tx;
    }

    pcm_start(adev->pcm_fm_tx);

    return;

err_fm_tx:
    pcm_close(adev->pcm_fm_tx);
    adev->pcm_fm_tx = NULL;
}

/* must be called with the hw device mutex locked, OK to hold other mutexes */
static void stop_fm(struct audio_device *adev) {
    ALOGV("%s: Closing FM PCMs", __func__);

    if (adev->pcm_fm_tx != NULL) {
        pcm_stop(adev->pcm_fm_tx);
        pcm_close(adev->pcm_fm_tx);
        adev->pcm_fm_tx = NULL;
    }

    adev->fm_mode = false;
}

/**********************************************************
 * BT SCO functions
 **********************************************************/

/* must be called with the hw device mutex locked, OK to hold other mutexes */
static void start_bt_sco(struct audio_device *adev)
{
    struct pcm_config *sco_config;
    

    if (adev->pcm_sco_rx != NULL || adev->pcm_sco_tx != NULL) {
        ALOGW("%s: SCO PCMs already open!\n", __func__);
        return;
    }

    ALOGV("%s: Opening SCO PCMs", __func__);
    
    if (adev->bluetooth_wbs) {
        sco_config = &pcm_config_sco_wide;
    } else {
        sco_config = &pcm_config_sco;
    }

    adev->pcm_sco_rx = pcm_open(PCM_CARD,
                                PCM_DEVICE_SCO,
                                PCM_IN,
                                sco_config);
    if (adev->pcm_sco_rx != NULL && !pcm_is_ready(adev->pcm_sco_rx)) {
        ALOGE("%s: cannot open PCM SCO RX stream: %s",
              __func__, pcm_get_error(adev->pcm_sco_rx));
        goto err_sco_rx;
    }

    adev->pcm_sco_tx = pcm_open(PCM_CARD,
                                PCM_DEVICE_SCO,
                                PCM_OUT | PCM_MONOTONIC,
                                sco_config);
    if (adev->pcm_sco_tx && !pcm_is_ready(adev->pcm_sco_tx)) {
        ALOGE("%s: cannot open PCM SCO TX stream: %s",
              __func__, pcm_get_error(adev->pcm_sco_tx));
        goto err_sco_tx;
    }

    pcm_start(adev->pcm_sco_rx);
    pcm_start(adev->pcm_sco_tx);

    return;

err_sco_tx:
    pcm_close(adev->pcm_sco_tx);
    adev->pcm_sco_tx = NULL;
err_sco_rx:
    pcm_close(adev->pcm_sco_rx);
    adev->pcm_sco_rx = NULL;
}

static void stop_call(struct audio_device *adev)
{
    int status = 0;

    ALOGV("%s: Closing active PCMs", __func__);

    if (adev->pcm_voice_rx) {
        pcm_stop(adev->pcm_voice_rx);
        pcm_close(adev->pcm_voice_rx);
        adev->pcm_voice_rx = NULL;
        status++;
    }

    if (adev->pcm_voice_tx) {
        pcm_stop(adev->pcm_voice_tx);
        pcm_close(adev->pcm_voice_tx);
        adev->pcm_voice_tx = NULL;
        status++;
    }

    ALOGV("%s: Successfully closed %d active PCMs", __func__, status);
}

/* must be called with the hw device mutex locked, OK to hold other mutexes */
static void stop_bt_sco(struct audio_device *adev) {
    ALOGV("%s: Closing SCO PCMs", __func__);

    if (adev->pcm_sco_rx != NULL) {
        pcm_stop(adev->pcm_sco_rx);
        pcm_close(adev->pcm_sco_rx);
        adev->pcm_sco_rx = NULL;
    }

    if (adev->pcm_sco_tx != NULL) {
        pcm_stop(adev->pcm_sco_tx);
        pcm_close(adev->pcm_sco_tx);
        adev->pcm_sco_tx = NULL;
    }
    adev->sco_act = false;
}


static int start_call(struct audio_device *adev)
{
    struct pcm_config *voice_config;

    if (adev->pcm_voice_rx != NULL && adev->pcm_voice_tx != NULL) {
        ALOGW("%s: Voice PCMs already open!\n", __func__);
        return 0;
    }

    ALOGV("%s: Opening voice PCMs", __func__);

    voice_config = &pcm_config_voice;

    /* Open modem PCM channels */
    adev->pcm_voice_rx = pcm_open(PCM_CARD,
                                  PCM_DEVICE_VOICE,
                                  PCM_OUT | PCM_MONOTONIC,
                                  voice_config);
    if (adev->pcm_voice_rx != NULL && !pcm_is_ready(adev->pcm_voice_rx)) {
        ALOGE("%s: cannot open PCM voice RX stream: %s",
              __func__, pcm_get_error(adev->pcm_voice_rx));
        goto err_voice_rx;
    }

    adev->pcm_voice_tx = pcm_open(PCM_CARD,
                                  PCM_DEVICE_VOICE,
                                  PCM_IN,
                                  voice_config);
    if (adev->pcm_voice_tx != NULL && !pcm_is_ready(adev->pcm_voice_tx)) {
        ALOGE("%s: cannot open PCM voice TX stream: %s",
              __func__, pcm_get_error(adev->pcm_voice_tx));
        goto err_voice_tx;
    }

    pcm_start(adev->pcm_voice_rx);
    pcm_start(adev->pcm_voice_tx);

    return 0;

err_voice_tx:
    pcm_close(adev->pcm_voice_tx);
    adev->pcm_voice_tx = NULL;
err_voice_rx:
    pcm_close(adev->pcm_voice_rx);
    adev->pcm_voice_rx = NULL;

    return -ENOMEM;
}



static void adev_set_call_audio_path(struct audio_device *adev)
{
    enum _AudioPath device_type;
    switch(adev->current_call_output->devices) {
        case AUDIO_DEVICE_OUT_SPEAKER:
            device_type = SOUND_AUDIO_PATH_SPEAKER;
            break;
        case AUDIO_DEVICE_OUT_EARPIECE:
            device_type = SOUND_AUDIO_PATH_HANDSET;
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADSET:
            device_type = SOUND_AUDIO_PATH_HEADSET;
            break;
        case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
            device_type = SOUND_AUDIO_PATH_HEADPHONE;
            break;
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
        case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
            if (adev->bluetooth_wbs) {
                if (adev->bluetooth_nrec) device_type = SOUND_AUDIO_PATH_BLUETOOTH_WB;
                else
                   device_type = SOUND_AUDIO_PATH_BLUETOOTH_WB_NO_NR;
            }
            if (!adev->bluetooth_wbs) {
                if (adev->bluetooth_nrec) device_type = SOUND_AUDIO_PATH_BLUETOOTH;
                else
                     device_type = SOUND_AUDIO_PATH_BLUETOOTH;
            }
            break;
        default:
            /* if output device isn't supported, use handset by default */
            device_type = SOUND_AUDIO_PATH_HANDSET;
            break;
    }

    ALOGV("%s: ril_set_call_audio_path(%d)", __func__, device_type);

    ril_set_call_audio_path(&adev->ril, device_type);
}


//#########################


static struct audio_usecase *get_usecase_from_id(struct audio_device *adev,
                                                   audio_usecase_t uc_id)
{
    struct audio_usecase *usecase;
    struct listnode *node;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->id == uc_id)
            return usecase;
    }
    return NULL;
}

static struct audio_usecase *get_usecase_from_type(struct audio_device *adev,
                                                        usecase_type_t type)
{
    struct audio_usecase *usecase;
    struct listnode *node;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->type & type)
            return usecase;
    }
    return NULL;
}

/* always called with adev lock held */
static int set_voice_volume_l(struct audio_device *adev, float volume)
{
    int err = 0;


    adev->voice_volume = volume;

    if (adev->mode == AUDIO_MODE_IN_CALL) {
        enum _SoundType sound_type;

        switch (adev->current_call_output->devices) {
            case AUDIO_DEVICE_OUT_EARPIECE:
                sound_type = SOUND_TYPE_VOICE;
                break;
            case AUDIO_DEVICE_OUT_SPEAKER:
                sound_type = SOUND_TYPE_SPEAKER;
                break;
            case AUDIO_DEVICE_OUT_WIRED_HEADSET:
            case AUDIO_DEVICE_OUT_WIRED_HEADPHONE:
                sound_type = SOUND_TYPE_HEADSET;
                break;
            case AUDIO_DEVICE_OUT_BLUETOOTH_SCO:
            case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET:
            case AUDIO_DEVICE_OUT_BLUETOOTH_SCO_CARKIT:
            case AUDIO_DEVICE_OUT_ALL_SCO:
                sound_type = SOUND_TYPE_BTVOICE;
                break;
            default:
                sound_type = SOUND_TYPE_VOICE;
        }

        ril_set_call_volume(&adev->ril, sound_type, volume);

    }
    return err;
}


snd_device_t get_output_snd_device(struct audio_device *adev, audio_devices_t devices)
{

    audio_mode_t mode = adev->mode;
    snd_device_t snd_device = SND_DEVICE_NONE;

    ALOGV("%s: enter: output devices(%#x), mode(%d)", __func__, devices, mode);
    if (devices == AUDIO_DEVICE_NONE ||
        devices & AUDIO_DEVICE_BIT_IN) {
        ALOGV("%s: Invalid output devices (%#x)", __func__, devices);
        goto exit;
    }

    if (mode == AUDIO_MODE_IN_CALL) {
        if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
            devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                snd_device = SND_DEVICE_OUT_VOICE_HEADPHONES;
        } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_OUT_VOICE_SPEAKER;
        } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
            snd_device = SND_DEVICE_OUT_VOICE_EARPIECE;
        }
        if (snd_device != SND_DEVICE_NONE)
            goto exit;
    }

    if (popcount(devices) == 2) {
        if (devices == (AUDIO_DEVICE_OUT_WIRED_HEADPHONE |
                        AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES;
        } else if (devices == (AUDIO_DEVICE_OUT_WIRED_HEADSET |
                               AUDIO_DEVICE_OUT_SPEAKER)) {
            snd_device = SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES;
        } else {
            ALOGE("%s: Invalid combo device(%#x)", __func__, devices);
            goto exit;
        }
        if (snd_device != SND_DEVICE_NONE) {
            goto exit;
        }
    }

    if (popcount(devices) != 1) {
        ALOGE("%s: Invalid output devices(%#x)", __func__, devices);
        goto exit;
    }

    if (devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE ||
        devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
        snd_device = SND_DEVICE_OUT_HEADPHONES;
    } else if (devices & AUDIO_DEVICE_OUT_SPEAKER) {
        snd_device = SND_DEVICE_OUT_SPEAKER;
    } else if (devices & AUDIO_DEVICE_OUT_EARPIECE) {
        snd_device = SND_DEVICE_OUT_EARPIECE;
    } else {
        ALOGE("%s: Unknown device(s) %#x", __func__, devices);
    }
exit:
    ALOGV("%s: exit: snd_device(%s)", __func__, device_table[snd_device]);
    return snd_device;
}


static snd_device_t get_input_snd_device(struct audio_device *adev, audio_devices_t out_device)
{
    audio_source_t  source;
    audio_mode_t    mode   = adev->mode;
    audio_devices_t in_device;
    audio_channel_mask_t channel_mask;
    snd_device_t snd_device = SND_DEVICE_NONE;
    struct stream_in *active_input = NULL;
    struct audio_usecase *usecase;

    usecase = get_usecase_from_type(adev, PCM_CAPTURE|VOICE_CALL);
    if (usecase != NULL) {
        active_input = (struct stream_in *)usecase->stream;
    }
    source = (active_input == NULL) ?
                                AUDIO_SOURCE_DEFAULT : active_input->source;

    in_device = (active_input == NULL) ?
                    AUDIO_DEVICE_NONE :
                    (active_input->devices & ~AUDIO_DEVICE_BIT_IN);
    channel_mask = (active_input == NULL) ?
                                AUDIO_CHANNEL_IN_MONO : active_input->main_channels;

    ALOGV("%s: enter: out_device(%#x) in_device(%#x)",
          __func__, out_device, in_device);
    if (mode == AUDIO_MODE_IN_CALL) {
        if (out_device == AUDIO_DEVICE_NONE) {
            ALOGE("%s: No output device set for voice call", __func__);
            goto exit;
        }

        snd_device = SND_DEVICE_IN_VOICE_MIC;
        if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_VOICE_HEADSET_MIC;
        }

        if (adev->voice_two_mic) {
            if (out_device & AUDIO_DEVICE_OUT_EARPIECE ||
                out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                snd_device = SND_DEVICE_IN_VOICE_EARPIECE_MIC;
            } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
                snd_device = SND_DEVICE_IN_VOICE_SPEAKER_MIC;
            }
        }

        if (adev->voice_wb) {
            if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
                snd_device = SND_DEVICE_IN_VOICE_HEADSET_MIC_WB;
            }

            if (adev->voice_two_mic) {
                if (out_device & AUDIO_DEVICE_OUT_EARPIECE ||
                    out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                    snd_device = SND_DEVICE_IN_VOICE_EARPIECE_MIC_WB;
                } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
                    snd_device = SND_DEVICE_IN_VOICE_SPEAKER_MIC_WB;
                }
            }
        }

        /* BT SCO */
        if (out_device & AUDIO_DEVICE_OUT_ALL_SCO) {
            snd_device = SND_DEVICE_IN_VOICE_MIC;

            if (out_device & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) {
                if (adev->voice_wb) {
                    snd_device = SND_DEVICE_IN_VOICE_BT_SCO_MIC_WB;
                } else {
                    snd_device = SND_DEVICE_IN_VOICE_BT_SCO_MIC;
                }
            } else if (adev->voice_two_mic) {
                snd_device = SND_DEVICE_IN_VOICE_EARPIECE_MIC;
            }
        }
    } else if (source == AUDIO_SOURCE_CAMCORDER) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC ||
            in_device & AUDIO_DEVICE_IN_BACK_MIC) {
            snd_device = SND_DEVICE_IN_CAMCORDER_MIC;
        }
    } else if (source == AUDIO_SOURCE_VOICE_RECOGNITION) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
            if (snd_device == SND_DEVICE_NONE) {
                snd_device = SND_DEVICE_IN_VOICE_REC_MIC;
            }
        } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_VOICE_REC_HEADSET_MIC;
        }
    } else if (source == AUDIO_SOURCE_VOICE_COMMUNICATION || source == AUDIO_SOURCE_MIC) {
        if (out_device & AUDIO_DEVICE_OUT_SPEAKER)
            in_device = AUDIO_DEVICE_IN_BACK_MIC;
        if (active_input) {
            if (active_input->enable_aec) {
                if (in_device & AUDIO_DEVICE_IN_BACK_MIC) {
                    snd_device = SND_DEVICE_IN_SPEAKER_MIC_AEC;
                } else if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
                    if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
                        snd_device = SND_DEVICE_IN_SPEAKER_MIC_AEC;
                    } else {
                        snd_device = SND_DEVICE_IN_EARPIECE_MIC_AEC;
                    }
                } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
                    snd_device = SND_DEVICE_IN_HEADSET_MIC_AEC;
                }
            }
            /* TODO: set echo reference */
        }
    } else if (source == AUDIO_SOURCE_DEFAULT) {
        goto exit;
    }


    if (snd_device != SND_DEVICE_NONE) {
        goto exit;
    }

    if (in_device != AUDIO_DEVICE_NONE &&
            !(in_device & AUDIO_DEVICE_IN_VOICE_CALL) &&
            !(in_device & AUDIO_DEVICE_IN_COMMUNICATION)) {
        if (in_device & AUDIO_DEVICE_IN_BUILTIN_MIC) {
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BACK_MIC) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (in_device & AUDIO_DEVICE_IN_BLUETOOTH_SCO_HEADSET) {
            snd_device = SND_DEVICE_IN_BT_SCO_MIC ;
        } else if (in_device & AUDIO_DEVICE_IN_AUX_DIGITAL) {
            snd_device = SND_DEVICE_IN_HDMI_MIC;
        } else {
            ALOGE("%s: Unknown input device(s) %#x", __func__, in_device);
            ALOGW("%s: Using default earpiece-mic", __func__);
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        }
    } else {
        if (out_device & AUDIO_DEVICE_OUT_EARPIECE) {
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADSET) {
            snd_device = SND_DEVICE_IN_HEADSET_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_SPEAKER) {
            snd_device = SND_DEVICE_IN_SPEAKER_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) {
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        } else if (out_device & AUDIO_DEVICE_OUT_BLUETOOTH_SCO_HEADSET) {
            snd_device = SND_DEVICE_IN_BT_SCO_MIC;
        } else {
            ALOGE("%s: Unknown output device(s) %#x", __func__, out_device);
            ALOGW("%s: Using default earpiece-mic", __func__);
            snd_device = SND_DEVICE_IN_EARPIECE_MIC;
        }
    }
exit:
    ALOGV("%s: exit: in_snd_device(%s)", __func__, device_table[snd_device]);
    return snd_device;
}

int set_hdmi_channels(struct audio_device *adev,  int channel_count)
{
    struct mixer_ctl *ctl;
    const char *mixer_ctl_name = "";
    (void)adev;
    (void)channel_count;
    /* TODO */

    return 0;
}

int edid_get_max_channels(struct audio_device *adev)
{
    int max_channels = 2;
    struct mixer_ctl *ctl;
    (void)adev;

    /* TODO */
    return max_channels;
}

/* Delay in Us */
int64_t render_latency(audio_usecase_t usecase)
{
    (void)usecase;
    /* TODO */
    return 0;
}

static int enable_snd_device(struct audio_device *adev,
                             snd_device_t snd_device)
{
    const char *snd_device_name = get_snd_device_name(snd_device);

    if (snd_device_name == NULL)
        return -EINVAL;


    if (snd_device == SND_DEVICE_OUT_SPEAKER_AND_HEADPHONES) {
        ALOGV("Request to enable combo device: enable individual devices\n");
        enable_snd_device(adev, SND_DEVICE_OUT_SPEAKER);
        enable_snd_device(adev, SND_DEVICE_OUT_HEADPHONES);
        return 0;
    }


    adev->snd_dev_ref_cnt[snd_device]++;

    if (adev->snd_dev_ref_cnt[snd_device] > 1) {
        ALOGV("%s: snd_device(%d: %s) is already active",
              __func__, snd_device, snd_device_name);
        return 0;
    }

    ALOGV("%s: snd_device(%d: %s)", __func__, snd_device, snd_device_name);
    audio_route_apply_and_update_path(adev->audio_route, snd_device_name);

    return 0;
}

static int disable_snd_device(struct audio_device *adev,
                              snd_device_t snd_device)
{
    const char *snd_device_name = get_snd_device_name(snd_device);

    if (snd_device_name == NULL)
        return -EINVAL;

    if (adev->snd_dev_ref_cnt[snd_device] <= 0) {
        ALOGE("%s: device ref cnt is already 0", __func__);
        return -EINVAL;
    }
    adev->snd_dev_ref_cnt[snd_device]--;
    if (adev->snd_dev_ref_cnt[snd_device] == 0) {

        ALOGV("%s: snd_device(%d: %s)", __func__,
              snd_device, snd_device_name);
            audio_route_reset_and_update_path(adev->audio_route, snd_device_name);

    }

    return 0;
}

static void check_and_route_playback_usecases(struct audio_device *adev,
                                              struct audio_usecase *uc_info,
                                              snd_device_t snd_device)
{
    struct listnode *node;
    struct audio_usecase *usecase;
    bool switch_device[AUDIO_USECASE_MAX];
    int i, num_uc_to_switch = 0;

    /*
     * This function is to make sure that all the usecases that are active on
     * the hardware codec backend are always routed to any one device that is
     * handled by the hardware codec.
     * For example, if low-latency and deep-buffer usecases are currently active
     * on speaker and out_set_parameters(headset) is received on low-latency
     * output, then we have to make sure deep-buffer is also switched to headset,
     * because of the limitation that both the devices cannot be enabled
     * at the same time as they share the same backend.
     */
    /* Disable all the usecases on the shared backend other than the
       specified usecase */
    for (i = 0; i < AUDIO_USECASE_MAX; i++)
        switch_device[i] = false;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->type == PCM_CAPTURE || usecase == uc_info)
            continue;
        if (usecase->out_snd_device != snd_device) {
            ALOGV("%s: Usecase (%s) is active on (%s) - disabling ..",
                  __func__, use_case_table[usecase->id],
                  get_snd_device_name(usecase->out_snd_device));
            switch_device[usecase->id] = true;
            num_uc_to_switch++;
        }
    }
    if (num_uc_to_switch) {
        list_for_each(node, &adev->usecase_list) {
            usecase = node_to_item(node, struct audio_usecase, adev_list_node);
            if (switch_device[usecase->id]) {
		disable_snd_device(adev, usecase->out_snd_device);
            }
        }
        list_for_each(node, &adev->usecase_list) {
            usecase = node_to_item(node, struct audio_usecase, adev_list_node);
            if (switch_device[usecase->id]) {
                enable_snd_device(adev, snd_device);
                /* Update the out_snd_device before enabling the audio route */
                usecase->out_snd_device = snd_device;
            }
        }
    }
}


static void check_and_route_capture_usecases(struct audio_device *adev,
                                             struct audio_usecase *uc_info,
                                             snd_device_t snd_device)
{
    struct listnode *node;
    struct audio_usecase *usecase;
    bool switch_device[AUDIO_USECASE_MAX];
    int i, num_uc_to_switch = 0;

    /*
     * This function is to make sure that all the active capture usecases
     * are always routed to the same input sound device.
     * For example, if audio-record and voice-call usecases are currently
     * active on speaker(rx) and speaker-mic (tx) and out_set_parameters(earpiece)
     * is received for voice call then we have to make sure that audio-record
     * usecase is also switched to earpiece i.e. voice-dmic-ef,
     * because of the limitation that two devices cannot be enabled
     * at the same time if they share the same backend.
     */
    for (i = 0; i < AUDIO_USECASE_MAX; i++)
        switch_device[i] = false;
    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->type != PCM_PLAYBACK &&
                usecase != uc_info &&
                usecase->in_snd_device != snd_device) {
            ALOGV("%s: Usecase (%s) is active on (%s) - disabling ..",
                  __func__, use_case_table[usecase->id],
                  get_snd_device_name(usecase->in_snd_device));
            switch_device[usecase->id] = true;
            num_uc_to_switch++;
        }
    }
    if (num_uc_to_switch) {
        list_for_each(node, &adev->usecase_list) {
            usecase = node_to_item(node, struct audio_usecase, adev_list_node);
            if (switch_device[usecase->id]) {
                disable_snd_device(adev, usecase->in_snd_device);
            }
        }
        list_for_each(node, &adev->usecase_list) {
            usecase = node_to_item(node, struct audio_usecase, adev_list_node);
            if (switch_device[usecase->id]) {
                enable_snd_device(adev, snd_device);
            }
        }
        /* Re-route all the usecases on the shared backend other than the
           specified usecase to new snd devices */
        list_for_each(node, &adev->usecase_list) {
            usecase = node_to_item(node, struct audio_usecase, adev_list_node);
            /* Update the in_snd_device only before enabling the audio route */
            if (switch_device[usecase->id] ) {
                usecase->in_snd_device = snd_device;
            }
        }
    }
}


static int select_devices(struct audio_device *adev,
                          audio_usecase_t uc_id)
{
    snd_device_t out_snd_device = SND_DEVICE_NONE;
    snd_device_t in_snd_device = SND_DEVICE_NONE;
    struct audio_usecase *usecase = NULL;
    struct audio_usecase *vc_usecase = NULL;
    struct listnode *node;
    struct stream_in *active_input = NULL;
    struct stream_out *active_out;
    const char *snd_device_name = "none";

    ALOGV("%s: usecase(%d)", __func__, uc_id);

    usecase = get_usecase_from_type(adev, PCM_CAPTURE|VOICE_CALL);
    if (usecase != NULL) {
        active_input = (struct stream_in *)usecase->stream;
    }

    usecase = get_usecase_from_id(adev, uc_id);
    if (usecase == NULL) {
        ALOGE("%s: Could not find the usecase(%d)", __func__, uc_id);
        return -EINVAL;
    }
    active_out = (struct stream_out *)usecase->stream;

    if (usecase->type == VOICE_CALL) {
        out_snd_device = get_output_snd_device(adev, active_out->devices);
        in_snd_device = get_input_snd_device(adev, active_out->devices);
        usecase->devices = active_out->devices;
    } else {
        /*
         * If the voice call is active, use the sound devices of voice call usecase
         * so that it would not result any device switch. All the usecases will
         * be switched to new device when select_devices() is called for voice call
         * usecase.
         */
        if (adev->in_call) {
            vc_usecase = get_usecase_from_id(adev, get_voice_usecase_id_from_list(adev));
            if (usecase == NULL) {
                ALOGE("%s: Could not find the voice call usecase", __func__);
            } else {
                in_snd_device = vc_usecase->in_snd_device;
                out_snd_device = vc_usecase->out_snd_device;
            }
        }
        if (usecase->type == PCM_PLAYBACK) {
            usecase->devices = active_out->devices;
            in_snd_device = SND_DEVICE_NONE;
            if (out_snd_device == SND_DEVICE_NONE) {
                out_snd_device = get_output_snd_device(adev, active_out->devices);
                if (active_out == adev->primary_output &&
                        active_input &&
                        active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION) {
                    select_devices(adev, active_input->usecase);
                }
            }
        } else if (usecase->type == PCM_CAPTURE) {
            usecase->devices = ((struct stream_in *)usecase->stream)->devices;
            out_snd_device = SND_DEVICE_NONE;
            if (in_snd_device == SND_DEVICE_NONE) {
                if (active_input->source == AUDIO_SOURCE_VOICE_COMMUNICATION &&
                        adev->primary_output && !adev->primary_output->standby) {
                    in_snd_device = get_input_snd_device(adev, adev->primary_output->devices);
                } else {
                    in_snd_device = get_input_snd_device(adev, AUDIO_DEVICE_NONE);
                }
            }
        }
    }

    if (out_snd_device == usecase->out_snd_device &&
        in_snd_device == usecase->in_snd_device) {
        return 0;
    }

    ALOGV("%s: out_snd_device(%d: %s) in_snd_device(%d: %s)", __func__,
          out_snd_device, get_snd_device_display_name(out_snd_device),
          in_snd_device,  get_snd_device_display_name(in_snd_device));

    /* Disable current sound devices */

    if (usecase->out_snd_device != SND_DEVICE_NONE) {
        disable_snd_device(adev, usecase->out_snd_device);
    }

    if (usecase->in_snd_device != SND_DEVICE_NONE) {
        disable_snd_device(adev, usecase->in_snd_device);
    }

    /* Enable new sound devices */

    if (out_snd_device != SND_DEVICE_NONE) {
	check_and_route_playback_usecases(adev, usecase, out_snd_device);
        enable_snd_device(adev, out_snd_device);
    }


    if (in_snd_device != SND_DEVICE_NONE) {
	check_and_route_capture_usecases(adev, usecase, in_snd_device);
	enable_snd_device(adev, in_snd_device);
    }

    usecase->in_snd_device = in_snd_device;
    usecase->out_snd_device = out_snd_device;

    return 0;
}


void voice_update_devices_for_all_voice_usecases(struct audio_device *adev)
{
    struct listnode *node;
    struct audio_usecase *usecase;

    list_for_each(node, &adev->usecase_list) {
        usecase = node_to_item(node, struct audio_usecase, adev_list_node);
        if (usecase->type == VOICE_CALL) {
            ALOGV("%s: updating device for usecase:%s", __func__,
                  use_case_table[usecase->id]);
            usecase->stream = (struct audio_stream *)adev->current_call_output;
            select_devices(adev, usecase->id);
        }
    }
}


static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames);
static int do_in_standby_l(struct stream_in *in);
static audio_format_t in_get_format(const struct audio_stream *stream);

/* This function reads PCM data and:
 * - resample if needed
 * - process if pre-processors are attached
 * - discard unwanted channels
 */
static ssize_t read_and_process_frames(struct audio_stream_in *stream, void* buffer, ssize_t frames_num)
{
    struct stream_in *in = (struct stream_in *)stream;
    ssize_t frames_wr = 0; /* Number of frames actually read */
    size_t bytes_per_sample = audio_bytes_per_sample(stream->common.get_format(&stream->common));
    void *proc_buf_out = buffer;

    /* Additional channels might be added on top of main_channels:
    * - aux_channels (by processing effects)
    * - extra channels due to HW limitations
    * In case of additional channels, we cannot work inplace
    */
    size_t src_channels = in->config.channels;
    size_t dst_channels = audio_channel_count_from_in_mask(in->main_channels);
    bool channel_remapping_needed = (dst_channels != src_channels);
    const size_t src_frame_size = src_channels * bytes_per_sample;

    const bool has_processing = false;

    /* With additional channels or processing, we need intermediate buffers */
    if (channel_remapping_needed || has_processing) {
        const size_t src_buffer_size = frames_num * src_frame_size;

        if (in->proc_buf_size < src_buffer_size) {
            in->proc_buf_size = src_buffer_size;
            in->proc_buf_out = realloc(in->proc_buf_out, src_buffer_size);
            ALOG_ASSERT((in->proc_buf_out != NULL),
                    "process_frames() failed to reallocate proc_buf_out");
        }
        if (channel_remapping_needed) {
            proc_buf_out = in->proc_buf_out;
        }
    }

    {
        /* No processing effects attached */
        frames_wr = read_frames(in, proc_buf_out, frames_num);
        ALOG_ASSERT(frames_wr <= frames_num, "read more frames than requested");
    }

    /* check negative frames_wr (error) before channel remapping to avoid overwriting memory. */
    if (channel_remapping_needed && frames_wr > 0) {
        size_t ret = adjust_channels(proc_buf_out, src_channels, buffer, dst_channels,
            bytes_per_sample, frames_wr * src_frame_size);
        ALOG_ASSERT(ret == (frames_wr * dst_channels * bytes_per_sample));
    }

    return frames_wr;
}

static int get_next_buffer(struct resampler_buffer_provider *buffer_provider,
                                   struct resampler_buffer* buffer)
{
    struct stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return -EINVAL;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    if (!in->pcm) {
        buffer->raw = NULL;
        buffer->frame_count = 0;
        in->read_status = -ENODEV;
        return -ENODEV;
    }

    if (in->read_buf_frames == 0) {
        size_t size_in_bytes = pcm_frames_to_bytes(in->pcm, in->config.period_size);
        if (in->read_buf_size < in->config.period_size) {
            in->read_buf_size = in->config.period_size;
            in->read_buf = (int16_t *) realloc(in->read_buf, size_in_bytes);
            ALOG_ASSERT((in->read_buf != NULL),
                        "get_next_buffer() failed to reallocate read_buf");
        }

        in->read_status = pcm_read(in->pcm, (void*)in->read_buf, size_in_bytes);

        if (in->read_status != 0) {
            ALOGE("get_next_buffer() pcm_read error %d", in->read_status);
            buffer->raw = NULL;
            buffer->frame_count = 0;
            return in->read_status;
        }
        in->read_buf_frames = in->config.period_size;
    }

    buffer->frame_count = (buffer->frame_count > in->read_buf_frames) ?
                                in->read_buf_frames : buffer->frame_count;
    buffer->i16 = in->read_buf + (in->config.period_size - in->read_buf_frames) *
                                                in->config.channels;
    return in->read_status;
}

static void release_buffer(struct resampler_buffer_provider *buffer_provider,
                                  struct resampler_buffer* buffer)
{
    struct stream_in *in;

    if (buffer_provider == NULL || buffer == NULL)
        return;

    in = (struct stream_in *)((char *)buffer_provider -
                                   offsetof(struct stream_in, buf_provider));

    in->read_buf_frames -= buffer->frame_count;
}

/* read_frames() reads frames from kernel driver, down samples to capture rate
 * if necessary and output the number of frames requested to the buffer specified */
static ssize_t read_frames(struct stream_in *in, void *buffer, ssize_t frames)
{
    ssize_t frames_wr = 0;

    while (frames_wr < frames) {
        size_t frames_rd = frames - frames_wr;
        ALOGVV("%s: frames_rd: %zd, frames_wr: %zd, in->config.channels: %d",
               __func__,frames_rd,frames_wr,in->config.channels);
        if (in->resampler != NULL) {
            in->resampler->resample_from_provider(in->resampler,
                    (int16_t *)((char *)buffer +
                            pcm_frames_to_bytes(in->pcm, frames_wr)),
                    &frames_rd);
        } else {
            struct resampler_buffer buf = {
                    { raw : NULL, },
                    frame_count : frames_rd,
            };
            get_next_buffer(&in->buf_provider, &buf);
            if (buf.raw != NULL) {
                memcpy((char *)buffer +
                            pcm_frames_to_bytes(in->pcm, frames_wr),
                        buf.raw,
                        pcm_frames_to_bytes(in->pcm, buf.frame_count));
                frames_rd = buf.frame_count;
            }
            release_buffer(&in->buf_provider, &buf);
        }
        /* in->read_status is updated by getNextBuffer() also called by
         * in->resampler->resample_from_provider() */
        if (in->read_status != 0)
            return in->read_status;

        frames_wr += frames_rd;
    }
    return frames_wr;
}

static int stop_input_stream(struct stream_in *in)
{
    struct audio_usecase *uc_info;
    struct audio_device *adev = in->dev;

    adev->active_input = NULL;
    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          in->usecase, use_case_table[in->usecase]);
    uc_info = get_usecase_from_id(adev, in->usecase);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
              __func__, in->usecase);
        return -EINVAL;
    }

    /* Disable the tx device */
    disable_snd_device(adev, uc_info->in_snd_device);

    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    return 0;
}

int start_input_stream(struct stream_in *in)
{
    /* Enable output device and stream routing controls */
    int ret = 0;
    bool recreate_resampler = false;
    struct audio_usecase *uc_info;
    struct audio_device *adev = in->dev;
    unsigned int flags = PCM_IN | PCM_MONOTONIC;
    unsigned int pcm_open_retry_count = 0;



    ALOGV("%s: enter: usecase(%d)", __func__, in->usecase);
    adev->active_input = in;

    in->pcm_device_id = platform_get_pcm_device_id(in->usecase, PCM_CAPTURE);
    if (in->pcm_device_id < 0) {
        ALOGE("%s: Could not find PCM device id for the usecase(%d)",
              __func__, in->usecase);
        ret = -EINVAL;
        goto error_config;
    }


    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    uc_info->id = in->usecase;
    uc_info->type = PCM_CAPTURE;
    uc_info->stream = (struct audio_stream *)in;
    uc_info->devices = in->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);

    select_devices(adev, in->usecase);

    /* Config should be updated as profile can be changed between different calls
     * to this function:
     * - Trigger resampler creation
     * - Config needs to be updated */


    if (in->config.rate != pcm_config_audio_capture.rate) {
        recreate_resampler = true;
    }
    in->config = pcm_config_audio_capture;


    if (in->requested_rate != in->config.rate) {
        recreate_resampler = true;
    }

    if (recreate_resampler) {
        if (in->resampler) {
            release_resampler(in->resampler);
            in->resampler = NULL;
        }
        in->buf_provider.get_next_buffer = get_next_buffer;
        in->buf_provider.release_buffer = release_buffer;
        ret = create_resampler(in->config.rate,
                               in->requested_rate,
                               in->config.channels,
                               RESAMPLER_QUALITY_DEFAULT,
                               &in->buf_provider,
                               &in->resampler);
    }



    /* Open the PCM device.
     * The HW is limited to support only the default pcm_profile settings.
     * As such a change in aux_channels will not have an effect.
     */

        while (1) {
            in->pcm = pcm_open(SOUND_CARD, in->pcm_device_id,
                               flags, &in->config);
            if (in->pcm == NULL || !pcm_is_ready(in->pcm)) {
                ALOGE("%s: %s", __func__, pcm_get_error(in->pcm));
                if (in->pcm != NULL) {
                    pcm_close(in->pcm);
                    in->pcm = NULL;
                }
                if (pcm_open_retry_count-- == 0) {
                    ret = -EIO;
                    goto error_open;
                }
                continue;
            }
            break;
        }


skip_pcm_handling:
    /* force read and proc buffer reallocation in case of frame size or
     * channel count change */
    in->proc_buf_size = 0;
    in->read_buf_size = 0;
    in->read_buf_frames = 0;

    /* if no supported sample rate is available, use the resampler */
    if (in->resampler) {
        in->resampler->reset(in->resampler);
    }

    ALOGV("%s: exit", __func__);
    return ret;

error_open:
    if (in->resampler) {
        release_resampler(in->resampler);
        in->resampler = NULL;
    }
    stop_input_stream(in);

error_config:
    ALOGV("%s: exit: status(%d)", __func__, ret);
    adev->active_input = NULL;
    return ret;
}

static void lock_input_stream(struct stream_in *in)
{
    pthread_mutex_lock(&in->pre_lock);
    pthread_mutex_lock(&in->lock);
    pthread_mutex_unlock(&in->pre_lock);
}

static void lock_output_stream(struct stream_out *out)
{
    pthread_mutex_lock(&out->pre_lock);
    pthread_mutex_lock(&out->lock);
    pthread_mutex_unlock(&out->pre_lock);
}


static int out_close_pcm_devices(struct stream_out *out)
{
    struct listnode *node;
    struct audio_device *adev = out->dev;

    if (out->pcm) {
       pcm_close(out->pcm);
       out->pcm = NULL;
    }

    return 0;
}

static int out_open_pcm_devices(struct stream_out *out)
{
    struct listnode *node;
    struct audio_device *adev = out->dev;
    int ret = 0;
    unsigned int flags = PCM_OUT | PCM_MONOTONIC;
    unsigned int pcm_open_retry_count = 0;


    while (1) {
            out->pcm = pcm_open(SOUND_CARD, out->pcm_device_id,
                               flags, &out->config);
            if (out->pcm == NULL || !pcm_is_ready(out->pcm)) {
                ALOGE("%s: %s", __func__, pcm_get_error(out->pcm));
                if (out->pcm != NULL) {
                    pcm_close(out->pcm);
                    out->pcm = NULL;
                }
                if (pcm_open_retry_count-- == 0) {
                    ret = -EIO;
                    goto error_open;
                }
                continue;
            }
            break;
    }

    return ret;

error_open:
    out_close_pcm_devices(out);
    return ret;
}

static int disable_output_path_l(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    struct audio_usecase *uc_info;

    uc_info = get_usecase_from_id(adev, out->usecase);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
             __func__, out->usecase);
        return -EINVAL;
    }


    /* 2. Disable the rx device */
    disable_snd_device(adev, uc_info->out_snd_device);

    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    return 0;
}

static void enable_output_path_l(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    struct audio_usecase *uc_info;

    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    uc_info->id = out->usecase;
    uc_info->type = PCM_PLAYBACK;
    uc_info->stream = (struct audio_stream *)out;
    uc_info->devices = out->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);

    select_devices(adev, out->usecase);
}

static int stop_output_stream(struct stream_out *out)
{
    int ret = 0;
    struct audio_device *adev = out->dev;
    bool do_disable = true;

    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          out->usecase, use_case_table[out->usecase]);

    ret = disable_output_path_l(out);

    ALOGV("%s: exit: status(%d)", __func__, ret);
    return ret;
}

int start_output_stream(struct stream_out *out)
{
    int ret = 0;
    struct audio_device *adev = out->dev;

    ALOGV("%s: enter: usecase(%d: %s) devices(%#x) channels(%d)",
          __func__, out->usecase, use_case_table[out->usecase], out->devices, out->config.channels);

    out->pcm_device_id = platform_get_pcm_device_id(out->usecase, PCM_PLAYBACK);
    if (out->pcm_device_id < 0) {
        ALOGE("%s: Invalid PCM device id(%d) for the usecase(%d)",
              __func__, out->pcm_device_id, out->usecase);
        ret = -EINVAL;
        goto error_config;
    }

    enable_output_path_l(out);

    ret = out_open_pcm_devices(out);
    if (ret != 0)
        goto error_open;
    ALOGV("%s: exit", __func__);
    return 0;
error_open:
    stop_output_stream(out);
error_config:
    return ret;
}

static int stop_voice_call(struct audio_device *adev)
{
    struct audio_usecase *uc_info;

    ALOGV("%s: enter", __func__);
    adev->in_call = false;

    /* TODO: implement voice call stop */

    stop_call(adev);


    uc_info = get_usecase_from_id(adev, USECASE_VOICE_CALL);
    if (uc_info == NULL) {
        ALOGE("%s: Could not find the usecase (%d) in the list",
              __func__, USECASE_VOICE_CALL);
        return -EINVAL;
    }

    disable_snd_device(adev, uc_info->out_snd_device);
    disable_snd_device(adev, uc_info->in_snd_device);

    list_remove(&uc_info->adev_list_node);
    free(uc_info);

    ril_set_call_clock_sync(&adev->ril, SOUND_CLOCK_STOP);


    ALOGV("%s: exit", __func__);
    return 0;
}

/* always called with adev lock held */
static int start_voice_call(struct audio_device *adev)
{
    struct audio_usecase *uc_info;

    ALOGV("%s: enter", __func__);

    adev->in_call = true;

    uc_info = (struct audio_usecase *)calloc(1, sizeof(struct audio_usecase));
    uc_info->id = USECASE_VOICE_CALL;
    uc_info->type = VOICE_CALL;
    uc_info->stream = (struct audio_stream *)adev->current_call_output;
    uc_info->devices = adev->current_call_output->devices;
    uc_info->in_snd_device = SND_DEVICE_NONE;
    uc_info->out_snd_device = SND_DEVICE_NONE;

    list_add_tail(&adev->usecase_list, &uc_info->adev_list_node);

    check_dualmic(adev, uc_info);

    select_devices(adev, USECASE_VOICE_CALL);

    /* TODO: implement voice call start */

    start_call(adev);

    ril_set_two_mic_control(&adev->ril, AUDIENCE, TWO_MIC_SOLUTION_ON);

    adev_set_call_audio_path(adev);

    /* set cached volume */
    set_voice_volume_l(adev, adev->voice_volume);

    ril_set_call_clock_sync(&adev->ril, SOUND_CLOCK_START);

    ALOGV("%s: exit", __func__);
    return 0;
}

static int check_input_parameters(uint32_t sample_rate,
                                  audio_format_t format,
                                  int channel_count)
{
    if (format != AUDIO_FORMAT_PCM_16_BIT) return -EINVAL;

    if ((channel_count < 1) || (channel_count > 4)) return -EINVAL;

    switch (sample_rate) {
    case 8000:
    case 11025:
    case 12000:
    case 16000:
    case 22050:
    case 24000:
    case 32000:
    case 44100:
    case 48000:
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

static size_t get_input_buffer_size(uint32_t sample_rate,
                                    audio_format_t format,
                                    int channel_count,
                                    usecase_type_t usecase_type,
                                    audio_devices_t devices)
{
    size_t size = 0;

    if (check_input_parameters(sample_rate, format, channel_count) != 0)
        return 0;

    size = (sample_rate * 20) / 1000;
    size *= channel_count * audio_bytes_per_sample(format);

    /* make sure the size is multiple of 32 bytes
     * At 48 kHz mono 16-bit PCM:
     *  5.000 ms = 240 frames = 15*16*1*2 = 480, a whole multiple of 32 (15)
     *  3.333 ms = 160 frames = 10*16*1*2 = 320, a whole multiple of 32 (10)
     */
    size += 0x1f;
    size &= ~0x1f;

    return size;
}

static uint32_t out_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->sample_rate;
}

static int out_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    (void)stream;
    (void)rate;
    return -ENOSYS;
}

static size_t out_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->config.period_size *
               audio_stream_out_frame_size((const struct audio_stream_out *)stream);
}

static uint32_t out_get_channels(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->channel_mask;
}

static audio_format_t out_get_format(const struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return out->format;
}

static int out_set_format(struct audio_stream *stream, audio_format_t format)
{
    (void)stream;
    (void)format;
    return -ENOSYS;
}

static int do_out_standby_l(struct stream_out *out)
{
    struct audio_device *adev = out->dev;
    int status = 0;

    out->standby = true;
    out_close_pcm_devices(out);
    status = stop_output_stream(out);

    return status;
}

static int out_standby(struct audio_stream *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;

    ALOGV("%s: enter: usecase(%d: %s)", __func__,
          out->usecase, use_case_table[out->usecase]);
    lock_output_stream(out);
    if (!out->standby) {
        pthread_mutex_lock(&adev->lock);
        do_out_standby_l(out);
        pthread_mutex_unlock(&adev->lock);
    }
    pthread_mutex_unlock(&out->lock);
    ALOGV("%s: exit", __func__);
    return 0;
}

static int out_dump(const struct audio_stream *stream, int fd)
{
    (void)stream;
    (void)fd;

    return 0;
}

static int out_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    struct audio_usecase *usecase;
    struct listnode *node;
    struct str_parms *parms;
    char value[32];
    int ret, val = 0;
    bool devices_changed;

    ALOGV("%s: enter: usecase(%d: %s) kvpairs: %s out->devices(%d) adev->mode(%d)",
          __func__, out->usecase, use_case_table[out->usecase], kvpairs, out->devices, adev->mode);
    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);

        pthread_mutex_lock(&adev->lock_inputs);
        lock_output_stream(out);
        pthread_mutex_lock(&adev->lock);

        if (val != 0) {
            devices_changed = out->devices != (audio_devices_t)val;
            out->devices = val;

            if ((adev->mode == AUDIO_MODE_IN_CALL) && !adev->in_call &&
                    (out == adev->primary_output)) {
		adev->current_call_output = out;
                start_voice_call(adev);
            } else if ((adev->mode == AUDIO_MODE_IN_CALL) && adev->in_call &&
                       (out == adev->primary_output)) {

                adev->current_call_output = out;
                voice_update_devices_for_all_voice_usecases(adev);
                stop_voice_call(adev);
                start_voice_call(adev);
            }

            if (!out->standby) {
                select_devices(adev, out->usecase);
            }



        }

        pthread_mutex_unlock(&adev->lock);
        pthread_mutex_unlock(&out->lock);
        pthread_mutex_unlock(&adev->lock_inputs);
    }

    str_parms_destroy(parms);
    ALOGV("%s: exit: code(%d)", __func__, ret);
    return ret;
}

static char* out_get_parameters(const struct audio_stream *stream, const char *keys)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct str_parms *query = str_parms_create_str(keys);
    char *str;
    char value[256];
    struct str_parms *reply = str_parms_create();
    size_t i, j;
    int ret;
    bool first = true;
    ALOGV("%s: enter: keys - %s", __func__, keys);
    ret = str_parms_get_str(query, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value, sizeof(value));
    if (ret >= 0) {
        value[0] = '\0';
        i = 0;
        while (out->supported_channel_masks[i] != 0) {
            for (j = 0; j < ARRAY_SIZE(out_channels_name_to_enum_table); j++) {
                if (out_channels_name_to_enum_table[j].value == out->supported_channel_masks[i]) {
                    if (!first) {
                        strcat(value, "|");
                    }
                    strcat(value, out_channels_name_to_enum_table[j].name);
                    first = false;
                    break;
                }
            }
            i++;
        }
        str_parms_add_str(reply, AUDIO_PARAMETER_STREAM_SUP_CHANNELS, value);
        str = str_parms_to_str(reply);
    } else {
        str = strdup(keys);
    }
    str_parms_destroy(query);
    str_parms_destroy(reply);
    ALOGV("%s: exit: returns - %s", __func__, str);
    return str;
}

static uint32_t out_get_latency(const struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;

    return (out->config.period_count * out->config.period_size * 1000) /
           (out->config.rate);
}

static int out_set_volume(struct audio_stream_out *stream, float left,
                          float right)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    (void)right;

    return -ENOSYS;
}

static ssize_t out_write(struct audio_stream_out *stream, const void *buffer,
                         size_t bytes)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    ssize_t ret = 0;
    struct listnode *node;
    size_t frame_size = audio_stream_out_frame_size(stream);
    size_t frames_wr = 0, frames_rq = 0;
    unsigned char *data = NULL;
    struct pcm_config config;
    const size_t frames = bytes / frame_size;



    lock_output_stream(out);
    if (out->standby) {
        out->standby = false;
        pthread_mutex_lock(&adev->lock);
        ret = start_output_stream(out);

        /* ToDo: If use case is compress offload should return 0 */
        if (ret != 0) {
            out->standby = true;
            pthread_mutex_unlock(&adev->lock);
            goto exit;
        }
        pthread_mutex_unlock(&adev->lock);
    }

    if (out->pcm) {
            size_t bytes_to_write = bytes;

            if (out->muted)
                memset((void *)buffer, 0, bytes);

            ret = pcm_write(out->pcm, (void *)buffer, bytes_to_write);
    }

exit:

    // For PCM we always consume the buffer and return #bytes regardless of ret.
    if (ret == 0) {
        out->written += frames;
    }

    pthread_mutex_unlock(&out->lock);

    if (ret != 0) {
        out_standby(&out->stream.common);
        usleep(bytes * 1000000 / audio_stream_out_frame_size(stream) /
               out_get_sample_rate(&out->stream.common));
    }

    return bytes;
}

static int out_get_render_position(const struct audio_stream_out *stream,
                                   uint32_t *dsp_frames)
{
    (void)stream;
    *dsp_frames = 0;
    return -EINVAL;
}

static int out_add_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int out_remove_audio_effect(const struct audio_stream *stream, effect_handle_t effect)
{
    (void)stream;
    (void)effect;
    return 0;
}

static int out_get_next_write_timestamp(const struct audio_stream_out *stream,
                                        int64_t *timestamp)
{
    (void)stream;
    (void)timestamp;
    return -EINVAL;
}

static int out_get_presentation_position(const struct audio_stream_out *stream,
                                   uint64_t *frames, struct timespec *timestamp)
{
    struct stream_out *out = (struct stream_out *)stream;
    int ret = -ENODATA;
    unsigned long dsp_frames;


    lock_output_stream(out);

    if (out->pcm) {
            unsigned int avail;
            if (pcm_get_htimestamp(out->pcm, &avail, timestamp) == 0) {
                size_t kernel_buffer_size = out->config.period_size * out->config.period_count;
                int64_t signed_frames = out->written - kernel_buffer_size + avail;
                // This adjustment accounts for buffering after app processor.
                // It is based on estimated DSP latency per use case, rather than exact.
                signed_frames -=
                    (render_latency(out->usecase) * out->sample_rate / 1000000LL);

                // It would be unusual for this value to be negative, but check just in case ...
                if (signed_frames >= 0) {
                    *frames = signed_frames;
                    ret = 0;
                }
            }
    }


done:

    pthread_mutex_unlock(&out->lock);

    return ret;
}

/** audio_stream_in implementation **/
static uint32_t in_get_sample_rate(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in->requested_rate;
}

static int in_set_sample_rate(struct audio_stream *stream, uint32_t rate)
{
    (void)stream;
    (void)rate;
    return -ENOSYS;
}

static uint32_t in_get_channels(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return in->main_channels;
}

static audio_format_t in_get_format(const struct audio_stream *stream)
{
    (void)stream;
    return AUDIO_FORMAT_PCM_16_BIT;
}

static int in_set_format(struct audio_stream *stream, audio_format_t format)
{
    (void)stream;
    (void)format;

    return -ENOSYS;
}

static size_t in_get_buffer_size(const struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;

    return get_input_buffer_size(in->requested_rate,
                                 in_get_format(stream),
                                 audio_channel_count_from_in_mask(in->main_channels),
                                 in->usecase_type,
                                 in->devices);
}

static int in_close_pcm_devices(struct stream_in *in)
{
    struct listnode *node;
    struct audio_device *adev = in->dev;


    if (in->pcm)
        pcm_close(in->pcm);

    return 0;
}


/* must be called with stream and hw device mutex locked */
static int do_in_standby_l(struct stream_in *in)
{
    int status = 0;

    if (!in->standby) {

        in_close_pcm_devices(in);

        status = stop_input_stream(in);

        if (in->read_buf) {
            free(in->read_buf);
            in->read_buf = NULL;
        }

        in->standby = 1;
    }
    return 0;
}

// called with adev->lock_inputs locked
static int in_standby_l(struct stream_in *in)
{
    struct audio_device *adev = in->dev;
    int status = 0;
    lock_input_stream(in);
    if (!in->standby) {
        pthread_mutex_lock(&adev->lock);
        status = do_in_standby_l(in);
        pthread_mutex_unlock(&adev->lock);
    }
    pthread_mutex_unlock(&in->lock);
    return status;
}

static int in_standby(struct audio_stream *stream)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    int status;
    ALOGV("%s: enter", __func__);
    pthread_mutex_lock(&adev->lock_inputs);
    status = in_standby_l(in);
    pthread_mutex_unlock(&adev->lock_inputs);
    ALOGV("%s: exit:  status(%d)", __func__, status);
    return status;
}

static int in_dump(const struct audio_stream *stream, int fd)
{
    (void)stream;
    (void)fd;

    return 0;
}

static int in_set_parameters(struct audio_stream *stream, const char *kvpairs)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int ret, val = 0;
    struct audio_usecase *uc_info;
    bool do_standby = false;
    struct listnode *node;

    ALOGV("%s: enter: kvpairs=%s", __func__, kvpairs);
    parms = str_parms_create_str(kvpairs);

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_INPUT_SOURCE, value, sizeof(value));

    pthread_mutex_lock(&adev->lock_inputs);
    lock_input_stream(in);
    pthread_mutex_lock(&adev->lock);
    if (ret >= 0) {
        val = atoi(value);
        /* no audio source uses val == 0 */
        if (((int)in->source != val) && (val != 0)) {
            in->source = val;
        }
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_STREAM_ROUTING, value, sizeof(value));
    if (ret >= 0) {
        val = atoi(value);
        if (((int)in->devices != val) && (val != 0)) {
            in->devices = val;
            /* If recording is in progress, change the tx device to new device */
            if (!in->standby) {
               ALOGV("update input routing change");
               select_devices(adev, in->usecase);
            }
        }
    }
    pthread_mutex_unlock(&adev->lock);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&adev->lock_inputs);
    str_parms_destroy(parms);

    if (ret > 0)
        ret = 0;

    return ret;
}

static char* in_get_parameters(const struct audio_stream *stream,
                               const char *keys)
{
    (void)stream;
    (void)keys;

    return strdup("");
}

static int in_set_gain(struct audio_stream_in *stream, float gain)
{
    (void)stream;
    (void)gain;

    return 0;
}

static ssize_t in_read(struct audio_stream_in *stream, void *buffer,
                       size_t bytes)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    ssize_t frames = -1;
    int ret = -1;
    int read_and_process_successful = false;

    size_t frames_rq = bytes / audio_stream_in_frame_size(stream);

    /* no need to acquire adev->lock_inputs because API contract prevents a close */
    lock_input_stream(in);
    if (in->standby) {
        pthread_mutex_unlock(&in->lock);
        pthread_mutex_lock(&adev->lock_inputs);
        lock_input_stream(in);
        if (!in->standby) {
            pthread_mutex_unlock(&adev->lock_inputs);
            goto false_alarm;
        }
        pthread_mutex_lock(&adev->lock);
        ret = start_input_stream(in);
        pthread_mutex_unlock(&adev->lock);
        pthread_mutex_unlock(&adev->lock_inputs);
        if (ret != 0) {
            goto exit;
        }
        in->standby = 0;
    }

false_alarm:
    if (in->pcm) {
            /*
             * Read PCM and:
             * - resample if needed
             * - process if pre-processors are attached
             * - discard unwanted channels
             */
            frames = read_and_process_frames(stream, buffer, frames_rq);
            if (frames >= 0)
                read_and_process_successful = true;
    }

    /*
     * Instead of writing zeroes here, we could trust the hardware
     * to always provide zeroes when muted.
     */
    if (read_and_process_successful == true && adev->mic_mute)
        memset(buffer, 0, bytes);

exit:
    pthread_mutex_unlock(&in->lock);

    if (read_and_process_successful == false) {
        in_standby(&in->stream.common);
        ALOGV("%s: read failed - sleeping for buffer duration", __func__);
        usleep(bytes * 1000000 / audio_stream_in_frame_size(stream) /
               in->requested_rate);
    }

    if (bytes > 0) {
        in->frames_read += bytes / audio_stream_in_frame_size(stream);
    }

    return bytes;
}

static uint32_t in_get_input_frames_lost(struct audio_stream_in *stream)
{
    (void)stream;

    return 0;
}

static int in_get_capture_position(const struct audio_stream_in *stream,
                                   int64_t *frames, int64_t *time)
{
    if (stream == NULL || frames == NULL || time == NULL) {
        return -EINVAL;
    }

    struct stream_in *in = (struct stream_in *)stream;
    int ret = -ENOSYS;

    pthread_mutex_lock(&in->lock);
    if (in->pcm) {
        struct timespec timestamp;
        unsigned int avail;
        if (pcm_get_htimestamp(in->pcm, &avail, &timestamp) == 0) {
            *frames = in->frames_read + avail;
            *time = timestamp.tv_sec * 1000000000LL + timestamp.tv_nsec;
            ret = 0;
        }
    }

    pthread_mutex_unlock(&in->lock);
    return ret;
}

static int add_remove_audio_effect(const struct audio_stream *stream,
                                   effect_handle_t effect,
                                   bool enable)
{
    struct stream_in *in = (struct stream_in *)stream;
    struct audio_device *adev = in->dev;
    int status = 0;
    effect_descriptor_t desc;
    status = (*effect)->get_descriptor(effect, &desc);
    if (status != 0)
        return status;

    ALOGI("add_remove_audio_effect(), effect type: %08x, enable: %d ", desc.type.timeLow, enable);

    pthread_mutex_lock(&adev->lock_inputs);
    lock_input_stream(in);
    pthread_mutex_lock(&in->dev->lock);

    if ((in->source == AUDIO_SOURCE_VOICE_COMMUNICATION) &&
            in->enable_aec != enable &&
            (memcmp(&desc.type, FX_IID_AEC, sizeof(effect_uuid_t)) == 0)) {
        in->enable_aec = enable;
        if (!in->standby)
            select_devices(in->dev, in->usecase);
    }

    ALOGW_IF(status != 0, "add_remove_audio_effect() error %d", status);
    pthread_mutex_unlock(&in->dev->lock);
    pthread_mutex_unlock(&in->lock);
    pthread_mutex_unlock(&adev->lock_inputs);
    return status;
}

static int in_add_audio_effect(const struct audio_stream *stream,
                               effect_handle_t effect)
{
    ALOGV("%s: effect %p", __func__, effect);
    return add_remove_audio_effect(stream, effect, true /* enabled */);
}

static int in_remove_audio_effect(const struct audio_stream *stream,
                                  effect_handle_t effect)
{
    ALOGV("%s: effect %p", __func__, effect);
    return add_remove_audio_effect(stream, effect, false /* disabled */);
}

static int adev_open_output_stream(struct audio_hw_device *dev,
                                   audio_io_handle_t handle,
                                   audio_devices_t devices,
                                   audio_output_flags_t flags,
                                   struct audio_config *config,
                                   struct audio_stream_out **stream_out,
                                   const char *address __unused)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_out *out;
    int i, ret;

    ALOGV("%s: enter: sample_rate(%d) channel_mask(%#x) devices(%#x) flags(%#x)",
          __func__, config->sample_rate, config->channel_mask, devices, flags);
    *stream_out = NULL;
    out = (struct stream_out *)calloc(1, sizeof(struct stream_out));

    if (devices == AUDIO_DEVICE_NONE)
        devices = AUDIO_DEVICE_OUT_SPEAKER;

    out->flags = flags;
    out->devices = devices;
    out->dev = adev;
    out->format = config->format;
    out->sample_rate = config->sample_rate;
    out->channel_mask = AUDIO_CHANNEL_OUT_STEREO;
    out->supported_channel_masks[0] = AUDIO_CHANNEL_OUT_STEREO;
    out->handle = handle;

    if (out->flags & AUDIO_OUTPUT_FLAG_DEEP_BUFFER) {
	out->usecase = USECASE_AUDIO_PLAYBACK_DEEP_BUFFER;
	out->config = pcm_config_deep_buffer;
        out->sample_rate = out->config.rate;
    }
    else {
        out->usecase = USECASE_AUDIO_PLAYBACK_LOW_LATENCY;
        out->config = pcm_config_low_latency;
        out->sample_rate = out->config.rate;
    }

    if (flags & AUDIO_OUTPUT_FLAG_PRIMARY) {
        if (adev->primary_output == NULL)
            adev->primary_output = out;
        else {
            ALOGE("%s: Primary output is already opened", __func__);
            ret = -EEXIST;
            goto error_open;
        }
    }

    /* Check if this usecase is already existing */
    pthread_mutex_lock(&adev->lock);
    if (get_usecase_from_id(adev, out->usecase) != NULL) {
        ALOGE("%s: Usecase (%d) is already present", __func__, out->usecase);
        pthread_mutex_unlock(&adev->lock);
        ret = -EEXIST;
        goto error_open;
    }
    pthread_mutex_unlock(&adev->lock);

    out->stream.common.get_sample_rate = out_get_sample_rate;
    out->stream.common.set_sample_rate = out_set_sample_rate;
    out->stream.common.get_buffer_size = out_get_buffer_size;
    out->stream.common.get_channels = out_get_channels;
    out->stream.common.get_format = out_get_format;
    out->stream.common.set_format = out_set_format;
    out->stream.common.standby = out_standby;
    out->stream.common.dump = out_dump;
    out->stream.common.set_parameters = out_set_parameters;
    out->stream.common.get_parameters = out_get_parameters;
    out->stream.common.add_audio_effect = out_add_audio_effect;
    out->stream.common.remove_audio_effect = out_remove_audio_effect;
    out->stream.get_latency = out_get_latency;
    out->stream.set_volume = out_set_volume;
    out->stream.write = out_write;
    out->stream.get_render_position = out_get_render_position;
    out->stream.get_next_write_timestamp = out_get_next_write_timestamp;
    out->stream.get_presentation_position = out_get_presentation_position;

    out->standby = 1;
    /* out->muted = false; by calloc() */
    /* out->written = 0; by calloc() */

    pthread_mutex_init(&out->lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&out->pre_lock, (const pthread_mutexattr_t *) NULL);
    pthread_cond_init(&out->cond, (const pthread_condattr_t *) NULL);

    config->format = out->stream.common.get_format(&out->stream.common);
    config->channel_mask = out->stream.common.get_channels(&out->stream.common);
    config->sample_rate = out->stream.common.get_sample_rate(&out->stream.common);

    *stream_out = &out->stream;
    ALOGV("%s: exit", __func__);
    return 0;

error_open:
    free(out);
    *stream_out = NULL;
    ALOGV("%s: exit: ret %d", __func__, ret);
    return ret;
}

static void adev_close_output_stream(struct audio_hw_device *dev,
                                     struct audio_stream_out *stream)
{
    struct stream_out *out = (struct stream_out *)stream;
    struct audio_device *adev = out->dev;
    (void)dev;

    ALOGV("%s: enter", __func__);
    out_standby(&stream->common);
    pthread_cond_destroy(&out->cond);
    pthread_mutex_destroy(&out->lock);
    pthread_mutex_destroy(&out->pre_lock);
    free(out->proc_buf_out);
    free(stream);
    ALOGV("%s: exit", __func__);
}

static int adev_set_parameters(struct audio_hw_device *dev, const char *kvpairs)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct str_parms *parms;
    char *str;
    char value[32];
    int val;
    int ret;

    ALOGV("%s: enter: %s", __func__, kvpairs);

    parms = str_parms_create_str(kvpairs);
    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_TTY_MODE, value, sizeof(value));
    if (ret >= 0) {
        int tty_mode;

        if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_OFF) == 0)
            tty_mode = TTY_MODE_OFF;
        else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_VCO) == 0)
            tty_mode = TTY_MODE_VCO;
        else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_HCO) == 0)
            tty_mode = TTY_MODE_HCO;
        else if (strcmp(value, AUDIO_PARAMETER_VALUE_TTY_FULL) == 0)
            tty_mode = TTY_MODE_FULL;
        else
            return -EINVAL;

        pthread_mutex_lock(&adev->lock);
        if (tty_mode != adev->tty_mode) {
            adev->tty_mode = tty_mode;
            if (adev->in_call)
                select_devices(adev, USECASE_VOICE_CALL);
        }
        pthread_mutex_unlock(&adev->lock);
    }


    ret = str_parms_get_str(parms,
                            AUDIO_PARAMETER_KEY_BT_SCO_WB,
                            value,
                            sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0) {
            adev->bluetooth_wbs = true;
        } else {
            adev->bluetooth_wbs = false;
        }
    }


    ret = str_parms_get_str(parms,
                            "BT_SCO",
                            value,
                            sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0) {
            start_bt_sco(adev);
        } else {

            stop_bt_sco(adev);
        }
    }

    ret = str_parms_get_str(parms,
                            "fmradio",
                            value,
                            sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0) {
            start_fm(adev);
        } else {

            stop_fm(adev);
        }
    }

    ret = str_parms_get_str(parms, AUDIO_PARAMETER_KEY_BT_NREC, value, sizeof(value));
    if (ret >= 0) {
        /* When set to false, HAL should disable EC and NS
         * But it is currently not supported.
         */
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->bluetooth_nrec = true;
        else
            adev->bluetooth_nrec = false;
    }

    ret = str_parms_get_str(parms, "screen_state", value, sizeof(value));
    if (ret >= 0) {
        if (strcmp(value, AUDIO_PARAMETER_VALUE_ON) == 0)
            adev->screen_off = false;
        else
            adev->screen_off = true;
    }

    ret = str_parms_get_int(parms, "rotation", &val);
    if (ret >= 0) {
        bool reverse_speakers = false;
        switch(val) {
        /* Assume 0deg rotation means the front camera is up with the usb port
         * on the lower left when the user is facing the screen. This assumption
         * is device-specific, not platform-specific like this code.
         */
        case 180:
            reverse_speakers = true;
            break;
        case 0:
        case 90:
        case 270:
            break;
        default:
            ALOGE("%s: unexpected rotation of %d", __func__, val);
        }
        pthread_mutex_lock(&adev->lock);
        if (adev->speaker_lr_swap != reverse_speakers) {
            adev->speaker_lr_swap = reverse_speakers;
        }
        pthread_mutex_unlock(&adev->lock);
    }

    str_parms_destroy(parms);
    ALOGV("%s: exit with code(%d)", __func__, ret);
    return ret;
}

static char* adev_get_parameters(const struct audio_hw_device *dev,
                                 const char *keys)
{
    (void)dev;
    (void)keys;

    return strdup("");
}

static int adev_init_check(const struct audio_hw_device *dev)
{
    (void)dev;

    return 0;
}

static int adev_set_voice_volume(struct audio_hw_device *dev, float volume)
{
    int ret = 0;
    struct audio_device *adev = (struct audio_device *)dev;
    pthread_mutex_lock(&adev->lock);
    /* cache volume */
    adev->voice_volume = volume;
    ret = set_voice_volume_l(adev, adev->voice_volume);
    pthread_mutex_unlock(&adev->lock);
    return ret;
}

static int adev_set_master_volume(struct audio_hw_device *dev, float volume)
{
    (void)dev;
    (void)volume;

    return -ENOSYS;
}

static int adev_get_master_volume(struct audio_hw_device *dev,
                                  float *volume)
{
    (void)dev;
    (void)volume;

    return -ENOSYS;
}

static int adev_set_master_mute(struct audio_hw_device *dev, bool muted)
{
    (void)dev;
    (void)muted;

    return -ENOSYS;
}

static int adev_get_master_mute(struct audio_hw_device *dev, bool *muted)
{
    (void)dev;
    (void)muted;

    return -ENOSYS;
}

static int adev_set_mode(struct audio_hw_device *dev, audio_mode_t mode)
{
    struct audio_device *adev = (struct audio_device *)dev;

    pthread_mutex_lock(&adev->lock);
    if (adev->mode != mode) {
        ALOGI("%s mode = %d", __func__, mode);
        adev->mode = mode;

        if ((mode == AUDIO_MODE_NORMAL) && adev->in_call) {
            stop_voice_call(adev);
	    adev->current_call_output = NULL;
        }
    }
    pthread_mutex_unlock(&adev->lock);
    return 0;
}

static int adev_set_mic_mute(struct audio_hw_device *dev, bool state)
{
    struct audio_device *adev = (struct audio_device *)dev;
    int err = 0;

    pthread_mutex_lock(&adev->lock);
    adev->mic_mute = state;

    if (adev->mode == AUDIO_MODE_IN_CALL) {
        /* TODO */
    }

    pthread_mutex_unlock(&adev->lock);
    return err;
}

static int adev_get_mic_mute(const struct audio_hw_device *dev, bool *state)
{
    struct audio_device *adev = (struct audio_device *)dev;

    *state = adev->mic_mute;

    return 0;
}

static size_t adev_get_input_buffer_size(const struct audio_hw_device *dev,
                                         const struct audio_config *config)
{
    (void)dev;

    /* NOTE: we default to built in mic which may cause a mismatch between what we
     * report here and the actual buffer size
     */
    return get_input_buffer_size(config->sample_rate,
                                 config->format,
                                 audio_channel_count_from_in_mask(config->channel_mask),
                                 PCM_CAPTURE /* usecase_type */,
                                 AUDIO_DEVICE_IN_BUILTIN_MIC);
}

static int adev_open_input_stream(struct audio_hw_device *dev,
                                  audio_io_handle_t handle __unused,
                                  audio_devices_t devices,
                                  struct audio_config *config,
                                  struct audio_stream_in **stream_in,
                                  audio_input_flags_t flags,
                                  const char *address __unused,
                                  audio_source_t source)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in;

    ALOGV("%s: enter", __func__);

    *stream_in = NULL;
    if (check_input_parameters(config->sample_rate, config->format,
                               audio_channel_count_from_in_mask(config->channel_mask)) != 0)
        return -EINVAL;

    // When making voice call, ignore the request of opening input stream by other apps.
    if (adev->in_call)
        return -EINVAL;

    usecase_type_t usecase_type = PCM_CAPTURE;

    in = (struct stream_in *)calloc(1, sizeof(struct stream_in));

    in->stream.common.get_sample_rate = in_get_sample_rate;
    in->stream.common.set_sample_rate = in_set_sample_rate;
    in->stream.common.get_buffer_size = in_get_buffer_size;
    in->stream.common.get_channels = in_get_channels;
    in->stream.common.get_format = in_get_format;
    in->stream.common.set_format = in_set_format;
    in->stream.common.standby = in_standby;
    in->stream.common.dump = in_dump;
    in->stream.common.set_parameters = in_set_parameters;
    in->stream.common.get_parameters = in_get_parameters;
    in->stream.common.add_audio_effect = in_add_audio_effect;
    in->stream.common.remove_audio_effect = in_remove_audio_effect;
    in->stream.set_gain = in_set_gain;
    in->stream.read = in_read;
    in->stream.get_input_frames_lost = in_get_input_frames_lost;
    in->stream.get_capture_position = in_get_capture_position;


    in->devices = devices;
    in->source = source;
    in->dev = adev;
    in->standby = 1;
    in->main_channels = config->channel_mask;
    in->requested_rate = config->sample_rate;
    if (config->sample_rate != CAPTURE_DEFAULT_SAMPLING_RATE)
        flags = flags & ~AUDIO_INPUT_FLAG_FAST;
    in->input_flags = flags;
    /* HW codec is limited to default channels. No need to update with
     * requested channels */
    in->config = pcm_config_audio_capture;

    /* Update config params with the requested sample rate and channels */
    in->usecase = USECASE_AUDIO_CAPTURE;
    in->usecase_type = usecase_type;

    pthread_mutex_init(&in->lock, (const pthread_mutexattr_t *) NULL);
    pthread_mutex_init(&in->pre_lock, (const pthread_mutexattr_t *) NULL);

    *stream_in = &in->stream;
    ALOGV("%s: exit", __func__);
    return 0;
}

static void adev_close_input_stream(struct audio_hw_device *dev,
                                    struct audio_stream_in *stream)
{
    struct audio_device *adev = (struct audio_device *)dev;
    struct stream_in *in = (struct stream_in*)stream;
    ALOGV("%s", __func__);

    /* prevent concurrent out_set_parameters, or out_write from standby */
    pthread_mutex_lock(&adev->lock_inputs);

    in_standby_l(in);
    pthread_mutex_destroy(&in->lock);
    pthread_mutex_destroy(&in->pre_lock);
    free(in->proc_buf_out);
    free(stream);

    pthread_mutex_unlock(&adev->lock_inputs);

    return;
}

static int adev_dump(const audio_hw_device_t *device, int fd)
{
    (void)device;
    (void)fd;

    return 0;
}

static int adev_close(hw_device_t *device)
{
    struct audio_device *adev = (struct audio_device *)device;
    free(adev->snd_dev_ref_cnt);
    audio_route_free(adev->audio_route);
    free(device);
    return 0;
}

static int adev_open(const hw_module_t *module, const char *name,
                     hw_device_t **device)
{
    struct audio_device *adev;
    int i, ret, retry_count;

    ALOGV("%s: enter", __func__);
    if (strcmp(name, AUDIO_HARDWARE_INTERFACE) != 0) return -EINVAL;

    adev = calloc(1, sizeof(struct audio_device));

    adev->device.common.tag = HARDWARE_DEVICE_TAG;
    adev->device.common.version = AUDIO_DEVICE_API_VERSION_2_0;
    adev->device.common.module = (struct hw_module_t *)module;
    adev->device.common.close = adev_close;

    adev->device.init_check = adev_init_check;
    adev->device.set_voice_volume = adev_set_voice_volume;
    adev->device.set_master_volume = adev_set_master_volume;
    adev->device.get_master_volume = adev_get_master_volume;
    adev->device.set_master_mute = adev_set_master_mute;
    adev->device.get_master_mute = adev_get_master_mute;
    adev->device.set_mode = adev_set_mode;
    adev->device.set_mic_mute = adev_set_mic_mute;
    adev->device.get_mic_mute = adev_get_mic_mute;
    adev->device.set_parameters = adev_set_parameters;
    adev->device.get_parameters = adev_get_parameters;
    adev->device.get_input_buffer_size = adev_get_input_buffer_size;
    adev->device.open_output_stream = adev_open_output_stream;
    adev->device.close_output_stream = adev_close_output_stream;
    adev->device.open_input_stream = adev_open_input_stream;
    adev->device.close_input_stream = adev_close_input_stream;
    adev->device.dump = adev_dump;

    /* Set the default route before the PCM stream is opened */
    adev->mode = AUDIO_MODE_NORMAL;
    adev->active_input = NULL;
    adev->primary_output = NULL;

    adev->voice_volume = 1.0f;
    adev->voice_wb = false;
    adev->voice_two_mic = true;

    adev->tty_mode = TTY_MODE_OFF;
    adev->bluetooth_nrec = true;
    adev->in_call = false;

    /* adev->cur_hdmi_channels = 0;  by calloc() */
    adev->snd_dev_ref_cnt = calloc(SND_DEVICE_MAX, sizeof(int));

    list_init(&adev->usecase_list);

    /* RIL */
    ril_open(&adev->ril);

    if (mixer_init(adev) != 0) {
        free(adev->snd_dev_ref_cnt);
        free(adev);
        ALOGE("%s: Failed to init, aborting.", __func__);
        *device = NULL;
        return -EINVAL;
    }


    if (access(SOUND_TRIGGER_HAL_LIBRARY_PATH, R_OK) == 0) {
        adev->sound_trigger_lib = dlopen(SOUND_TRIGGER_HAL_LIBRARY_PATH,
                                         RTLD_NOW);
        if (adev->sound_trigger_lib == NULL) {
            ALOGE("%s: DLOPEN failed for %s", __func__,
                  SOUND_TRIGGER_HAL_LIBRARY_PATH);
        } else {
            ALOGV("%s: DLOPEN successful for %s", __func__,
                  SOUND_TRIGGER_HAL_LIBRARY_PATH);
            adev->sound_trigger_open_for_streaming =
                    (int (*)(void))dlsym(adev->sound_trigger_lib,
                                         "sound_trigger_open_for_streaming");
            adev->sound_trigger_read_samples =
                    (size_t (*)(int, void *, size_t))dlsym(
                            adev->sound_trigger_lib,
                            "sound_trigger_read_samples");
            adev->sound_trigger_close_for_streaming =
                        (int (*)(int))dlsym(
                                adev->sound_trigger_lib,
                                "sound_trigger_close_for_streaming");
            if (!adev->sound_trigger_open_for_streaming ||
                !adev->sound_trigger_read_samples ||
                !adev->sound_trigger_close_for_streaming) {

                ALOGE("%s: Error grabbing functions in %s", __func__,
                      SOUND_TRIGGER_HAL_LIBRARY_PATH);
                adev->sound_trigger_open_for_streaming = 0;
                adev->sound_trigger_read_samples = 0;
                adev->sound_trigger_close_for_streaming = 0;
            }
        }
    }

    *device = &adev->device.common;


    ALOGV("%s: exit", __func__);
    return 0;
}

static struct hw_module_methods_t hal_module_methods = {
    .open = adev_open,
};

struct audio_module HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = AUDIO_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = AUDIO_HARDWARE_MODULE_ID,
        .name = "EXYNOS 7580 Audio HAL",
        .author = "Stenkin Evgeniy <stenkinevgeniy@gmail.com>",
        .methods = &hal_module_methods,
    },
};
