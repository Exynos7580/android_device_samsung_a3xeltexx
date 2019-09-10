/*
 * Copyright (C) 2008 The Android Open Source Project
 * Copyright (C) 2013 The CyanogenMod Project
 * Copyright (C) 2016 ShevT
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

#define LOG_TAG "lights"
#include <cutils/log.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/time.h>
#include <hardware/lights.h>

static pthread_once_t g_init = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

char const *const PANEL_FILE       = "/sys/class/backlight/panel/brightness";
char const *const BUTTON_FILE      = "/sys/class/sec/sec_touchkey/brightness";

char const *const WAKE_LOCK_PATH   = "/sys/power/wake_lock";
char const *const WAKE_UNLOCK_PATH = "/sys/power/wake_unlock";
char const *const WAKE_LOCK_NAME   = "notification_flash_timer";

#define FLASH_TIMER_CMD_ON 1
#define FLASH_TIMER_CMD_OFF 0

static timer_t g_flash_timer_id;
static struct itimerspec g_flash_timer;
static int g_flash_mode;
static struct timespec g_flash_on_time;
static struct timespec g_flash_off_time;
static int g_flash_cmd;

void init_g_lock(void)
{
    pthread_mutex_init(&g_lock, NULL);
}

static int read_int(const char *path)
{
    int fd;
    char buffer[2];

    fd = open(path, O_RDONLY);
    if (fd >= 0) {
        read(fd, buffer, 1);
    } else {
        ALOGE("read_int failed to open %s\n", path);
        return -1;
    }
    close(fd);

    return atoi(buffer);
}

static int write_int(char const *path, int value)
{
    int fd;
    static int already_warned = 0;

    ALOGV("write_int: path %s, value %d", path, value);
    fd = open(path, O_RDWR);

    if (fd >= 0) {
        char buffer[20];
        int bytes = sprintf(buffer, "%d\n", value);
        int amt = write(fd, buffer, bytes);
        close(fd);
        return amt == -1 ? -errno : 0;
    } else {
        if (already_warned == 0) {
            ALOGE("write_int failed to open %s\n", path);
            already_warned = 1;
        }
        return -errno;
    }
}

static int is_lit(struct light_state_t const* state)
{
    return state->color & 0x00ffffff;
}

static int rgb_to_brightness(struct light_state_t const *state)
{
    int color = is_lit(state);

    return ((77*((color>>16) & 0x00ff))
        + (150*((color>>8) & 0x00ff)) + (29*(color & 0x00ff))) >> 8;
}

static void notifications_flash_callback(int signum)
{
    int v = 0;
    int ret = 0;

    pthread_mutex_lock(&g_lock);
    if (LIGHT_FLASH_TIMED == g_flash_mode) {
       if (FLASH_TIMER_CMD_ON == g_flash_cmd) {
            g_flash_timer.it_value = g_flash_on_time;
            g_flash_cmd = FLASH_TIMER_CMD_OFF;
            v = 1;
       } else {
            g_flash_timer.it_value = g_flash_off_time;
            g_flash_cmd = FLASH_TIMER_CMD_ON;
            v = 0;
       }
        timer_settime(g_flash_timer_id, 0, &g_flash_timer, NULL);
        write_int(BUTTON_FILE, v);
    } else {
        ALOGD("End flashing");
        write_int(BUTTON_FILE, 0);
    }
    pthread_mutex_unlock(&g_lock);
}

static struct timespec to_timespec(int total_ms)
{
    struct timespec ts;
    ts.tv_sec = total_ms / 1000;
    ts.tv_nsec = 1000000 * (total_ms % 1000);

    return ts;
}

static void set_wakelock(bool acquire)
{
    int fd;
    const char *const path = acquire ? WAKE_LOCK_PATH : WAKE_UNLOCK_PATH;
    fd = open(path, O_WRONLY);
    if (fd != -1) {
        write(fd, WAKE_LOCK_NAME, strlen(WAKE_LOCK_NAME));
        close(fd);
    }
}

static int set_light_notifications(struct light_device_t* dev __unused,
                                    struct light_state_t const* state)
{
    int brightness = 0;
    int v = 0;
    int ret = 0;

    brightness = rgb_to_brightness(state);
    if (brightness+state->color == 0 || brightness > 100) {
        if (is_lit(state))
            v = 1;
        } else
            v = 0;
    ALOGI("notifications: fMode %u, onMS %u, offMS %u, v %u", state->flashMode,
            state->flashOnMS, state->flashOffMS, v);
    pthread_mutex_lock(&g_lock);

    if (LIGHT_FLASH_TIMED == state->flashMode &&
       state->flashOffMS > 0 && state->flashOnMS > 0) {
        if (LIGHT_FLASH_TIMED != g_flash_mode)
            set_wakelock(true);

        g_flash_mode = LIGHT_FLASH_TIMED;
        g_flash_on_time = to_timespec(state->flashOnMS);
        g_flash_off_time = to_timespec(state->flashOffMS);
        g_flash_cmd = FLASH_TIMER_CMD_OFF;
        g_flash_timer.it_value = g_flash_on_time;

        ALOGD("set notifications flash: onMS %u, offMS %u", state->flashOnMS, state->flashOffMS);

        timer_settime(g_flash_timer_id, 0, &g_flash_timer, NULL);
        ret = write_int(BUTTON_FILE, 1);
    } else {
        if (LIGHT_FLASH_TIMED == g_flash_mode) {
            if (0 == v) {
                g_flash_mode = LIGHT_FLASH_NONE;
                set_wakelock(false);
            }
        }
        ret = write_int(BUTTON_FILE, 1);
    }
    g_flash_mode = state->flashMode;
    pthread_mutex_unlock(&g_lock);

    return ret;
}

static int set_light_backlight(struct light_device_t *dev __unused,
            struct light_state_t const *state)
{
    int err = 0;
    int brightness = rgb_to_brightness(state);

    pthread_mutex_lock(&g_lock);
    err = write_int(PANEL_FILE, brightness);
    pthread_mutex_unlock(&g_lock);

    return err;
}

static int set_light_buttons(struct light_device_t* dev __unused,
        struct light_state_t const* state)
{
    int err = 0;
    int on = is_lit(state);

    pthread_mutex_lock(&g_lock);
    err = write_int(BUTTON_FILE, on?1:0);
    ALOGD("set_light_buttons: on %u", on);
    pthread_mutex_unlock(&g_lock);

    return err;
}

static int close_lights(struct light_device_t *dev)
{
    ALOGV("close_light is called");
    if (dev)
        free(dev);

    return 0;
}

static int set_light_leds_noop(struct light_device_t *dev __unused,
            struct light_state_t const *state)
{
    return 0;
}

static int open_lights(const struct hw_module_t *module, char const *name,
                        struct hw_device_t **device)
{
    struct sigaction sa;
    int (*set_light)(struct light_device_t *dev,
        struct light_state_t const *state);

    if (0 == strcmp(LIGHT_ID_BACKLIGHT, name))
        set_light = set_light_backlight;
    else if (0 == strcmp(LIGHT_ID_BUTTONS, name))
        set_light = set_light_buttons;
    else if (0 == strcmp(LIGHT_ID_BATTERY, name))
        set_light = set_light_leds_noop;
    else if (0 == strcmp(LIGHT_ID_NOTIFICATIONS, name)) {
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sa.sa_handler = notifications_flash_callback;
        sigaction (SIGALRM, &sa, NULL);
        timer_create(CLOCK_MONOTONIC, NULL, &g_flash_timer_id);
        set_light = set_light_notifications;
    }
    else if (0 == strcmp(LIGHT_ID_ATTENTION, name))
        set_light = set_light_notifications;
    else
        return -EINVAL;

    pthread_once(&g_init, init_g_lock);

    struct light_device_t *dev = malloc(sizeof(struct light_device_t));
    memset(dev, 0, sizeof(*dev));

    dev->common.tag = HARDWARE_DEVICE_TAG;
    dev->common.version = 0;
    dev->common.module = (struct hw_module_t *)module;
    dev->common.close = (int (*)(struct hw_device_t *))close_lights;
    dev->set_light = set_light;

    *device = (struct hw_device_t *)dev;

    return 0;
}

static struct hw_module_methods_t lights_module_methods = {
    .open =  open_lights,
};

struct hw_module_t HAL_MODULE_INFO_SYM = {
    .tag = HARDWARE_MODULE_TAG,
    .version_major = 1,
    .version_minor = 0,
    .id = LIGHTS_HARDWARE_MODULE_ID,
    .name = "lt03xx Lights Module",
    .author = "The CyanogenMod Project",
    .methods = &lights_module_methods,
};
