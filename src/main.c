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

#include <ctype.h>
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

enum column_id {
    COL_PRODUCT,
    COL_MANUFACTURER,
    COL_VENDOR_ID,
    COL_PRODUCT_ID,
    COL_SERIAL,
    COL_SPEED,
    COL_LOCATION,
    COL_COUNT
};

static const char *column_names[COL_COUNT] = {
    "Product", "Manufacturer", "Vendor ID", "Product ID",
    "Serial", "Speed", "Location"
};
static const int compact_columns[] = {
    COL_PRODUCT, COL_MANUFACTURER, -1, COL_SPEED
};
static const char *compact_names[] = {
    "Product", "Manufacturer", "Vendor/Product", "Speed"
};

static int sort_column = COL_PRODUCT;
static int sort_reverse;
static char filter_text[128];
static int selected_row;
static int first_row;
static const usb_device_list *current_list;

static const char *device_value(const usb_device *device, int column)
{
    switch (column) {
    case COL_PRODUCT: return device->product;
    case COL_MANUFACTURER: return device->manufacturer;
    case COL_VENDOR_ID: return device->vendor_id;
    case COL_PRODUCT_ID: return device->product_id;
    case COL_SERIAL: return device->serial;
    case COL_SPEED: return device->speed;
    case COL_LOCATION: return device->location_id;
    default: return "";
    }
}

static int row_matches(const usb_device *device)
{
    if (filter_text[0] == '\0') {
        return 1;
    }
    for (int column = 0; column < COL_COUNT; ++column) {
        const char *text = device_value(device, column);
        size_t query_length = strlen(filter_text);
        for (size_t i = 0; text[i] != '\0'; ++i) {
            size_t j = 0;
            while (j < query_length && text[i + j] != '\0' &&
                   tolower((unsigned char)text[i + j]) ==
                   tolower((unsigned char)filter_text[j])) {
                ++j;
            }
            if (j == query_length) {
                return 1;
            }
        }
    }
    return 0;
}

static size_t visible_count(void)
{
    size_t count = 0;
    for (size_t i = 0; i < current_list->count; ++i) {
        if (row_matches(&current_list->items[i])) {
            ++count;
        }
    }
    return count;
}

static int compare_devices(const void *left, const void *right)
{
    const usb_device *a = *(const usb_device * const *)left;
    const usb_device *b = *(const usb_device * const *)right;
    int result = strcasecmp(device_value(a, sort_column),
                            device_value(b, sort_column));
    if (result == 0) {
        result = strcasecmp(a->product, b->product);
    }
    return sort_reverse ? -result : result;
}

static usb_device **sorted_visible_devices(size_t *count)
{
    *count = visible_count();
    if (*count == 0) {
        return NULL;
    }
    usb_device **rows = malloc(*count * sizeof(*rows));
    if (rows == NULL) {
        *count = 0;
        return NULL;
    }
    size_t index = 0;
    for (size_t i = 0; i < current_list->count; ++i) {
        if (row_matches(&current_list->items[i])) {
            rows[index++] = &current_list->items[i];
        }
    }
    qsort(rows, *count, sizeof(*rows), compare_devices);
    return rows;
}

static void draw_cell(int y, int x, int width, const char *text, int selected)
{
    if (width <= 0) {
        return;
    }
    if (selected) {
        attron(A_REVERSE);
    }
    mvaddnstr(y, x, text, width);
    int length = (int)strlen(text);
    for (int i = length; i < width; ++i) {
        mvaddch(y, x + i, ' ');
    }
    if (selected) {
        attroff(A_REVERSE);
    }
}

static void draw_table(const usb_device_list *list)
{
    current_list = list;
    int height, width;
    getmaxyx(stdscr, height, width);
    erase();

    attron(A_BOLD);
    mvaddnstr(0, 0, "USB Devices", width);
    attroff(A_BOLD);
    mvprintw(1, 0, "Filter: %s%s", filter_text,
             filter_text[0] ? "" : "(none)");
    mvprintw(1, width > 31 ? width - 31 : 0,
             "Sort: %d %s", sort_column + 1,
             sort_reverse ? "descending" : "ascending");

    int compact = width < 112;
    int column_count = compact ? 4 : COL_COUNT;
    int widths[COL_COUNT];
    static const int desired[COL_COUNT] = { 23, 19, 11, 11, 19, 17, 13 };
    static const int compact_widths[] = { 26, 18, 21, 16 };
    int usable_width = width;
    int remaining = usable_width;
    for (int i = 0; i < column_count; ++i) {
        widths[i] = compact ? compact_widths[i] : desired[i];
        remaining -= widths[i];
    }
    while (remaining > 0) {
        ++widths[0];
        --remaining;
    }
    while (remaining < 0) {
        int changed = 0;
        for (int i = 0; i < column_count && remaining < 0; ++i) {
            if (widths[i] > 5) {
                --widths[i];
                ++remaining;
                changed = 1;
            }
        }
        if (!changed) {
            break;
        }
    }

    int x = 0;
    for (int i = 0; i < column_count; ++i) {
        int cell_width = widths[i] - 1;
        char heading[64];
        int sort_key = compact ? compact_columns[i] : i;
        const char *name = compact ? compact_names[i] : column_names[i];
        snprintf(heading, sizeof(heading), "%s%s", name,
                 sort_key == sort_column ? (sort_reverse ? " v" : " ^") : "");
        attron(A_BOLD);
        draw_cell(3, x, cell_width, heading, 0);
        attroff(A_BOLD);
        x += widths[i];
    }

    size_t count = 0;
    usb_device **rows = sorted_visible_devices(&count);
    int body_height = height - 6;
    if (body_height < 1) {
        body_height = 1;
    }
    if (selected_row < 0) {
        selected_row = 0;
    }
    if (count == 0) {
        mvaddnstr(4, 0, filter_text[0] ? "No devices match this filter."
                                      : "No USB devices found.", width);
    } else {
        if ((size_t)selected_row >= count) {
            selected_row = (int)count - 1;
        }
        if (selected_row < first_row) {
            first_row = selected_row;
        }
        if (selected_row >= first_row + body_height) {
            first_row = selected_row - body_height + 1;
        }
        for (int row = first_row; row < (int)count && row < first_row + body_height; ++row) {
            const usb_device *device = rows[row];
            x = 0;
            for (int column = 0; column < column_count; ++column) {
                char identity[40];
                const char *value;
                if (compact && compact_columns[column] == -1) {
                    snprintf(identity, sizeof(identity), "%s/%s",
                             device->vendor_id[0] ? device->vendor_id : "?",
                             device->product_id[0] ? device->product_id : "?");
                    value = identity;
                } else {
                    int value_column = compact ? compact_columns[column] : column;
                    value = device_value(device, value_column);
                }
                draw_cell(4 + row - first_row, x, widths[column] - 1,
                          value, row == selected_row);
                x += widths[column];
            }
        }
    }
    free(rows);

    mvhline(height - 2, 0, ACS_HLINE, width);
    mvprintw(height - 1, 0,
             "j/k or arrows: move  Enter: details  /: filter  1-7: sort  r: refresh  q: quit");
    mvprintw(height - 3, 0, "%zu of %zu devices", count, list->count);
    refresh();
}

