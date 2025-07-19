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
    /*! Whether this entry is active */
    bool active;
};

/*!
 * \brief Sensor context for each sensor type
 */
struct sensor_context {
    /*! Array of registrations for this sensor */
    struct sensor_registration *registrations;
    /*! Number of active registrations */
    unsigned int active_count;
    /*! Maximum registrations allowed */
    unsigned int max_registrations;
    /*! Current sensor value */
    uint32_t current_value;
    /*! Register base address */
    uintptr_t reg_base;
    /*! IRQ number */
    unsigned int irq;
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

static enum sensor_type get_sensor_type_from_irq(unsigned int irq)
{
    if (irq == sensor_manager_ctx.config->temp_irq) {
        return SENSOR_TYPE_TEMPERATURE;
    } else if (irq == sensor_manager_ctx.config->voltage_irq) {
        return SENSOR_TYPE_VOLTAGE;
    } else if (irq == sensor_manager_ctx.config->freq_irq) {
        return SENSOR_TYPE_FREQUENCY;
    }
    
    return SENSOR_TYPE_COUNT; /* Invalid */
}

static void notify_registered_modules(enum sensor_type type, uint32_t value)
{
    struct sensor_context *sensor_ctx = &sensor_manager_ctx.sensors[type];
    struct sensor_registration *reg;
    unsigned int i;

    for (i = 0; i < sensor_ctx->max_registrations; i++) {
        reg = &sensor_ctx->registrations[i];
        if (reg->active && reg->callback != NULL) {
            FWK_LOG_DEBUG(
                "[SENSOR_MGR] Notifying module 0x%x for sensor type %d, value: %u",
                fwk_id_get_module_idx(reg->requester_id),
                type,
                value);
            
            reg->callback(type, value, reg->requester_id);
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
    uint32_t value;
    struct sensor_context *sensor_ctx;

    /* Get the current IRQ number */
    irq = fwk_interrupt_get_current();
    
    /* Determine sensor type from IRQ */
    type = get_sensor_type_from_irq(irq);
    if (type >= SENSOR_TYPE_COUNT) {
        FWK_LOG_ERR("[SENSOR_MGR] Unknown IRQ: %u", irq);
        return;
    }

    sensor_ctx = &sensor_manager_ctx.sensors[type];
    
    /* Read sensor value */
    value = read_sensor_register(sensor_ctx->reg_base);
    sensor_ctx->current_value = value;

    FWK_LOG_INFO(
        "[SENSOR_MGR] Sensor interrupt: type=%d, value=%u", type, value);

    /* Notify all registered modules */
    notify_registered_modules(type, value);

    /* Clear interrupt (implementation depends on hardware) */
    /* *(volatile uint32_t *)(sensor_ctx->reg_base + 0x04) = 1; */
}

/*
 * API implementation
 */

static int register_notification(
    enum sensor_type type,
    sensor_notification_callback_t callback,
    fwk_id_t requester_id)
{
    struct sensor_context *sensor_ctx;
    struct sensor_registration *reg;
    unsigned int i;

    if (type >= SENSOR_TYPE_COUNT || callback == NULL) {
        return FWK_E_PARAM;
    }

    sensor_ctx = &sensor_manager_ctx.sensors[type];

    /* Check if already registered */
    for (i = 0; i < sensor_ctx->max_registrations; i++) {
        reg = &sensor_ctx->registrations[i];
        if (reg->active && fwk_id_is_equal(reg->requester_id, requester_id)) {
            FWK_LOG_WARN(
                "[SENSOR_MGR] Module 0x%x already registered for sensor type %d",
                fwk_id_get_module_idx(requester_id),
                type);
            return FWK_E_STATE;
        }
    }

    /* Find empty slot */
    for (i = 0; i < sensor_ctx->max_registrations; i++) {
        reg = &sensor_ctx->registrations[i];
        if (!reg->active) {
            reg->callback = callback;
            reg->requester_id = requester_id;
            reg->active = true;
            sensor_ctx->active_count++;
            
            FWK_LOG_INFO(
                "[SENSOR_MGR] Registered module 0x%x for sensor type %d",
                fwk_id_get_module_idx(requester_id),
                type);
            
            return FWK_SUCCESS;
        }
    }

    FWK_LOG_ERR(
        "[SENSOR_MGR] No more registration slots for sensor type %d", type);
    return FWK_E_NOMEM;
}

static int unregister_notification(enum sensor_type type, fwk_id_t requester_id)
{
    struct sensor_context *sensor_ctx;
    struct sensor_registration *reg;
    unsigned int i;

    if (type >= SENSOR_TYPE_COUNT) {
        return FWK_E_PARAM;
    }

    sensor_ctx = &sensor_manager_ctx.sensors[type];

    /* Find and remove registration */
    for (i = 0; i < sensor_ctx->max_registrations; i++) {
        reg = &sensor_ctx->registrations[i];
        if (reg->active && fwk_id_is_equal(reg->requester_id, requester_id)) {
            reg->active = false;
            reg->callback = NULL;
            reg->requester_id = FWK_ID_NONE;
            sensor_ctx->active_count--;
            
            FWK_LOG_INFO(
                "[SENSOR_MGR] Unregistered module 0x%x from sensor type %d",
                fwk_id_get_module_idx(requester_id),
                type);
            
            return FWK_SUCCESS;
        }
    }

    FWK_LOG_WARN(
        "[SENSOR_MGR] Module 0x%x not found in registrations for sensor type %d",
        fwk_id_get_module_idx(requester_id),
        type);
    
    return FWK_E_ACCESS;
}

static int get_sensor_value(enum sensor_type type, uint32_t *value)
{
    struct sensor_context *sensor_ctx;

    if (type >= SENSOR_TYPE_COUNT || value == NULL) {
        return FWK_E_PARAM;
    }

    sensor_ctx = &sensor_manager_ctx.sensors[type];
    *value = sensor_ctx->current_value;

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
    enum sensor_type type;

    if (config == NULL) {
        return FWK_E_PARAM;
    }

    sensor_manager_ctx.config = config;

    /* Initialize sensor contexts */
    for (type = 0; type < SENSOR_TYPE_COUNT; type++) {
        sensor_ctx = &sensor_manager_ctx.sensors[type];
        sensor_ctx->max_registrations = 
            sensor_manager_ctx.config->max_registrations_per_type;
        sensor_ctx->active_count = 0;
        sensor_ctx->current_value = 0;

        /* Allocate registration array */
        sensor_ctx->registrations = fwk_mm_calloc(
            sensor_ctx->max_registrations,
            sizeof(struct sensor_registration));
        
        if (sensor_ctx->registrations == NULL) {
            return FWK_E_NOMEM;
        }

        /* Set register base and IRQ based on sensor type */
        switch (type) {
        case SENSOR_TYPE_TEMPERATURE:
            sensor_ctx->reg_base = sensor_manager_ctx.config->temp_reg_base;
            sensor_ctx->irq = sensor_manager_ctx.config->temp_irq;
            break;
        case SENSOR_TYPE_VOLTAGE:
            sensor_ctx->reg_base = sensor_manager_ctx.config->voltage_reg_base;
            sensor_ctx->irq = sensor_manager_ctx.config->voltage_irq;
            break;
        case SENSOR_TYPE_FREQUENCY:
            sensor_ctx->reg_base = sensor_manager_ctx.config->freq_reg_base;
            sensor_ctx->irq = sensor_manager_ctx.config->freq_irq;
            break;
        default:
            return FWK_E_PARAM;
        }
    }

    return FWK_SUCCESS;
}

static int sensor_manager_start(fwk_id_t id)
{
    struct sensor_context *sensor_ctx;
    enum sensor_type type;
    int status;

    /* Set up interrupt handlers for each sensor */
    for (type = 0; type < SENSOR_TYPE_COUNT; type++) {
        sensor_ctx = &sensor_manager_ctx.sensors[type];

        status = fwk_interrupt_set_isr(
            sensor_ctx->irq, sensor_interrupt_handler);
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR(
                "[SENSOR_MGR] Failed to set ISR for sensor type %d, IRQ %u",
                type, sensor_ctx->irq);
            return status;
        }

        /* Enable interrupt */
        status = fwk_interrupt_enable(sensor_ctx->irq);
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR(
                "[SENSOR_MGR] Failed to enable IRQ %u for sensor type %d",
                sensor_ctx->irq, type);
            return status;
        }

        FWK_LOG_INFO(
            "[SENSOR_MGR] Sensor type %d initialized with IRQ %u",
            type, sensor_ctx->irq);
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
