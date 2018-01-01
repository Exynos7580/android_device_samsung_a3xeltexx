#include <stdio.h>
#include <stdlib.h>

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

#include <android-base/properties.h>

#include "log.h"
#include "property_service.h"
#include "util.h"
#include "vendor_init.h"

#include "init_sec.h"

#define MODEL_NAME_LEN 5  // e.g. "A310F"
#define BUILD_NAME_LEN 8  // e.g. "XXU3CQI2"
#define CODENAME_LEN   9  // e.g. "a3xeltexx"


static void property_override(char const prop[], char const value[]) {
    prop_info *pi;

    pi = (prop_info*) __system_property_find(prop);
    if (pi)
        __system_property_update(pi, value, strlen(value));
    else
        __system_property_add(prop, strlen(prop), value, strlen(value));
}

void vendor_load_properties()
{
    const std::string bootloader = android::base::GetProperty("ro.bootloader", "");
    const std::string bl_model = bootloader.substr(0, MODEL_NAME_LEN);
    const std::string bl_build = bootloader.substr(MODEL_NAME_LEN);

    std::string model;  // A310F
    std::string device; // a3xelte
    std::string name;    // a3xeltexx
    std::string description;
    std::string fingerprint;

    model = "SM-" + bl_model;

    for (size_t i = 0; i < VARIANT_MAX; i++) {
        std::string model_ = all_variants[i]->model;
        if (model.compare(model_) == 0) {
            device = all_variants[i]->codename;
            break;
        }
    }

    if (device.size() == 0) {
        device = "a3xeltexx";
    }

    name = device + "xx";

    description = name + "-user 7.0 NRD90M " + bl_model + bl_build + " release-keys";
    fingerprint = "samsung/" + name + "/" + device + ":7.0/NRD90M/" + bl_model + bl_build + ":user/release-keys";

    property_override("ro.product.model", model.c_str());
    property_override("ro.product.device", device.c_str());
    property_override("ro.product.name", name.c_str());
    property_override("ro.build.product", device.c_str());
    property_override("ro.build.description", description.c_str());
    property_override("ro.build.fingerprint", fingerprint.c_str());
}