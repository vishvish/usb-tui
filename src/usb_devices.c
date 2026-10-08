/*
 * usb-tui - terminal browser for connected USB devices
 * Copyright (C) 2026 Vish Vishvanath
 * SPDX-License-Identifier: AGPL-3.0-only
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, version 3. See LICENSE for the full text.
 */

#include "usb_devices.h"

#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/IOUSBLib.h>
#include <IOKit/usb/USBSpec.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_cf_string(io_service_t service, const char *key,
                           char *buffer, size_t buffer_size)
{
    CFStringRef cf_key = CFStringCreateWithCString(
        kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    if (cf_key == NULL) {
        return;
    }
    CFTypeRef value = IORegistryEntryCreateCFProperty(
        service, cf_key, kCFAllocatorDefault, 0);
    CFRelease(cf_key);
    if (value == NULL) {
        return;
    }
    if (CFGetTypeID(value) == CFStringGetTypeID()) {
        CFStringGetCString((CFStringRef)value, buffer, (CFIndex)buffer_size,
                           kCFStringEncodingUTF8);
    }
    CFRelease(value);
}

static int copy_number(io_service_t service, const char *key, int *number)
{
    CFStringRef cf_key = CFStringCreateWithCString(
        kCFAllocatorDefault, key, kCFStringEncodingUTF8);
    if (cf_key == NULL) {
        return 0;
    }
    CFTypeRef value = IORegistryEntryCreateCFProperty(
        service, cf_key, kCFAllocatorDefault, 0);
    CFRelease(cf_key);
    if (value == NULL) {
        return 0;
    }
    int found = 0;
    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
        found = CFNumberGetValue((CFNumberRef)value, kCFNumberIntType, number);
    }
    CFRelease(value);
    return found;
}

static void format_id(io_service_t service, const char *key,
                      char *buffer, size_t buffer_size)
{
    int value = 0;
    if (copy_number(service, key, &value)) {
        snprintf(buffer, buffer_size, "0x%04X", value & 0xffff);
    }
}

static void format_location(io_service_t service, char *buffer,
                            size_t buffer_size)
{
    int value = 0;
    if (copy_number(service, "locationID", &value)) {
        snprintf(buffer, buffer_size, "0x%08X", (unsigned int)value);
    }
}

static void format_speed(io_service_t service, char *buffer,
                         size_t buffer_size)
{
    int speed = 0;
    if (!copy_number(service, "USBSpeed", &speed)) {
        return;
    }
    if (speed <= 0) {
        snprintf(buffer, buffer_size, "Unknown");
    } else if (speed <= 2) {
        snprintf(buffer, buffer_size, "Low (1.5 Mbps)");
    } else if (speed <= 12) {
        snprintf(buffer, buffer_size, "Full (12 Mbps)");
    } else if (speed <= 480) {
        snprintf(buffer, buffer_size, "High (480 Mbps)");
    } else if (speed <= 5000) {
        snprintf(buffer, buffer_size, "Super (5 Gbps)");
    } else {
        snprintf(buffer, buffer_size, "Super+ (%d Mbps)", speed);
    }
}

static void read_device(io_service_t service, usb_device *device)
{
    memset(device, 0, sizeof(*device));
    IORegistryEntryGetRegistryEntryID(service, &device->registry_id);
    copy_cf_string(service, kUSBProductString,
                   device->product, sizeof(device->product));
    copy_cf_string(service, kUSBVendorString,
                   device->manufacturer, sizeof(device->manufacturer));
    copy_cf_string(service, kUSBSerialNumberString,
                   device->serial, sizeof(device->serial));
    if (device->product[0] == '\0') {
        snprintf(device->product, sizeof(device->product), "Unknown device");
    }
    format_id(service, kUSBVendorID, device->vendor_id,
              sizeof(device->vendor_id));
    format_id(service, kUSBProductID, device->product_id,
              sizeof(device->product_id));
    format_speed(service, device->speed, sizeof(device->speed));
    format_location(service, device->location_id, sizeof(device->location_id));
}

int usb_devices_refresh(usb_device_list *list, char *error, size_t error_size)
{
    io_iterator_t iterator = IO_OBJECT_NULL;
    CFMutableDictionaryRef matching = IOServiceMatching(kIOUSBDeviceClassName);
    if (matching == NULL) {
        snprintf(error, error_size, "Could not create USB matching dictionary");
        return 0;
    }
    kern_return_t result = IOServiceGetMatchingServices(
        kIOMasterPortDefault, matching, &iterator);
    if (result != KERN_SUCCESS) {
        snprintf(error, error_size, "IOKit enumeration failed (%d)", result);
        return 0;
    }

    usb_device *items = NULL;
    size_t count = 0;
    size_t capacity = 0;
    io_service_t service;
    while ((service = IOIteratorNext(iterator)) != IO_OBJECT_NULL) {
        if (count == capacity) {
            size_t next_capacity = capacity == 0 ? 8 : capacity * 2;
            usb_device *next = realloc(items, next_capacity * sizeof(*items));
            if (next == NULL) {
                IOObjectRelease(service);
                IOObjectRelease(iterator);
                free(items);
                snprintf(error, error_size, "Out of memory listing USB devices");
                return 0;
            }
            items = next;
            capacity = next_capacity;
        }
        read_device(service, &items[count++]);
        IOObjectRelease(service);
    }
    IOObjectRelease(iterator);

    free(list->items);
    list->items = items;
    list->count = count;
    if (error_size > 0) {
        error[0] = '\0';
    }
    return 1;
}

void usb_devices_destroy(usb_device_list *list)
{
    free(list->items);
    list->items = NULL;
    list->count = 0;
}
