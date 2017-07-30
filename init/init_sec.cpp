/*
   Copyright (c) 2016, The CyanogenMod Project. All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors may be used to endorse or promote products derived
      from this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
   ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
   BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
   BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
   OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
   IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   File Name : init_sec.c
   Create Date : 2016.04.13
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "vendor_init.h"
#include "property_service.h"
#include "log.h"
#include "util.h"

#define _REALLY_INCLUDE_SYS__SYSTEM_PROPERTIES_H_
#include <sys/_system_properties.h>

void property_override(char const prop[], char const value[])
{
    prop_info *pi;

    pi = (prop_info*) __system_property_find(prop);
    if (pi)
        __system_property_update(pi, value, strlen(value));
    else
        __system_property_add(prop, strlen(prop), value, strlen(value));
}


void make_me_dual()
{
	property_set("rild.libpath2", "/system/lib/libsec-ril-dsds.so");
	property_override("persist.radio.multisim.config", "dsds");
	property_override("ro.multisim.simslotcount", "2");
}

void vendor_load_properties()
{

    std::string bootloader = property_get("ro.bootloader");

    if (bootloader.find("A310F") == 0) {
        property_override("ro.build.fingerprint", "samsung/a3xeltexx/a3xelte:6.0.1/MMB29K/A310FXXU3BQC2:user/release-keys");
        property_override("ro.build.description", "a3xeltexx-user 6.0.1 MMB29K A310FXXU3BQC2 release-keys");
        property_override("ro.product.model", "SM-A310F");
        property_override("ro.product.device", "a3xelte");
    } else if (bootloader.find("A310M") == 0) {
        property_override("ro.build.fingerprint", "samsung/a3xeltexx/a3xelte:6.0.1/MMB29K/A310FXXU3BQC2:user/release-keys");
        property_override("ro.build.description", "a3xeltexx-user 6.0.1 MMB29K A310FXXU3BQC2 release-keys");
        property_override("ro.product.model", "SM-A310M");
        property_override("ro.product.device", "a3xelte");
    } else {
        property_override("ro.build.fingerprint", "samsung/a3xeltexx/a3xelte:6.0.1/MMB29K/A310FXXU3BQC2:user/release-keys");
        property_override("ro.build.description", "a3xeltexx-user 6.0.1 MMB29K A310FXXU3BQC2 release-keys");
        property_override("ro.product.model", "SM-A310Y");
        property_override("ro.product.device", "a3xeltexx");
    }

    std::string device = property_get("ro.product.device");
    std::string devicename = property_get("ro.product.model");
    ERROR("Found bootloader id %s setting build properties for %s device\n", bootloader.c_str(), devicename.c_str());
}
