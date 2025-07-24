/*
 * Example configuration for sensor manager with multiple detectors
 * This file shows how to configure the sensor manager to support
 * 2 detectors per sensor type (temperature, voltage, frequency)
 */

#include <mod_sensor_manager.h>

/* Example register base addresses and IRQ numbers */
#define TEMP_DETECTOR_0_BASE    0x50000000
#define TEMP_DETECTOR_1_BASE    0x50001000
#define VOLTAGE_DETECTOR_0_BASE 0x50002000
#define VOLTAGE_DETECTOR_1_BASE 0x50003000
#define FREQ_DETECTOR_0_BASE    0x50004000
#define FREQ_DETECTOR_1_BASE    0x50005000

#define TEMP_DETECTOR_0_IRQ     32
#define TEMP_DETECTOR_1_IRQ     33
#define VOLTAGE_DETECTOR_0_IRQ  34
#define VOLTAGE_DETECTOR_1_IRQ  35
#define FREQ_DETECTOR_0_IRQ     36
#define FREQ_DETECTOR_1_IRQ     37

/* Configuration for temperature detectors */
static const struct detector_config temp_detectors[DETECTORS_PER_SENSOR] = {
    [0] = {
        .reg_base = TEMP_DETECTOR_0_BASE,
        .irq = TEMP_DETECTOR_0_IRQ,
        .enabled = true,
        .threshold_low = 10,    /* 10°C minimum */
        .threshold_high = 85,   /* 85°C maximum */
        .threshold_enabled = true,
    },
    [1] = {
        .reg_base = TEMP_DETECTOR_1_BASE,
        .irq = TEMP_DETECTOR_1_IRQ,
        .enabled = true,
        .threshold_low = 15,    /* 15°C minimum */
        .threshold_high = 90,   /* 90°C maximum */
        .threshold_enabled = true,
    },
};

/* Configuration for voltage detectors */
static const struct detector_config voltage_detectors[DETECTORS_PER_SENSOR] = {
    [0] = {
        .reg_base = VOLTAGE_DETECTOR_0_BASE,
        .irq = VOLTAGE_DETECTOR_0_IRQ,
        .enabled = true,
        .threshold_low = 3000,  /* 3.0V minimum */
        .threshold_high = 3600, /* 3.6V maximum */
        .threshold_enabled = true,
    },
    [1] = {
        .reg_base = VOLTAGE_DETECTOR_1_BASE,
        .irq = VOLTAGE_DETECTOR_1_IRQ,
        .enabled = true,
        .threshold_low = 1100,  /* 1.1V minimum */
        .threshold_high = 1300, /* 1.3V maximum */
        .threshold_enabled = true,
    },
};

/* Configuration for frequency detectors */
static const struct detector_config freq_detectors[DETECTORS_PER_SENSOR] = {
    [0] = {
        .reg_base = FREQ_DETECTOR_0_BASE,
        .irq = FREQ_DETECTOR_0_IRQ,
        .enabled = true,
        .threshold_low = 100,   /* 100 MHz minimum */
        .threshold_high = 2400, /* 2.4 GHz maximum */
        .threshold_enabled = true,
    },
    [1] = {
        .reg_base = FREQ_DETECTOR_1_BASE,
        .irq = FREQ_DETECTOR_1_IRQ,
        .enabled = false,  /* Example: disable detector 1 for frequency */
        .threshold_low = 0,
        .threshold_high = 0,
        .threshold_enabled = false,
    },
};

/* Main sensor manager configuration */
const struct mod_sensor_manager_config sensor_manager_config = {
    .temp_detectors = {
        [0] = {
            .reg_base = TEMP_DETECTOR_0_BASE,
            .irq = TEMP_DETECTOR_0_IRQ,
            .enabled = true,
            .threshold_low = 10,    /* 10°C minimum */
            .threshold_high = 85,   /* 85°C maximum */
            .threshold_enabled = true,
        },
        [1] = {
            .reg_base = TEMP_DETECTOR_1_BASE,
            .irq = TEMP_DETECTOR_1_IRQ,
            .enabled = true,
            .threshold_low = 15,    /* 15°C minimum */
            .threshold_high = 90,   /* 90°C maximum */
            .threshold_enabled = true,
        },
    },
    .voltage_detectors = {
        [0] = {
            .reg_base = VOLTAGE_DETECTOR_0_BASE,
            .irq = VOLTAGE_DETECTOR_0_IRQ,
            .enabled = true,
            .threshold_low = 3000,  /* 3.0V minimum */
            .threshold_high = 3600, /* 3.6V maximum */
            .threshold_enabled = true,
        },
        [1] = {
            .reg_base = VOLTAGE_DETECTOR_1_BASE,
            .irq = VOLTAGE_DETECTOR_1_IRQ,
            .enabled = true,
            .threshold_low = 1100,  /* 1.1V minimum */
            .threshold_high = 1300, /* 1.3V maximum */
            .threshold_enabled = true,
        },
    },
    .freq_detectors = {
        [0] = {
            .reg_base = FREQ_DETECTOR_0_BASE,
            .irq = FREQ_DETECTOR_0_IRQ,
            .enabled = true,
            .threshold_low = 100,   /* 100 MHz minimum */
            .threshold_high = 2400, /* 2.4 GHz maximum */
            .threshold_enabled = true,
        },
        [1] = {
            .reg_base = FREQ_DETECTOR_1_BASE,
            .irq = FREQ_DETECTOR_1_IRQ,
            .enabled = false,  /* Example: disable detector 1 for frequency */
            .threshold_low = 0,
            .threshold_high = 0,
            .threshold_enabled = false,
        },
    },
    .max_registrations_per_detector = 5,  /* Allow up to 5 modules to register per detector */
};

/*
 * Usage example in a product configuration:
 * 
 * In your product's firmware configuration file, you would include this
 * configuration like this:
 * 
 * extern const struct mod_sensor_manager_config sensor_manager_config;
 * 
 * static const struct fwk_element sensor_manager_element_table[] = {
 *     [0] = { 0 }, // Terminator
 * };
 * 
 * static const struct fwk_element_config sensor_manager_config_table[] = {
 *     [0] = { 0 }, // Terminator  
 * };
 * 
 * struct fwk_module_config config_sensor_manager = {
 *     .elements = FWK_MODULE_STATIC_ELEMENTS_PTR(sensor_manager_element_table),
 *     .data = &sensor_manager_config,
 * };
 */
