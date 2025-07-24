/*
 * Arm SCP/MCP Software
 * Copyright (c) 2024, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <mod_sensor_manager.h>

#include <fwk_assert.h>
#include <fwk_id.h>
#include <fwk_interrupt.h>
#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_status.h>

#include <stddef.h>
#include <stdint.h>

/*!
 * \brief Registration entry for sensor notifications
 */
struct sensor_registration {
    /*! Callback function */
    sensor_notification_callback_t callback;
    /*! Requester module ID */
    fwk_id_t requester_id;
    /*! Detector ID (UINT_MAX for all detectors) */
    unsigned int detector_id;
    /*! Whether this entry is active */
    bool active;
};

/*!
 * \brief Detector context for each individual detector
 */
struct detector_context {
    /*! Detector configuration */
    const struct detector_config *config;
    /*! Array of registrations for this detector */
    struct sensor_registration *registrations;
    /*! Number of active registrations */
    unsigned int active_count;
    /*! Maximum registrations allowed */
    unsigned int max_registrations;
    /*! Current sensor value */
    uint32_t current_value;
    /*! Previous sensor value */
    uint32_t previous_value;
    /*! Whether this detector is enabled */
    bool enabled;
    /*! Whether the current value is within normal threshold range */
    bool is_in_normal_range;
};

/*!
 * \brief Detector context for each individual detector
 */
struct detector_context {
    /*! Detector configuration */
    const struct detector_config *config;
    /*! Array of registrations for this detector */
    struct sensor_registration *registrations;
    /*! Number of active registrations */
    unsigned int active_count;
    /*! Maximum registrations allowed */
    unsigned int max_registrations;
    /*! Current sensor value */
    uint32_t current_value;
    /*! Whether this detector is enabled */
    bool enabled;
};

/*!
 * \brief Sensor context for each sensor type
 */
struct sensor_context {
    /*! Array of detector contexts (2 detectors per sensor) */
    struct detector_context detectors[DETECTORS_PER_SENSOR];
    /*! Array of registrations for all detectors (wildcard registrations) */
    struct sensor_registration *global_registrations;
    /*! Number of active global registrations */
    unsigned int global_active_count;
    /*! Maximum global registrations allowed */
    unsigned int max_global_registrations;
};

/*!
 * \brief Module context
 */
static struct {
    /*! Module configuration */
    const struct mod_sensor_manager_config *config;
    /*! Sensor contexts for each type */
    struct sensor_context sensors[SENSOR_TYPE_COUNT];
} sensor_manager_ctx;

/*
 * Static helper functions
 */

static uint32_t read_sensor_register(uintptr_t base_addr)
{
    /* Read sensor value from hardware register */
    return *(volatile uint32_t *)base_addr;
}

static bool is_value_in_normal_range(uint32_t value, const struct detector_config *config)
{
    if (!config->threshold_enabled) {
        return true; /* Always normal if threshold monitoring is disabled */
    }

    return (value >= config->threshold_low && value <= config->threshold_high);
}

static enum sensor_interrupt_type determine_interrupt_type(
    uint32_t current_value,
    bool was_in_normal_range,
    const struct detector_config *config)
{
    bool is_now_in_normal_range = is_value_in_normal_range(current_value, config);

    if (!config->threshold_enabled) {
        /* If threshold monitoring is disabled, always report as normal */
        return SENSOR_INTERRUPT_THRESHOLD_NORMAL;
    }

    if (was_in_normal_range && !is_now_in_normal_range) {
        /* Transitioned from normal to out-of-range */
        return SENSOR_INTERRUPT_THRESHOLD_EXCEEDED;
    } else if (!was_in_normal_range && is_now_in_normal_range) {
        /* Transitioned from out-of-range to normal */
        return SENSOR_INTERRUPT_THRESHOLD_NORMAL;
    } else {
        /* No state change, use current state */
        return is_now_in_normal_range ?
            SENSOR_INTERRUPT_THRESHOLD_NORMAL :
            SENSOR_INTERRUPT_THRESHOLD_EXCEEDED;
    }
}