static void draw_details(const usb_device *device)
{
    int height, width;
    getmaxyx(stdscr, height, width);
    erase();
    attron(A_BOLD);
    mvaddnstr(0, 0, device->product, width);
    attroff(A_BOLD);
    mvhline(1, 0, ACS_HLINE, width);

    const char *labels[] = {
        "Manufacturer", "Vendor ID", "Product ID", "Serial Number",
        "Speed", "Location ID"
    };
    const char *values[] = {
        device->manufacturer, device->vendor_id, device->product_id,
        device->serial, device->speed, device->location_id
    };
    for (int i = 0; i < 6 && i + 3 < height - 2; ++i) {
        mvprintw(i + 3, 2, "%-18s", labels[i]);
        mvaddnstr(i + 3, 22, values[i], width > 22 ? width - 22 : 0);
    }
    mvprintw(height - 1, 0, "Press any key to return");
    refresh();
    getch();
}

static void prompt_filter(void)
{
    int height, width;
    getmaxyx(stdscr, height, width);
    echo();
    curs_set(1);
    move(height - 1, 0);
    clrtoeol();
    addstr("Filter: ");
    getnstr(filter_text, (int)sizeof(filter_text) - 1);
    noecho();
    curs_set(0);
    (void)width;
    selected_row = 0;
    first_row = 0;
}

int main(void)
{
    setlocale(LC_ALL, "");
    usb_device_list devices = { NULL, 0 };
    char error[256];
    if (!usb_devices_refresh(&devices, error, sizeof(error))) {
        fprintf(stderr, "usb-tui: %s\n", error);
        return EXIT_FAILURE;
    }

    if (initscr() == NULL) {
        fprintf(stderr, "usb-tui: could not initialize terminal\n");
        usb_devices_destroy(&devices);
        return EXIT_FAILURE;
    }
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    int running = 1;
    while (running) {
        draw_table(&devices);
        int key = getch();
        size_t count = visible_count();
        switch (key) {
        case 'q':
        case 'Q':
            running = 0;
            break;
        case KEY_UP:
        case 'k':
            if (selected_row > 0) --selected_row;
            break;
        case KEY_DOWN:
        case 'j':
            if ((size_t)(selected_row + 1) < count) ++selected_row;
            break;
        case KEY_NPAGE:
            selected_row += LINES - 6;
            if ((size_t)selected_row >= count && count > 0) selected_row = (int)count - 1;
            break;
        case KEY_PPAGE:
            selected_row -= LINES - 6;
            if (selected_row < 0) selected_row = 0;
            break;
        case '/':
            prompt_filter();
            break;
        case 'r':
        case 'R':
            if (!usb_devices_refresh(&devices, error, sizeof(error))) {
                endwin();
                fprintf(stderr, "usb-tui: %s\n", error);
                usb_devices_destroy(&devices);
                return EXIT_FAILURE;
            }
            selected_row = first_row = 0;
            break;
        case KEY_ENTER:
        case '\n':
        case '\r': {
            size_t sorted_count = 0;
            usb_device **rows = sorted_visible_devices(&sorted_count);
            if (rows != NULL && selected_row >= 0 && (size_t)selected_row < sorted_count) {
                draw_details(rows[selected_row]);
            }
            free(rows);
            break;
        }
        default:
            if (key >= '1' && key <= '7') {
                int next_column = key - '1';
                if (sort_column == next_column) {
                    sort_reverse = !sort_reverse;
                } else {
                    sort_column = next_column;
                    sort_reverse = 0;
                }
            }
            break;
        }
    }

    endwin();
    usb_devices_destroy(&devices);
    return EXIT_SUCCESS;
}
