/*
 * Arm SCP/MCP Software
 * Copyright (c) 2024, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *      Sensor Manager Module - Provides notification service for temperature,
 *      voltage, and frequency sensors.
 */

#ifndef MOD_SENSOR_MANAGER_H
#define MOD_SENSOR_MANAGER_H

#include <fwk_id.h>
#include <fwk_macros.h>

#include <stdint.h>

/*!
 * \addtogroup GroupModules Modules
 * \{
 */

/*!
 * \defgroup GroupSensorManager Sensor Manager
 * \{
 */

/*!
 * \brief Sensor types supported by the manager
 */
enum sensor_type {
    SENSOR_TYPE_TEMPERATURE = 0,
    SENSOR_TYPE_VOLTAGE,
    SENSOR_TYPE_FREQUENCY,
    SENSOR_TYPE_COUNT
};

/*!
 * \brief Number of detectors per sensor type
 */
#define DETECTORS_PER_SENSOR 2

/*!
 * \brief Sensor interrupt types
 */
enum sensor_interrupt_type {
    /*! Sensor value exceeded threshold range */
    SENSOR_INTERRUPT_THRESHOLD_EXCEEDED = 0,
    /*! Sensor value returned to normal range */
    SENSOR_INTERRUPT_THRESHOLD_NORMAL,
    /*! Number of interrupt types */
    SENSOR_INTERRUPT_TYPE_COUNT
};

/*!
 * \brief Detector configuration structure
 */
struct detector_config {
    /*! Register base address for this detector */
    uintptr_t reg_base;
    /*! IRQ number for this detector */
    unsigned int irq;
    /*! Enable/disable this detector */
    bool enabled;
    /*! Low threshold value (minimum normal range) */
    uint32_t threshold_low;
    /*! High threshold value (maximum normal range) */
    uint32_t threshold_high;
    /*! Enable threshold monitoring */
    bool threshold_enabled;
};

/*!
 * \brief Sensor Manager API indices
 */
enum mod_sensor_manager_api_idx {
    /*! Notification registration API */
    MOD_SENSOR_MANAGER_API_IDX_NOTIFICATION,
    /*! Number of APIs */
    MOD_SENSOR_MANAGER_API_IDX_COUNT,
};

/*!
 * \brief Sensor notification callback function type
 *
 * \param type The type of sensor that triggered the notification
 * \param detector_id The ID of the detector (0 or 1)
 * \param interrupt_type The type of interrupt (threshold exceeded or normal)
 * \param value The sensor reading value
 * \param source_id The ID of the requesting module
 */
typedef void (*sensor_notification_callback_t)(
    enum sensor_type type,
    unsigned int detector_id,
    enum sensor_interrupt_type interrupt_type,
    uint32_t value,
    fwk_id_t source_id
);

/*!
 * \brief Sensor Manager notification API
 */
struct mod_sensor_manager_notification_api {
    /*!
     * \brief Register for sensor notifications from a specific detector
     *
     * \param type The sensor type to register for
     * \param detector_id The specific detector ID (use UINT_MAX for all detectors)
     * \param callback The callback function to be called on notification
     * \param requester_id The ID of the requesting module
     *
     * \retval ::FWK_SUCCESS The registration was successful
     * \retval ::FWK_E_PARAM Invalid parameters
     * \retval ::FWK_E_NOMEM No memory available for registration
     */
    int (*register_notification)(
        enum sensor_type type,
        unsigned int detector_id,
        sensor_notification_callback_t callback,
        fwk_id_t requester_id
    );

    /*!
     * \brief Unregister from sensor notifications
     *
     * \param type The sensor type to unregister from
     * \param detector_id The specific detector ID (use UINT_MAX for all detectors)
     * \param requester_id The ID of the requesting module
     *
     * \retval ::FWK_SUCCESS The unregistration was successful
     * \retval ::FWK_E_PARAM Invalid parameters
     * \retval ::FWK_E_ACCESS No registration found for this module
     */
    int (*unregister_notification)(
        enum sensor_type type,
        unsigned int detector_id,
        fwk_id_t requester_id
    );

    /*!
     * \brief Get current sensor value from a specific detector
     *
     * \param type The sensor type to read
     * \param detector_id The specific detector ID
     * \param[out] value Pointer to store the sensor value
     *
     * \retval ::FWK_SUCCESS The read was successful
     * \retval ::FWK_E_PARAM Invalid parameters
     * \retval ::FWK_E_DEVICE Sensor not available
     */
    int (*get_sensor_value)(
        enum sensor_type type,
        unsigned int detector_id,
        uint32_t *value
    );


};

/*!
 * \brief Sensor Manager module configuration
 */
struct mod_sensor_manager_config {
    /*! Temperature detector configurations (2 detectors) */
    struct detector_config temp_detectors[DETECTORS_PER_SENSOR];

    /*! Voltage detector configurations (2 detectors) */
    struct detector_config voltage_detectors[DETECTORS_PER_SENSOR];

    /*! Frequency detector configurations (2 detectors) */
    struct detector_config freq_detectors[DETECTORS_PER_SENSOR];

    /*! Maximum number of registrations per detector */
    unsigned int max_registrations_per_detector;
};

/*!
 * \}
 */

/*!
 * \}
 */

#endif /* MOD_SENSOR_MANAGER_H */