static bool get_sensor_and_detector_from_irq(unsigned int irq, enum sensor_type *type, unsigned int *detector_id)
{
    enum sensor_type sensor_type;
    unsigned int detector;

    /* Check all sensor types and their detectors */
    for (sensor_type = 0; sensor_type < SENSOR_TYPE_COUNT; sensor_type++) {
        for (detector = 0; detector < DETECTORS_PER_SENSOR; detector++) {
            struct detector_context *detector_ctx =
                &sensor_manager_ctx.sensors[sensor_type].detectors[detector];

            if (detector_ctx->config && detector_ctx->config->irq == irq) {
                *type = sensor_type;
                *detector_id = detector;
                return true;
            }
        }
    }

    return false; /* IRQ not found */
}

static void notify_registered_modules(
    enum sensor_type type,
    unsigned int detector_id,
    enum sensor_interrupt_type interrupt_type,
    uint32_t value)
{
    struct sensor_context *sensor_ctx = &sensor_manager_ctx.sensors[type];
    struct detector_context *detector_ctx = &sensor_ctx->detectors[detector_id];
    struct sensor_registration *reg;
    unsigned int i;
    const char *interrupt_type_str =
        (interrupt_type == SENSOR_INTERRUPT_THRESHOLD_EXCEEDED) ? "EXCEEDED" : "NORMAL";

    /* Notify detector-specific registrations */
    for (i = 0; i < detector_ctx->max_registrations; i++) {
        reg = &detector_ctx->registrations[i];
        if (reg->active && reg->callback != NULL) {
            FWK_LOG_DEBUG(
                "[SENSOR_MGR] Notifying module 0x%x for sensor type %d, detector %d, %s, value: %u",
                fwk_id_get_module_idx(reg->requester_id),
                type,
                detector_id,
                interrupt_type_str,
                value);

            reg->callback(type, detector_id, interrupt_type, value, reg->requester_id);
        }
    }

    /* Notify global registrations (wildcard) */
    for (i = 0; i < sensor_ctx->max_global_registrations; i++) {
        reg = &sensor_ctx->global_registrations[i];
        if (reg->active && reg->callback != NULL) {
            FWK_LOG_DEBUG(
                "[SENSOR_MGR] Notifying module 0x%x (global) for sensor type %d, detector %d, %s, value: %u",
                fwk_id_get_module_idx(reg->requester_id),
                type,
                detector_id,
                interrupt_type_str,
                value);

            reg->callback(type, detector_id, interrupt_type, value, reg->requester_id);
        }
    }
}

/*
 * Interrupt handlers
 */

static void sensor_interrupt_handler(void)
{
    unsigned int irq;
    enum sensor_type type;
    unsigned int detector_id;
    uint32_t value;
    struct detector_context *detector_ctx;
    enum sensor_interrupt_type interrupt_type;
    bool was_in_normal_range;

    /* Get the current IRQ number */
    irq = fwk_interrupt_get_current();

    /* Determine sensor type and detector ID from IRQ */
    if (!get_sensor_and_detector_from_irq(irq, &type, &detector_id)) {
        FWK_LOG_ERR("[SENSOR_MGR] Unknown IRQ: %u", irq);
        return;
    }

    detector_ctx = &sensor_manager_ctx.sensors[type].detectors[detector_id];

    /* Store previous state */
    was_in_normal_range = detector_ctx->is_in_normal_range;
    detector_ctx->previous_value = detector_ctx->current_value;

    /* Read new sensor value */
    value = read_sensor_register(detector_ctx->config->reg_base);
    detector_ctx->current_value = value;

    /* Update current range status */
    detector_ctx->is_in_normal_range = is_value_in_normal_range(value, detector_ctx->config);

    /* Determine interrupt type based on threshold transition */
    interrupt_type = determine_interrupt_type(value, was_in_normal_range, detector_ctx->config);

    FWK_LOG_INFO(
        "[SENSOR_MGR] Sensor interrupt: type=%d, detector=%d, value=%u, interrupt_type=%s, range=%s",
        type, detector_id, value,
        (interrupt_type == SENSOR_INTERRUPT_THRESHOLD_EXCEEDED) ? "EXCEEDED" : "NORMAL",
        detector_ctx->is_in_normal_range ? "NORMAL" : "OUT_OF_RANGE");

    /* Only notify if there's a meaningful state change or if threshold monitoring is disabled */
    if (!detector_ctx->config->threshold_enabled ||
        (was_in_normal_range != detector_ctx->is_in_normal_range)) {

        /* Notify all registered modules */
        notify_registered_modules(type, detector_id, interrupt_type, value);
    } else {
        FWK_LOG_DEBUG(
            "[SENSOR_MGR] No threshold state change, skipping notification");
    }

    /* Clear interrupt (implementation depends on hardware) */
    /* *(volatile uint32_t *)(detector_ctx->config->reg_base + 0x04) = 1; */
}

