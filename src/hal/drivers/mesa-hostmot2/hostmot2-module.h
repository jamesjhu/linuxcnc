//
//    Copyright (C) 2025 James Hu
//
//    This program is free software; you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation; either version 2 of the License, or
//    (at your option) any later version.
//
//    This program is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with this program; if not, write to the Free Software
//    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
//

#ifndef __HOSTMOT2_MODULE_H
#define __HOSTMOT2_MODULE_H

#include <rtapi.h>

typedef struct {
    rtapi_u16 base_address;
    rtapi_u32 register_stride;
    rtapi_u32 instance_stride;
} hm2_rawmodule_addrinfo_t;

RTAPI_BEGIN_DECLS

int hm2_rawmodule_setup(char *name, hm2_rawmodule_addrinfo_t *addrinfo);
int hm2_rawmodule_add_tram_read_region(char *name, rtapi_u16 addr, rtapi_u16 size, rtapi_u32 **buffer);
int hm2_rawmodule_add_tram_write_region(char *name, rtapi_u16 addr, rtapi_u16 size, rtapi_u32 **buffer);
int hm2_rawmodule_allocate_tram(char* name);
int hm2_rawmodule_set_read_function(char *name, int (*func)(void *subdata), void *subdata);
int hm2_rawmodule_set_write_function(char *name, int (*func)(void *subdata), void *subdata);
int hm2_rawmodule_write(char *name, rtapi_u32 addr, const void *buffer, int size);

RTAPI_END_DECLS

#endif
