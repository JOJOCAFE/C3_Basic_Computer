#include "bin_service.h"

#include "bin_internal.h"

#include <string.h>
#include <strings.h>

#ifndef CONFIG_C3_SERVICE_HARDWARE
#define CONFIG_C3_SERVICE_HARDWARE 1
#endif

#ifndef CONFIG_C3_SERVICE_NANO
#define CONFIG_C3_SERVICE_NANO 1
#endif

#ifndef CONFIG_C3_SERVICE_TERM
#define CONFIG_C3_SERVICE_TERM 1
#endif

static const char *const hardware_aliases[] = {
    "/bin/hardware",
    "bin/hardware",
    "HARDWARE",
};

static const char *const nano_aliases[] = {
    "/bin/nano",
    "bin/nano",
    "EDIT",
};

static const char *const basic_aliases[] = {
    "BASIC",
};

static const char *const term_aliases[] = {
    "/bin/term",
    "bin/term",
};

static const bin_service_t g_services[] = {
#if CONFIG_C3_SERVICE_HARDWARE
    {
        .name = "hardware",
        .aliases = hardware_aliases,
        .alias_count = sizeof(hardware_aliases) / sizeof(hardware_aliases[0]),
        .exec = bin_hardware_exec,
        .removable = true,
    },
#endif
#if CONFIG_C3_SERVICE_NANO
    {
        .name = "nano",
        .aliases = nano_aliases,
        .alias_count = sizeof(nano_aliases) / sizeof(nano_aliases[0]),
        .exec = bin_nano_exec,
        .removable = true,
    },
    {
        .name = "basic",
        .aliases = basic_aliases,
        .alias_count = sizeof(basic_aliases) / sizeof(basic_aliases[0]),
        .exec = bin_basic_exec,
        .removable = true,
    },
#endif
#if CONFIG_C3_SERVICE_TERM
    {
        .name = "term",
        .aliases = term_aliases,
        .alias_count = sizeof(term_aliases) / sizeof(term_aliases[0]),
        .exec = bin_term_exec,
        .removable = true,
    },
#endif
};

const bin_service_t *bin_find_service(const char *name)
{
    if (!name || *name == '\0') {
        return NULL;
    }

    for (size_t i = 0; i < sizeof(g_services) / sizeof(g_services[0]); i++) {
        const bin_service_t *service = &g_services[i];
        if (strcasecmp(name, service->name) == 0) {
            return service;
        }
        for (size_t j = 0; j < service->alias_count; j++) {
            if (strcasecmp(name, service->aliases[j]) == 0) {
                return service;
            }
        }
    }

    return NULL;
}

void bin_list_services(const shell_exec_io_t *io)
{
    for (size_t i = 0; i < sizeof(g_services) / sizeof(g_services[0]); i++) {
        bin_writef(io, "/bin/%s", g_services[i].name);
        if (g_services[i].removable) {
            bin_write(io, " removable");
        }
        bin_write(io, "\r\n");
    }
}
//Keep Going.