/*
 * API implementation
 */

static int register_notification(
    enum sensor_type type,
    unsigned int detector_id,
    sensor_notification_callback_t callback,
    fwk_id_t requester_id)
{
    struct sensor_context *sensor_ctx;
    struct detector_context *detector_ctx;
    struct sensor_registration *reg;
    unsigned int i;
    bool is_wildcard = (detector_id == UINT_MAX);

    if (type >= SENSOR_TYPE_COUNT || callback == NULL) {
        return FWK_E_PARAM;
    }

    if (!is_wildcard && detector_id >= DETECTORS_PER_SENSOR) {
        return FWK_E_PARAM;
    }

    sensor_ctx = &sensor_manager_ctx.sensors[type];

    if (is_wildcard) {
        /* Register for all detectors of this sensor type */
        /* Check if already registered */
        for (i = 0; i < sensor_ctx->max_global_registrations; i++) {
            reg = &sensor_ctx->global_registrations[i];
            if (reg->active && fwk_id_is_equal(reg->requester_id, requester_id)) {
                FWK_LOG_WARN(
                    "[SENSOR_MGR] Module 0x%x already registered globally for sensor type %d",
                    fwk_id_get_module_idx(requester_id),
                    type);
                return FWK_E_STATE;
            }
        }

        /* Find empty slot */
        for (i = 0; i < sensor_ctx->max_global_registrations; i++) {
            reg = &sensor_ctx->global_registrations[i];
            if (!reg->active) {
                reg->callback = callback;
                reg->requester_id = requester_id;
                reg->detector_id = UINT_MAX;
                reg->active = true;
                sensor_ctx->global_active_count++;

                FWK_LOG_INFO(
                    "[SENSOR_MGR] Registered module 0x%x globally for sensor type %d",
                    fwk_id_get_module_idx(requester_id),
                    type);

                return FWK_SUCCESS;
            }
        }

        FWK_LOG_ERR(
            "[SENSOR_MGR] No more global registration slots for sensor type %d", type);
        return FWK_E_NOMEM;
    } else {
        /* Register for specific detector */
        detector_ctx = &sensor_ctx->detectors[detector_id];

        /* Check if already registered */
        for (i = 0; i < detector_ctx->max_registrations; i++) {
            reg = &detector_ctx->registrations[i];
            if (reg->active && fwk_id_is_equal(reg->requester_id, requester_id)) {
                FWK_LOG_WARN(
                    "[SENSOR_MGR] Module 0x%x already registered for sensor type %d, detector %d",
                    fwk_id_get_module_idx(requester_id),
                    type,
                    detector_id);
                return FWK_E_STATE;
            }
        }

        /* Find empty slot */
        for (i = 0; i < detector_ctx->max_registrations; i++) {
            reg = &detector_ctx->registrations[i];
            if (!reg->active) {
                reg->callback = callback;
                reg->requester_id = requester_id;
                reg->detector_id = detector_id;
                reg->active = true;
                detector_ctx->active_count++;

                FWK_LOG_INFO(
                    "[SENSOR_MGR] Registered module 0x%x for sensor type %d, detector %d",
                    fwk_id_get_module_idx(requester_id),
                    type,
                    detector_id);

                return FWK_SUCCESS;
            }
        }

        FWK_LOG_ERR(
            "[SENSOR_MGR] No more registration slots for sensor type %d, detector %d",
            type, detector_id);
        return FWK_E_NOMEM;
    }
}

