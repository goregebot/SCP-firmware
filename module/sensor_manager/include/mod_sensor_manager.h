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
 * \param value The sensor reading value
 * \param source_id The ID of the requesting module
 */
typedef void (*sensor_notification_callback_t)(
    enum sensor_type type,
    uint32_t value,
    fwk_id_t source_id
);

/*!
 * \brief Sensor Manager notification API
 */
struct mod_sensor_manager_notification_api {
    /*!
     * \brief Register for sensor notifications
     *
     * \param type The sensor type to register for
     * \param callback The callback function to be called on notification
     * \param requester_id The ID of the requesting module
     *
     * \retval ::FWK_SUCCESS The registration was successful
     * \retval ::FWK_E_PARAM Invalid parameters
     * \retval ::FWK_E_NOMEM No memory available for registration
     */
    int (*register_notification)(
        enum sensor_type type,
        sensor_notification_callback_t callback,
        fwk_id_t requester_id
    );

    /*!
     * \brief Unregister from sensor notifications
     *
     * \param type The sensor type to unregister from
     * \param requester_id The ID of the requesting module
     *
     * \retval ::FWK_SUCCESS The unregistration was successful
     * \retval ::FWK_E_PARAM Invalid parameters
     * \retval ::FWK_E_ACCESS No registration found for this module
     */
    int (*unregister_notification)(
        enum sensor_type type,
        fwk_id_t requester_id
    );

    /*!
     * \brief Get current sensor value
     *
     * \param type The sensor type to read
     * \param[out] value Pointer to store the sensor value
     *
     * \retval ::FWK_SUCCESS The read was successful
     * \retval ::FWK_E_PARAM Invalid parameters
     * \retval ::FWK_E_DEVICE Sensor not available
     */
    int (*get_sensor_value)(
        enum sensor_type type,
        uint32_t *value
    );
};

/*!
 * \brief Sensor Manager module configuration
 */
struct mod_sensor_manager_config {
    /*! Temperature sensor IRQ number */
    unsigned int temp_irq;
    
    /*! Voltage sensor IRQ number */
    unsigned int voltage_irq;
    
    /*! Frequency sensor IRQ number */
    unsigned int freq_irq;
    
    /*! Temperature sensor register base address */
    uintptr_t temp_reg_base;
    
    /*! Voltage sensor register base address */
    uintptr_t voltage_reg_base;
    
    /*! Frequency sensor register base address */
    uintptr_t freq_reg_base;
    
    /*! Maximum number of registrations per sensor type */
    unsigned int max_registrations_per_type;
};

/*!
 * \}
 */

/*!
 * \}
 */

#endif /* MOD_SENSOR_MANAGER_H */
