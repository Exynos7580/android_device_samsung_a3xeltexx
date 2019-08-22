/*
 * Copyright (C) 2014 The Android Open Source Project
 * Copyright (C) 2019 Stenkin Evgeniy stenkinevgeniy@gmail.com
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

#define LOG_TAG "healthd-exynos7580"
#include <healthd/healthd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cutils/klog.h>
#include <sys/types.h>
#include <sys/sysinfo.h>

#define BATTERY_CAPACITY_MAX_PATH "/sys/class/power_supply/battery/batt_capacity_max"
#define EFS_CAPACITY_MAX_PATH "/efs/Battery/batt_capacity_max"

using namespace android;
static int efs_capacity_max;
static bool first_upd = 0;

static int read_sysfs(const char *path, char *buf, size_t size) {
    char *cp = NULL;

    int fd = open(path, O_RDONLY, 0);
    if (fd == -1) {
        KLOG_ERROR(LOG_TAG, "Could not open '%s'\n", path);
        return -1;
    }

    ssize_t count = TEMP_FAILURE_RETRY(read(fd, buf, size));
    if (count > 0)
        cp = (char *)memrchr(buf, '\n', count);

    if (cp)
        *cp = '\0';
    else
        buf[0] = '\0';

    close(fd);
    return count;
}

static void write_sysfs(const char *path, char *s)
{
    char buf[80];
    int len;
    int fd = open(path, O_WRONLY);

    if (fd < 0) {
        strerror_r(errno, buf, sizeof(buf));
        KLOG_ERROR(LOG_TAG, "Error opening %s: %s\n", path, buf);
        return;
    }

    len = write(fd, s, strlen(s));
    if (len < 0) {
        strerror_r(errno, buf, sizeof(buf));
        KLOG_ERROR(LOG_TAG, "Error writing to %s: %s\n", path, buf);
    }

    close(fd);
}

static int get_int_field(const char *path) {
    const int SIZE = 4;
    char buf[SIZE];

    int value = 0;
    if (read_sysfs(path, buf, SIZE) > 0) {
        value = strtoll(buf, NULL, 0);
    }
    return value;
}

static void write_int_field(const char *path, int value) {
    char buf[4];

    sprintf(buf, "%d", value);
    write_sysfs(path, buf);
}


static bool battery_status_check(struct BatteryProperties *props)
{
    if (props->batteryStatus == BATTERY_STATUS_CHARGING || props->batteryStatus == BATTERY_STATUS_FULL)
        return true;
    else
        return false;
}

int healthd_board_battery_update(struct BatteryProperties *props)
{

    if (efs_capacity_max != get_int_field(BATTERY_CAPACITY_MAX_PATH) && first_upd)
    {
	KLOG_INFO(LOG_TAG, "Update efs capacity_max to = %d\n",efs_capacity_max);
	efs_capacity_max = get_int_field(BATTERY_CAPACITY_MAX_PATH);
	write_int_field(EFS_CAPACITY_MAX_PATH, efs_capacity_max);
    }
    // return 0 to log periodic polled battery status to kernel log
    return 1;
}

void healthd_board_init(struct healthd_config *config)
{
    efs_capacity_max = get_int_field(EFS_CAPACITY_MAX_PATH);
    KLOG_INFO(LOG_TAG, "Current capacity_max from efs = %d\n",efs_capacity_max);
    write_int_field(BATTERY_CAPACITY_MAX_PATH, efs_capacity_max);
    first_upd = 1;

}

void healthd_board_mode_charger_draw_battery(struct android::BatteryProperties*)
{

}

 void healthd_board_mode_charger_battery_update(struct android::BatteryProperties*)
{

}

 void healthd_board_mode_charger_set_backlight(bool)
{

}

 void healthd_board_mode_charger_init()
{

}