static int unregister_notification(enum sensor_type type, unsigned int detector_id, fwk_id_t requester_id)
{
    struct sensor_context *sensor_ctx;
    struct detector_context *detector_ctx;
    struct sensor_registration *reg;
    unsigned int i;
    bool is_wildcard = (detector_id == UINT_MAX);

    if (type >= SENSOR_TYPE_COUNT) {
        return FWK_E_PARAM;
    }

    if (!is_wildcard && detector_id >= DETECTORS_PER_SENSOR) {
        return FWK_E_PARAM;
    }

    sensor_ctx = &sensor_manager_ctx.sensors[type];

    if (is_wildcard) {
        /* Unregister from global registrations */
        for (i = 0; i < sensor_ctx->max_global_registrations; i++) {
            reg = &sensor_ctx->global_registrations[i];
            if (reg->active && fwk_id_is_equal(reg->requester_id, requester_id)) {
                reg->active = false;
                reg->callback = NULL;
                reg->requester_id = FWK_ID_NONE;
                reg->detector_id = 0;
                sensor_ctx->global_active_count--;

                FWK_LOG_INFO(
                    "[SENSOR_MGR] Unregistered module 0x%x globally from sensor type %d",
                    fwk_id_get_module_idx(requester_id),
                    type);

                return FWK_SUCCESS;
            }
        }
    } else {
        /* Unregister from specific detector */
        detector_ctx = &sensor_ctx->detectors[detector_id];

        for (i = 0; i < detector_ctx->max_registrations; i++) {
            reg = &detector_ctx->registrations[i];
            if (reg->active && fwk_id_is_equal(reg->requester_id, requester_id)) {
                reg->active = false;
                reg->callback = NULL;
                reg->requester_id = FWK_ID_NONE;
                reg->detector_id = 0;
                detector_ctx->active_count--;

                FWK_LOG_INFO(
                    "[SENSOR_MGR] Unregistered module 0x%x from sensor type %d, detector %d",
                    fwk_id_get_module_idx(requester_id),
                    type,
                    detector_id);

                return FWK_SUCCESS;
            }
        }
    }

    FWK_LOG_WARN(
        "[SENSOR_MGR] Module 0x%x not found in registrations for sensor type %d, detector %d",
        fwk_id_get_module_idx(requester_id),
        type,
        detector_id);

    return FWK_E_ACCESS;
}

static int get_sensor_value(enum sensor_type type, unsigned int detector_id, uint32_t *value)
{
    struct detector_context *detector_ctx;

    if (type >= SENSOR_TYPE_COUNT || detector_id >= DETECTORS_PER_SENSOR || value == NULL) {
        return FWK_E_PARAM;
    }

    detector_ctx = &sensor_manager_ctx.sensors[type].detectors[detector_id];

    if (!detector_ctx->enabled) {
        return FWK_E_DEVICE;
    }

    *value = detector_ctx->current_value;

    return FWK_SUCCESS;
}

static const struct mod_sensor_manager_notification_api notification_api = {
    .register_notification = register_notification,
    .unregister_notification = unregister_notification,
    .get_sensor_value = get_sensor_value,
};

/*
 * Framework handlers
 */

static int sensor_manager_init(
    fwk_id_t module_id,
    unsigned int element_count,
    const void *config)
{
    struct sensor_context *sensor_ctx;
    struct detector_context *detector_ctx;
    const struct detector_config *detector_configs;
    enum sensor_type type;
    unsigned int detector_id;

    if (config == NULL) {
        return FWK_E_PARAM;
    }

    sensor_manager_ctx.config = config;

    /* Initialize sensor contexts */
    for (type = 0; type < SENSOR_TYPE_COUNT; type++) {
        sensor_ctx = &sensor_manager_ctx.sensors[type];

        /* Initialize global registrations */
        sensor_ctx->max_global_registrations =
            sensor_manager_ctx.config->max_registrations_per_detector;
        sensor_ctx->global_active_count = 0;

        /* Allocate global registration array */
        sensor_ctx->global_registrations = fwk_mm_calloc(
            sensor_ctx->max_global_registrations,
            sizeof(struct sensor_registration));

        if (sensor_ctx->global_registrations == NULL) {
            return FWK_E_NOMEM;
        }

        /* Get detector configurations based on sensor type */
        switch (type) {
        case SENSOR_TYPE_TEMPERATURE:
            detector_configs = sensor_manager_ctx.config->temp_detectors;
            break;
        case SENSOR_TYPE_VOLTAGE:
            detector_configs = sensor_manager_ctx.config->voltage_detectors;
            break;
        case SENSOR_TYPE_FREQUENCY:
            detector_configs = sensor_manager_ctx.config->freq_detectors;
            break;
        default:
            return FWK_E_PARAM;
        }

        /* Initialize each detector */
        for (detector_id = 0; detector_id < DETECTORS_PER_SENSOR; detector_id++) {
            detector_ctx = &sensor_ctx->detectors[detector_id];
            detector_ctx->config = &detector_configs[detector_id];
            detector_ctx->enabled = detector_ctx->config->enabled;
            detector_ctx->current_value = 0;
            detector_ctx->previous_value = 0;
            detector_ctx->active_count = 0;
            detector_ctx->max_registrations =
                sensor_manager_ctx.config->max_registrations_per_detector;

            /* Initialize threshold state - assume starting in normal range */
            detector_ctx->is_in_normal_range = true;

            /* Allocate registration array for this detector */
            detector_ctx->registrations = fwk_mm_calloc(
                detector_ctx->max_registrations,
                sizeof(struct sensor_registration));

            if (detector_ctx->registrations == NULL) {
                return FWK_E_NOMEM;
            }

            FWK_LOG_INFO(
                "[SENSOR_MGR] Initialized sensor type %d, detector %d (enabled: %s, threshold: %s, range: %u-%u)",
                type, detector_id,
                detector_ctx->enabled ? "yes" : "no",
                detector_ctx->config->threshold_enabled ? "enabled" : "disabled",
                detector_ctx->config->threshold_low,
                detector_ctx->config->threshold_high);
        }
    }

    return FWK_SUCCESS;
}

