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
    unsigned int detector_id,
    enum sensor_interrupt_type interrupt_type,
    uint32_t value,
    fwk_id_t source_id)
{
    const char *interrupt_str = (interrupt_type == SENSOR_INTERRUPT_THRESHOLD_EXCEEDED) ?
        "THRESHOLD_EXCEEDED" : "THRESHOLD_NORMAL";

    FWK_LOG_INFO("[CLIENT] Temperature sensor notification: detector %d, %s, %u°C",
                 detector_id, interrupt_str, value);

    /* Handle temperature threshold changes */
    if (interrupt_type == SENSOR_INTERRUPT_THRESHOLD_EXCEEDED) {
        FWK_LOG_WARN("[CLIENT] Temperature threshold exceeded on detector %d: %u°C", detector_id, value);
        /* Take action for high temperature - e.g., throttle CPU, increase fan speed */
        FWK_LOG_INFO("[CLIENT] Activating thermal protection measures...");
    } else {
        FWK_LOG_INFO("[CLIENT] Temperature returned to normal on detector %d: %u°C", detector_id, value);
        /* Take action for normal temperature - e.g., restore normal operation */
        FWK_LOG_INFO("[CLIENT] Restoring normal operation...");
    }
}

static void voltage_notification_callback(
    enum sensor_type type,
    unsigned int detector_id,
    enum sensor_interrupt_type interrupt_type,
    uint32_t value,
    fwk_id_t source_id)
{
    const char *interrupt_str = (interrupt_type == SENSOR_INTERRUPT_THRESHOLD_EXCEEDED) ?
        "THRESHOLD_EXCEEDED" : "THRESHOLD_NORMAL";

    FWK_LOG_INFO("[CLIENT] Voltage sensor notification: detector %d, %s, %u mV",
                 detector_id, interrupt_str, value);

    /* Handle voltage threshold changes */
    if (interrupt_type == SENSOR_INTERRUPT_THRESHOLD_EXCEEDED) {
        FWK_LOG_WARN("[CLIENT] Voltage threshold exceeded on detector %d: %u mV", detector_id, value);
        /* Take action for voltage out of range - e.g., emergency shutdown, switch power source */
        FWK_LOG_WARN("[CLIENT] Initiating voltage protection protocol...");
    } else {
        FWK_LOG_INFO("[CLIENT] Voltage returned to normal on detector %d: %u mV", detector_id, value);
        /* Take action for normal voltage - e.g., resume normal operation */
        FWK_LOG_INFO("[CLIENT] Voltage stabilized, resuming normal operation...");
    }
}

static void frequency_notification_callback(
    enum sensor_type type,
    unsigned int detector_id,
    enum sensor_interrupt_type interrupt_type,
    uint32_t value,
    fwk_id_t source_id)
{
    const char *interrupt_str = (interrupt_type == SENSOR_INTERRUPT_THRESHOLD_EXCEEDED) ?
        "THRESHOLD_EXCEEDED" : "THRESHOLD_NORMAL";

    FWK_LOG_INFO("[CLIENT] Frequency sensor notification: detector %d, %s, %u MHz",
                 detector_id, interrupt_str, value);

    /* Handle frequency threshold changes */
    if (interrupt_type == SENSOR_INTERRUPT_THRESHOLD_EXCEEDED) {
        FWK_LOG_WARN("[CLIENT] Frequency threshold exceeded on detector %d: %u MHz", detector_id, value);
        /* Take action for high frequency - e.g., adjust power management, thermal throttling */
        FWK_LOG_INFO("[CLIENT] Adjusting power management for high frequency...");
    } else {
        FWK_LOG_INFO("[CLIENT] Frequency returned to normal on detector %d: %u MHz", detector_id, value);
        /* Take action for normal frequency - e.g., restore standard power settings */
        FWK_LOG_INFO("[CLIENT] Restoring standard power settings...");
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
    unsigned int detector_id;

    /* Register for temperature notifications from all detectors (wildcard) */
    status = client_ctx.sensor_api->register_notification(
        SENSOR_TYPE_TEMPERATURE,
        UINT_MAX,  /* UINT_MAX means all detectors */
        temperature_notification_callback,
        client_id);

    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[CLIENT] Failed to register for temperature notifications");
        return status;
    }

    /* Register for voltage notifications from detector 0 only */
    status = client_ctx.sensor_api->register_notification(
        SENSOR_TYPE_VOLTAGE,
        0,  /* Specific detector ID */
        voltage_notification_callback,
        client_id);

    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[CLIENT] Failed to register for voltage notifications");
        return status;
    }

    /* Register for frequency notifications from detector 1 only */
    status = client_ctx.sensor_api->register_notification(
        SENSOR_TYPE_FREQUENCY,
        1,  /* Specific detector ID */
        frequency_notification_callback,
        client_id);

    if (status != FWK_SUCCESS) {
        FWK_LOG_ERR("[CLIENT] Failed to register for frequency notifications");
        return status;
    }

    FWK_LOG_INFO("[CLIENT] Successfully registered for sensor notifications");

    /* Example: Get current sensor values from both detectors */
    uint32_t sensor_value;

    for (detector_id = 0; detector_id < DETECTORS_PER_SENSOR; detector_id++) {
        if (client_ctx.sensor_api->get_sensor_value(SENSOR_TYPE_TEMPERATURE, detector_id, &sensor_value) == FWK_SUCCESS) {
            FWK_LOG_INFO("[CLIENT] Current temperature detector %d: %u°C", detector_id, sensor_value);
        }

        if (client_ctx.sensor_api->get_sensor_value(SENSOR_TYPE_VOLTAGE, detector_id, &sensor_value) == FWK_SUCCESS) {
            FWK_LOG_INFO("[CLIENT] Current voltage detector %d: %u mV", detector_id, sensor_value);
        }

        if (client_ctx.sensor_api->get_sensor_value(SENSOR_TYPE_FREQUENCY, detector_id, &sensor_value) == FWK_SUCCESS) {
            FWK_LOG_INFO("[CLIENT] Current frequency detector %d: %u MHz", detector_id, sensor_value);
        }
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
