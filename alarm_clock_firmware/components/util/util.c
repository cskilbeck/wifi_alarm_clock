
#include <stdio.h>
#include "esp_err.h"

#include "esp_rom_sys.h"

#include "util.h"

static void esp_error_check_failed_print(const char *msg, esp_err_t rc, const char *file, int line, const char *function, const char *expression, intptr_t addr)
{
    esp_rom_printf("%s failed: esp_err_t 0x%x", msg, rc);
#ifdef CONFIG_ESP_ERR_TO_NAME_LOOKUP
    esp_rom_printf(" (%s)", esp_err_to_name(rc));
#endif    // CONFIG_ESP_ERR_TO_NAME_LOOKUP
    esp_rom_printf(" at 0x%08x\n", addr);
    esp_rom_printf("file: \"%s\" line %d\nfunc: %s\nexpression: %s\n", file, line, function, expression);
}

void util_print_error_return_if_failed(esp_err_t rc, char const *file, int line, char const *function, char const *expression)
{
    esp_error_check_failed_print("ESP_RETURN_IF_FAILED", rc, file, line, function, expression, (intptr_t)__builtin_return_address(0));
}