static int sensor_manager_start(fwk_id_t id)
{
    struct sensor_context *sensor_ctx;
    struct detector_context *detector_ctx;
    enum sensor_type type;
    unsigned int detector_id;
    int status;

    /* Set up interrupt handlers for each detector */
    for (type = 0; type < SENSOR_TYPE_COUNT; type++) {
        sensor_ctx = &sensor_manager_ctx.sensors[type];

        for (detector_id = 0; detector_id < DETECTORS_PER_SENSOR; detector_id++) {
            detector_ctx = &sensor_ctx->detectors[detector_id];

            if (!detector_ctx->enabled) {
                FWK_LOG_INFO(
                    "[SENSOR_MGR] Skipping disabled detector: type %d, detector %d",
                    type, detector_id);
                continue;
            }

            status = fwk_interrupt_set_isr(
                detector_ctx->config->irq, sensor_interrupt_handler);
            if (status != FWK_SUCCESS) {
                FWK_LOG_ERR(
                    "[SENSOR_MGR] Failed to set ISR for sensor type %d, detector %d, IRQ %u",
                    type, detector_id, detector_ctx->config->irq);
                return status;
            }

            /* Enable interrupt */
            status = fwk_interrupt_enable(detector_ctx->config->irq);
            if (status != FWK_SUCCESS) {
                FWK_LOG_ERR(
                    "[SENSOR_MGR] Failed to enable IRQ %u for sensor type %d, detector %d",
                    detector_ctx->config->irq, type, detector_id);
                return status;
            }

            FWK_LOG_INFO(
                "[SENSOR_MGR] Sensor type %d, detector %d initialized with IRQ %u",
                type, detector_id, detector_ctx->config->irq);
        }
    }

    return FWK_SUCCESS;
}

static int sensor_manager_process_bind_request(
    fwk_id_t source_id,
    fwk_id_t target_id,
    fwk_id_t api_id,
    const void **api)
{
    enum mod_sensor_manager_api_idx api_idx;

    /* Only allow binding to the module */
    if (!fwk_id_is_type(target_id, FWK_ID_TYPE_MODULE)) {
        FWK_LOG_ERR(
            "[SENSOR_MGR] Binding only allowed to module, not elements");
        return FWK_E_ACCESS;
    }

    api_idx = (enum mod_sensor_manager_api_idx)fwk_id_get_api_idx(api_id);

    switch (api_idx) {
    case MOD_SENSOR_MANAGER_API_IDX_NOTIFICATION:
        *api = &notification_api;
        FWK_LOG_INFO(
            "[SENSOR_MGR] Module 0x%x bound to notification API",
            fwk_id_get_module_idx(source_id));
        break;

    default:
        FWK_LOG_ERR(
            "[SENSOR_MGR] Invalid API index: %d", api_idx);
        return FWK_E_PARAM;
    }

    return FWK_SUCCESS;
}

/* Module definition */
const struct fwk_module module_sensor_manager = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .api_count = MOD_SENSOR_MANAGER_API_IDX_COUNT,
    .init = sensor_manager_init,
    .start = sensor_manager_start,
    .process_bind_request = sensor_manager_process_bind_request,
};
