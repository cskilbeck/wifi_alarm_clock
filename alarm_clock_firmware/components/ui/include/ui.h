//////////////////////////////////////////////////////////////////////

#pragma once

#include <stdint.h>
#include <esp_err.h>
#include "util.h"
#include "encoder.h"
#include "display.h"

#if defined(__cplusplus)
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////
// user input (from the encoder)

// return ui_input_handler_pop to remove current handler from stack

typedef enum
{
    ui_input_handler_pop = 0,
    ui_input_handler_keep = 1,

} ui_input_handler_status;

typedef ui_input_handler_status (*ui_input_handler)(int frame);

esp_err_t ui_init();

void ui_draw(int frame);

esp_err_t ui_push_input_handler(ui_input_handler h);
esp_err_t ui_pop_current_handler();
ui_input_handler ui_get_current_handler();

//////////////////////////////////////////////////////////////////////
// drawing ui items

// priority is 0 (at the bottom, drawn first) to 7 (at the top, drawn last)

typedef enum ui_draw_priority
{
    ui_draw_priority_min = 0,

    ui_draw_priority_0 = 0,
    ui_draw_priority_1 = 1,
    ui_draw_priority_2 = 2,
    ui_draw_priority_3 = 3,
    ui_draw_priority_4 = 4,
    ui_draw_priority_5 = 5,
    ui_draw_priority_6 = 6,
    ui_draw_priority_7 = 7,

    ui_draw_priority_max = 7,

    ui_draw_num_priorities = 8

} ui_draw_priority_t;

//////////////////////////////////////////////////////////////////////

typedef enum
{
    uif_hidden = 1 << 0,

} ui_draw_item_flags;

//////////////////////////////////////////////////////////////////////

typedef void (*ui_draw_function_t)(int frame);

typedef struct ui_draw_item *ui_draw_item_handle_t;

//////////////////////////////////////////////////////////////////////

ui_draw_item_handle_t ui_add_item(ui_draw_priority_t priority, ui_draw_function_t draw_function);
void ui_remove_item(ui_draw_item_handle_t item);
void ui_item_change_priority(ui_draw_item_handle_t item, ui_draw_priority new_priority);
void ui_item_clear_flags(ui_draw_item_handle_t item, ui_draw_item_flags flags);
void ui_item_set_flags(ui_draw_item_handle_t item, ui_draw_item_flags flags);
void ui_item_toggle_flags(ui_draw_item_handle_t item, ui_draw_item_flags flags);
ui_draw_item_flags ui_item_get_flags(ui_draw_item_handle_t item);

//////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
}
#endif
