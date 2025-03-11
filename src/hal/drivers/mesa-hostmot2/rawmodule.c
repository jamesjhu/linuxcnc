//
// Copyright (C) 2025 James Hu
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//
//
// Driver for raw access to Hostmot2 modules.
//


#include <rtapi_slab.h>

#include "rtapi.h"
#include "hal.h"
#include "hostmot2.h"

int hm2_rawmodule_parse_md(hostmot2_t *hm2, int md_index) 
{
    // All this function actually does is allocate memory
    // and give the generic modules names.
    
    
    // 
    // some standard sanity checks
    //
    
    int i, r = -EINVAL;
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    
    if (hm2->rawmodule.num_instances != 0) {
        HM2_ERR(
                "found duplicate Module Descriptor (inconsistent "
                "firmware), not loading driver %i\n",
                md->gtag
                );
        return -EINVAL;
    }
    
    if (hm2->config.num_rawmodules > md->instances) {
        HM2_ERR(
                "config defines %d raw modules, but only %d are available, "
                "not loading driver\n",
                hm2->config.num_rawmodules,
                md->instances
                );
        return -EINVAL;
    }
    
    if (hm2->config.num_rawmodules == 0) {
        return 0;
    }
    
    // 
    // looks good, start, or continue, initializing
    // 
    
    if (hm2->config.num_rawmodules == -1) {
        hm2->rawmodule.num_instances = md->instances;
    } else {
        hm2->rawmodule.num_instances = hm2->config.num_rawmodules;
    }
    
    hm2->rawmodule.instance = (hm2_rawmodule_instance_t *)hal_malloc(hm2->rawmodule.num_instances 
                                                                     * sizeof(hm2_rawmodule_instance_t));
    if (hm2->rawmodule.instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }
    
    for (i = 0; i < hm2->rawmodule.num_instances; i++) {
        hm2_rawmodule_instance_t *inst = &hm2->rawmodule.instance[i];
        inst->clock_freq = md->clock_freq;
        inst->gtag = md->gtag;
        inst->version = md->version;
        r = snprintf(inst->name, sizeof(inst->name), "%s.rawm-%02x.%01d", hm2->llio->name, inst->gtag, i);
        if (r >= (int)sizeof(inst->name)) {
            r = -EINVAL;
            goto fail0;
        }
        HM2_PRINT("created module interface function %s.\n", inst->name);
        inst->base_address = md->base_address + i * md->instance_stride;
        inst->register_stride = md->register_stride;
        inst->instance_stride = md->instance_stride;
    }
    return hm2->rawmodule.num_instances;
fail0:
    return r;
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_setup);
int hm2_rawmodule_setup(char *name, rtapi_u8 version, rtapi_u8 num_registers,
                        rtapi_u32 instance_stride, rtapi_u32 multiple_registers,
                        hm2_rawmodule_addrinfo_t *addrinfo)
{
    hostmot2_t *hm2;
    int i;
    int r = 0;
    i = hm2_get_rawmodule(&hm2, name);
    if (i < 0) {
        HM2_ERR_NO_LL("Can not find module instance %s.\n", name);
        return -EINVAL;
    }

    if (!hm2_md_is_consistent_or_complain(hm2, i, version, num_registers, instance_stride, multiple_registers)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    addrinfo->base_address = hm2->rawmodule.instance[i].base_address;
    addrinfo->register_stride = hm2->rawmodule.instance[i].register_stride;
    addrinfo->instance_stride = hm2->rawmodule.instance[i].instance_stride;

    return r;
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_add_tram_read_region);
int hm2_rawmodule_add_tram_read_region(char *name, rtapi_u16 addr, rtapi_u16 size, rtapi_u32 **buffer)
{
    hostmot2_t *hm2;
    int i;
    i = hm2_get_rawmodule(&hm2, name);
    if (i < 0) {
        HM2_ERR_NO_LL("Can not find module instance %s.\n", name);
        return -EINVAL;
    }
    return hm2_register_tram_read_region(hm2, addr, size, buffer);
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_add_tram_write_region);
int hm2_rawmodule_add_tram_write_region(char *name, rtapi_u16 addr, rtapi_u16 size, rtapi_u32 **buffer)
{
    hostmot2_t *hm2;
    int i;
    i = hm2_get_rawmodule(&hm2, name);
    if (i < 0) {
        HM2_ERR_NO_LL("Can not find module instance %s.\n", name);
        return -EINVAL;
    }
    return hm2_register_tram_write_region(hm2, addr, size, buffer);
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_allocate_tram);
int hm2_rawmodule_allocate_tram(char* name)
{
    hostmot2_t *hm2;
    int i, r;
    i = hm2_get_rawmodule(&hm2, name);
    if (i < 0) {
        HM2_ERR_NO_LL("Can not find module instance %s.\n", name);
        return -EINVAL;
    }
    r = hm2_allocate_tram_regions(hm2);
    if (r < 0) {
        HM2_ERR("Failed to register TRAM for raw module %s\n", name);
        return -1;
    }
    
    return 0;
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_set_read_function);
int hm2_rawmodule_set_read_function(char *name, int (*func)(void *subdata), void *subdata)
{
    hostmot2_t *hm2;
    int i;
    i = hm2_get_rawmodule(&hm2, name);
    if (i < 0) {
        HM2_ERR_NO_LL("Can not find module instance %s.\n", name);
        return -EINVAL;
    }
    if (func == NULL) { 
        HM2_ERR("Invalid function pointer passed to "
                "hm2_rawmodule_set_read_function.\n");
        return -1;
    }
    if (subdata == NULL) { 
        HM2_ERR("Invalid data pointer passed to "
                "hm2_rawmodule_set_read_function.\n");
        return -1;
    }
    hm2->rawmodule.instance[i].read_function = func;
    hm2->rawmodule.instance[i].rsubdata = subdata;
    return 0;
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_set_write_function);
int hm2_rawmodule_set_write_function(char *name, int (*func)(void *subdata), void *subdata)
{
    hostmot2_t *hm2;
    int i;
    i = hm2_get_rawmodule(&hm2, name);
    if (i < 0) {
        HM2_ERR_NO_LL("Can not find module instance %s.\n", name);
        return -EINVAL;
    }
    if (func == NULL) { 
        HM2_ERR("Invalid function pointer passed to "
                "hm2_rawmodule_set_write_function.\n");
        return -1;
    }
    if (subdata == NULL) { 
        HM2_ERR("Invalid data pointer passed to "
                "hm2_rawmodule_set_write_function.\n");
        return -1;
    }
    hm2->rawmodule.instance[i].write_function = func;
    hm2->rawmodule.instance[i].wsubdata = subdata;
    return 0;
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_write);
int hm2_rawmodule_write(char *name, rtapi_u32 addr, const void *buffer, int size)
{
    hostmot2_t *hm2;
    int i, r;
    i = hm2_get_rawmodule(&hm2, name);
    if (i < 0) {
        HM2_ERR_NO_LL("Can not find module instance %s.\n", name);
        return -EINVAL;
    }
    r = hm2->llio->write(hm2->llio, addr, buffer, size);
    if (r < 0) {
        HM2_ERR("raw module hm2->llio->write failure %s\n", name);
    }
    return r;
}

void hm2_rawmodule_process_tram_read(hostmot2_t *hm2)
{
    int i, r;
    int (*func)(void *subdata);
    for (i = 0; i < hm2->rawmodule.num_instances; i++) {
        func = hm2->rawmodule.instance[i].read_function;
        if (func != NULL) {
            r = func(hm2->rawmodule.instance[i].rsubdata);
            if (r < 0) {
                HM2_ERR("raw module read function @%p failed (returned %d)\n",
                        func, r);
                continue;
            }
        }
    }
}

void hm2_rawmodule_prepare_tram_write(hostmot2_t *hm2)
{
    int i, r;
    int (*func)(void *subdata);
    for (i = 0; i < hm2->rawmodule.num_instances; i++) {
        func = hm2->rawmodule.instance[i].write_function;
        if (func != NULL) {
            r = func(hm2->rawmodule.instance[i].wsubdata);
            if (r < 0) {
                HM2_ERR("raw module write function @%p failed (returned %d)\n",
                        func, r);
                continue;
            }
        }
    }
}

void hm2_rawmodule_cleanup(hostmot2_t *hm2)
{
    (void)hm2;
}


void hm2_rawmodule_print_module(hostmot2_t *hm2)
{
    int i;
    if (hm2->rawmodule.num_instances <= 0)
        return;
    for (i = 0; i < hm2->rawmodule.num_instances; i++) {
        HM2_PRINT("Module 0x%02X:\n", hm2->rawmodule.instance[i].gtag);
        HM2_PRINT("    instance %d:\n", i);
        HM2_PRINT("    version: %d\n", hm2->rawmodule.instance[i].version);
        HM2_PRINT("    HAL name = %s\n", hm2->rawmodule.instance[i].name);
        HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", hm2->rawmodule.instance[i].clock_freq, hm2_hz_to_mhz(hm2->rawmodule.instance[i].clock_freq));
    }
}
