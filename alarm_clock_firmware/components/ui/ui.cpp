//////////////////////////////////////////////////////////////////////

#include <math.h>

#include <freertos/FreeRTOS.h>
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"

#include "util.h"
#include "chs_list.h"
#include "ui.h"

LOG_CONTEXT("ui");

//////////////////////////////////////////////////////////////////////

struct ui_draw_item : chs::list_node<ui_draw_item>
{
    uint32_t priority : 3;
    uint32_t flags : 29;

    ui_draw_function_t draw_function;
};

//////////////////////////////////////////////////////////////////////

namespace
{
    ui_input_handler ui_handler_stack[16];
    int ui_handler_stack_length = 0;

    ui_draw_item draw_items_pool[256];

    chs::linked_list<ui_draw_item> free_draw_items;

    chs::linked_list<ui_draw_item> live_draw_items[ui_draw_num_priorities];

    //////////////////////////////////////////////////////////////////////

    void init_draw_items_pool()
    {
        free_draw_items.clear();
        for(ui_draw_item &p : draw_items_pool) {
            free_draw_items.push_back(p);
        }
        for(auto &l : live_draw_items) {
            l.clear();
        }
    }

    //////////////////////////////////////////////////////////////////////

    ui_draw_item *allocate_draw_item(ui_draw_priority priority)
    {
        if(free_draw_items.empty()) {
            return nullptr;
        }
        ui_draw_item *new_item = free_draw_items.pop_back();
        new_item->priority = priority;
        new_item->flags = 0;
        live_draw_items[priority].push_back(new_item);
        return new_item;
    }

    //////////////////////////////////////////////////////////////////////

    void free_draw_item(ui_draw_item *i)
    {
        live_draw_items[i->priority].remove(i);
        free_draw_items.push_back(i);
    }

}    // namespace

//////////////////////////////////////////////////////////////////////

esp_err_t ui_init()
{
    init_draw_items_pool();
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

ui_draw_item_handle_t ui_add_item(ui_draw_priority_t priority, ui_draw_function_t draw_function)
{
    assert(priority >= ui_draw_priority_0 && priority < ui_draw_num_priorities);
    assert(draw_function != nullptr);

    ui_draw_item_handle_t new_item = allocate_draw_item(priority);
    if(new_item != nullptr) {
        new_item->draw_function = draw_function;
    }
    return new_item;
}

//////////////////////////////////////////////////////////////////////

esp_err_t ui_remove_item(ui_draw_item_handle_t item)
{
    if(item == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    free_draw_item(item);
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t ui_item_change_priority(ui_draw_item_handle_t item, ui_draw_priority new_priority)
{
    if(item == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if(new_priority == item->priority) {
        return ESP_OK;
    }
    live_draw_items[item->priority].remove(item);
    item->priority = new_priority;
    live_draw_items[new_priority].push_back(item);
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t ui_item_get_flags(ui_draw_item_handle_t item, ui_draw_item_flags *flags)
{
    if(item == nullptr || flags == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    *flags = static_cast<ui_draw_item_flags>(item->flags);
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t ui_item_set_flags(ui_draw_item_handle_t item, ui_draw_item_flags flags)
{
    if(item == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    item->flags |= flags;
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t ui_item_clear_flags(ui_draw_item_handle_t item, ui_draw_item_flags flags)
{
    if(item == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    item->flags &= ~flags;
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

esp_err_t ui_item_toggle_flags(ui_draw_item_handle_t item, ui_draw_item_flags flags)
{
    if(item == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    item->flags ^= flags;
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

void ui_draw(int frame)
{
    for(auto &l : live_draw_items) {
        for(auto *d = l.head(); d != l.done(); d = l.next(d)) {
            if((d->flags & uif_hidden) == 0) {
                d->draw_function(frame);
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////

esp_err_t ui_push_input_handler(ui_input_handler h)
{
    esp_err_t ret = ESP_OK;
    if(ui_handler_stack_length == countof(ui_handler_stack)) {
        ret = ESP_ERR_NO_MEM;
    } else {
        ui_handler_stack[ui_handler_stack_length] = h;
        ui_handler_stack_length += 1;
    }
    return ret;
}

//////////////////////////////////////////////////////////////////////

esp_err_t ui_pop_current_handler()
{
    if(ui_handler_stack_length == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    ui_handler_stack_length -= 1;
    return ESP_OK;
}

//////////////////////////////////////////////////////////////////////

ui_input_handler ui_get_current_handler()
{
    ui_input_handler h = nullptr;
    if(ui_handler_stack_length > 0) {
        h = ui_handler_stack[ui_handler_stack_length - 1];
    }
    return h;
}
