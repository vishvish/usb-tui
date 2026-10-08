/*
 * usb-tui - terminal browser for connected USB devices
 * Copyright (C) 2026 Vish Vishvanath
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, version 3. See LICENSE for the full text.
 */

#ifndef USB_DEVICES_H
#define USB_DEVICES_H

#include <stddef.h>
#include <stdint.h>

#define USB_TEXT_LENGTH 256

typedef struct {
    uint64_t registry_id;
    char product[USB_TEXT_LENGTH];
    char manufacturer[USB_TEXT_LENGTH];
    char vendor_id[16];
    char product_id[16];
    char serial[USB_TEXT_LENGTH];
    char speed[32];
    char location_id[16];
} usb_device;

typedef struct {
    usb_device *items;
    size_t count;
} usb_device_list;

int usb_devices_refresh(usb_device_list *list, char *error, size_t error_size);
void usb_devices_destroy(usb_device_list *list);

#endif
