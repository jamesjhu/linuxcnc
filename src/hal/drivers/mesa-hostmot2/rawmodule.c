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

static int hm2_rawmodule_find_gtag(hostmot2_t *hm2, rtapi_u8 gtag, hm2_rawmodule_t **rawmodule)
{
    struct rtapi_list_head *ptr;
    rtapi_list_for_each(ptr, &hm2->rawmodules) {
        *rawmodule = rtapi_list_entry(ptr, hm2_rawmodule_t, list);
        if ((*rawmodule)->gtag == gtag) {
            return 0;
        }
    }
    *rawmodule = NULL;
    return -ENOENT;
}

static int hm2_rawmodule_config_num_instances(hostmot2_t *hm2, rtapi_u8 gtag, int *num_instances)
{
    int i;
    for (i = 0; i < HM2_MAX_RAWMODULE; i++) {
        if (hm2->config.num_rawmodules[i].gtag == gtag) {
            *num_instances = hm2->config.num_rawmodules[i].num_instances;
            return 0;
        }
    }
    return -ENOENT;
}

int hm2_rawmodule_parse_md(hostmot2_t *hm2, int md_index) 
{
    // All this function actually does is allocate memory
    // and give the generic modules names.
    
    
    // 
    // some standard sanity checks
    //
    
    int i, r = -EINVAL;
    int num_instances = 0;
    hm2_module_descriptor_t *md = &hm2->md[md_index];
    hm2_rawmodule_t *rawmodule;

    r = hm2_rawmodule_find_gtag(hm2, md->gtag, &rawmodule);
    if (r >= 0) {
        HM2_ERR(
                "found duplicate module descriptor for tag %i (inconsistent "
                "firmware), not loading driver\n",
                md->gtag
                );
        return -EINVAL;
    }
    
    r = hm2_rawmodule_config_num_instances(hm2, md->gtag, &num_instances);
    if (r < 0) {
        HM2_ERR(
                "could not find config entry for module with tag %i\n",
                md->gtag
                );
        return -EINVAL;
    }
    if (num_instances > md->instances) {
        HM2_ERR(
                "config defines %d modules with tag %i, but only %d are available, "
                "not loading driver\n",
                num_instances,
                md->gtag,
                md->instances
                );
        return -EINVAL;
    }
    
    if (num_instances == 0) {
        return 0;
    }
    
    // 
    // looks good, start, or continue, initializing
    // 
    
    rawmodule = rtapi_kmalloc(sizeof(hm2_rawmodule_t), RTAPI_GFP_KERNEL);
    if (rawmodule == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    rawmodule->num_instances = num_instances;
    rawmodule->gtag = md->gtag;
    rawmodule->clock_freq = md->clock_freq;
    rawmodule->version = md->version;
    
    rawmodule->instance = (hm2_rawmodule_instance_t *)hal_malloc(num_instances 
                                                                 * sizeof(hm2_rawmodule_instance_t));
    if (rawmodule->instance == NULL) {
        HM2_ERR("out of memory!\n");
        r = -ENOMEM;
        goto fail0;
    }

    for (i = 0; i < rawmodule->num_instances; i++) {
        hm2_rawmodule_instance_t *inst = &rawmodule->instance[i];
        r = snprintf(inst->name, sizeof(inst->name), "%s.module-%02x.%01d", hm2->llio->name, rawmodule->gtag, i);
        if (r >= (int)sizeof(inst->name)) {
            r = -EINVAL;
            goto fail0;
        }
        HM2_PRINT("created module interface function %s.\n", inst->name);
        inst->base_address = md->base_address + i * md->instance_stride;
        inst->register_stride = md->register_stride;
        inst->instance_stride = md->instance_stride;
    }

    rtapi_list_add_tail(&rawmodule->list, &hm2->rawmodules);

    return rawmodule->num_instances;

fail0:
    return r;
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_setup);
int hm2_rawmodule_setup(char *name, rtapi_u8 version, rtapi_u8 num_registers,
                        rtapi_u32 instance_stride, rtapi_u32 multiple_registers,
                        hm2_rawmodule_addrinfo_t *addrinfo)
{
    hostmot2_t *hm2;
    hm2_rawmodule_t *rawmodule;
    int i;
    int r = 0;
    i = hm2_get_rawmodule(&hm2, &rawmodule, name);
    if (i < 0) {
        HM2_ERR_NO_LL("Can not find module instance %s.\n", name);
        return -EINVAL;
    }

    if (!hm2_md_is_consistent_or_complain(hm2, i, version, num_registers, instance_stride, multiple_registers)) {
        HM2_ERR("inconsistent Module Descriptor!\n");
        return -EINVAL;
    }

    addrinfo->base_address = rawmodule->instance[i].base_address;
    addrinfo->register_stride = rawmodule->instance[i].register_stride;
    addrinfo->instance_stride = rawmodule->instance[i].instance_stride;

    return r;
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_add_tram_read_region);
int hm2_rawmodule_add_tram_read_region(char *name, rtapi_u16 addr, rtapi_u16 size, rtapi_u32 **buffer)
{
    hostmot2_t *hm2;
    hm2_rawmodule_t *rawmodule;
    int i;
    i = hm2_get_rawmodule(&hm2, &rawmodule, name);
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
    hm2_rawmodule_t *rawmodule;
    int i;
    i = hm2_get_rawmodule(&hm2, &rawmodule, name);
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
    hm2_rawmodule_t *rawmodule;
    int i, r;
    i = hm2_get_rawmodule(&hm2, &rawmodule, name);
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
    hm2_rawmodule_t *rawmodule;
    int i;
    i = hm2_get_rawmodule(&hm2, &rawmodule, name);
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
    rawmodule->instance[i].read_function = func;
    rawmodule->instance[i].rsubdata = subdata;
    return 0;
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_set_write_function);
int hm2_rawmodule_set_write_function(char *name, int (*func)(void *subdata), void *subdata)
{
    hostmot2_t *hm2;
    hm2_rawmodule_t *rawmodule;
    int i;
    i = hm2_get_rawmodule(&hm2, &rawmodule, name);
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
    rawmodule->instance[i].write_function = func;
    rawmodule->instance[i].wsubdata = subdata;
    return 0;
}

EXPORT_SYMBOL_GPL(hm2_rawmodule_write);
int hm2_rawmodule_write(char *name, rtapi_u32 addr, const void *buffer, int size)
{
    hostmot2_t *hm2;
    hm2_rawmodule_t *rawmodule;
    int i, r;
    i = hm2_get_rawmodule(&hm2, &rawmodule, name);
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
    hm2_rawmodule_t *rawmodule;
    struct rtapi_list_head *ptr;
    int i, r;
    int (*func)(void *subdata);
    rtapi_list_for_each(ptr, &hm2->rawmodules) {
        rawmodule = rtapi_list_entry(ptr, hm2_rawmodule_t, list);
        for (i = 0; i < rawmodule->num_instances; i++) {
            func = rawmodule->instance[i].read_function;
            if (func != NULL) {
                r = func(rawmodule->instance[i].rsubdata);
                if (r < 0) {
                    HM2_ERR("raw module read function @%p failed (returned %d)\n",
                            func, r);
                    continue;
                }
            }
        }
    }
}

void hm2_rawmodule_prepare_tram_write(hostmot2_t *hm2)
{
    hm2_rawmodule_t *rawmodule;
    struct rtapi_list_head *ptr;
    int i, r;
    int (*func)(void *subdata);
    rtapi_list_for_each(ptr, &hm2->rawmodules) {
        rawmodule = rtapi_list_entry(ptr, hm2_rawmodule_t, list);
        for (i = 0; i < rawmodule->num_instances; i++) {
            func = rawmodule->instance[i].write_function;
            if (func != NULL) {
                r = func(rawmodule->instance[i].wsubdata);
                if (r < 0) {
                    HM2_ERR("raw module write function @%p failed (returned %d)\n",
                            func, r);
                    continue;
                }
            }
        }
    }
}

void hm2_rawmodule_cleanup(hostmot2_t *hm2)
{
    while (hm2->rawmodules.next != &hm2->rawmodules) {
        struct rtapi_list_head *ptr = hm2->rawmodules.next;
        hm2_rawmodule_t *rawmodule = rtapi_list_entry(ptr, hm2_rawmodule_t, list);
        rtapi_list_del(ptr);
        rtapi_kfree(rawmodule);
    }
}


void hm2_rawmodule_print_module(hostmot2_t *hm2)
{
    int i;
    hm2_rawmodule_t *rawmodule;
    struct rtapi_list_head *ptr;
    rtapi_list_for_each(ptr, &hm2->rawmodules) {
        rawmodule = rtapi_list_entry(ptr, hm2_rawmodule_t, list);
        if (rawmodule->num_instances <= 0) continue;
        HM2_PRINT("Module 0x%02X: %d\n", rawmodule->gtag, rawmodule->num_instances);
        HM2_PRINT("    clock_frequency: %d Hz (%s MHz)\n", rawmodule->clock_freq, hm2_hz_to_mhz(rawmodule->clock_freq));
        HM2_PRINT("    version: %d\n", rawmodule->version);
        for (i = 0; i < rawmodule->num_instances; i++) {
            HM2_PRINT("    instance %d:\n", i);
            HM2_PRINT("    HAL name = %s\n", rawmodule->instance[i].name);
            HM2_PRINT("        base_address = 0x%04X\n", rawmodule->instance[i].base_address);
            HM2_PRINT("        register_stride = %d\n", rawmodule->instance[i].register_stride);
            HM2_PRINT("        instance_stride = %d\n", rawmodule->instance[i].instance_stride);
        }
    }
}
