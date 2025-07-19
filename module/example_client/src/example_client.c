/*
 * Example client module showing how to use the sensor manager
 */

#include <mod_sensor_manager.h>

#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_module.h>
#include <fwk_status.h>

/* Client context */
static struct {
    const struct mod_sensor_manager_notification_api *sensor_api;
} client_ctx;

/*
 * Callback functions for sensor notifications
 */

static void temperature_notification_callback(
    enum sensor_type type,
    uint32_t value,
    fwk_id_t source_id)
{
    FWK_LOG_INFO("[CLIENT] Temperature sensor notification: %u°C", value);
    
    /* Handle temperature change */
    if (value > 80) {
        FWK_LOG_WARN("[CLIENT] High temperature detected: %u°C", value);
        /* Take action for high temperature */
    }
}

static void voltage_notification_callback(
    enum sensor_type type,
    uint32_t value,
    fwk_id_t source_id)
{
    FWK_LOG_INFO("[CLIENT] Voltage sensor notification: %u mV", value);
    
    /* Handle voltage change */
    if (value < 3000 || value > 3600) {
        FWK_LOG_WARN("[CLIENT] Voltage out of range: %u mV", value);
        /* Take action for voltage out of range */
    }
}

static void frequency_notification_callback(
    enum sensor_type type,
    uint32_t value,
    fwk_id_t source_id)
{
    FWK_LOG_INFO("[CLIENT] Frequency sensor notification: %u MHz", value);
    
    /* Handle frequency change */
    if (value > 2000) {
        FWK_LOG_INFO("[CLIENT] High frequency mode: %u MHz", value);
        /* Adjust system settings for high frequency */
    }
}

/*
 * Framework handlers
 */

static int client_init(fwk_id_t module_id, unsigned int element_count, const void *config)
{
    /* Module initialization */
    return FWK_SUCCESS;
}

static int client_bind(fwk_id_t id, unsigned int round)
{
    int status;

    if (round == 1) {
        return FWK_SUCCESS;
    }

    /* Bind to sensor manager notification API */
    status = fwk_module_bind(
        FWK_ID_MODULE(FWK_MODULE_IDX_SENSOR_MANAGER),
        FWK_ID_API(FWK_MODULE_IDX_SENSOR_MANAGER, MOD_SENSOR_MANAGER_API_IDX_NOTIFICATION),
        &client_ctx.sensor_api);

    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[CLIENT] Failed to bind to sensor manager API");
        return status;
    }

    return FWK_SUCCESS;
}

static int client_start(fwk_id_t id)
{
    int status;
    fwk_id_t client_id = FWK_ID_MODULE(FWK_MODULE_IDX_EXAMPLE_CLIENT);

    /* Register for temperature notifications */
    status = client_ctx.sensor_api->register_notification(
        SENSOR_TYPE_TEMPERATURE,
        temperature_notification_callback,
        client_id);

    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[CLIENT] Failed to register for temperature notifications");
        return status;
    }

    /* Register for voltage notifications */
    status = client_ctx.sensor_api->register_notification(
        SENSOR_TYPE_VOLTAGE,
        voltage_notification_callback,
        client_id);

    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[CLIENT] Failed to register for voltage notifications");
        return status;
    }

    /* Register for frequency notifications */
    status = client_ctx.sensor_api->register_notification(
        SENSOR_TYPE_FREQUENCY,
        frequency_notification_callback,
        client_id);

    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[CLIENT] Failed to register for frequency notifications");
        return status;
    }

    FWK_LOG_INFO("[CLIENT] Successfully registered for all sensor notifications");

    /* Example: Get current sensor values */
    uint32_t temp_value, voltage_value, freq_value;
    
    if (client_ctx.sensor_api->get_sensor_value(SENSOR_TYPE_TEMPERATURE, &temp_value) == FWK_SUCCESS) {
        FWK_LOG_INFO("[CLIENT] Current temperature: %u°C", temp_value);
    }
    
    if (client_ctx.sensor_api->get_sensor_value(SENSOR_TYPE_VOLTAGE, &voltage_value) == FWK_SUCCESS) {
        FWK_LOG_INFO("[CLIENT] Current voltage: %u mV", voltage_value);
    }
    
    if (client_ctx.sensor_api->get_sensor_value(SENSOR_TYPE_FREQUENCY, &freq_value) == FWK_SUCCESS) {
        FWK_LOG_INFO("[CLIENT] Current frequency: %u MHz", freq_value);
    }

    return FWK_SUCCESS;
}

/* Module definition */
const struct fwk_module module_example_client = {
    .type = FWK_MODULE_TYPE_SERVICE,
    .api_count = 0,
    .init = client_init,
    .bind = client_bind,
    .start = client_start,
};
