# Sensor Manager Module

## Overview

The Sensor Manager module provides notification services for temperature, voltage, and frequency sensors. Each sensor type supports multiple detectors (currently 2 detectors per sensor type) to monitor different locations or sources.

## Features

- **Multiple Detectors**: Each sensor type (temperature, voltage, frequency) supports 2 detectors
- **Flexible Registration**: Modules can register for notifications from specific detectors or all detectors
- **Independent Configuration**: Each detector can be independently enabled/disabled and configured
- **Interrupt-driven**: Each detector can have its own interrupt source
- **Threshold Monitoring**: Each detector supports configurable threshold ranges with two interrupt types:
  - **THRESHOLD_EXCEEDED**: Triggered when sensor value goes outside the normal range
  - **THRESHOLD_NORMAL**: Triggered when sensor value returns to the normal range
- **Notification System**: Registered modules receive callbacks with threshold status information

## Architecture

```
Sensor Manager
├── Temperature Sensor
│   ├── Detector 0 (e.g., CPU core)
│   └── Detector 1 (e.g., GPU)
├── Voltage Sensor
│   ├── Detector 0 (e.g., Main power rail)
│   └── Detector 1 (e.g., Memory power rail)
└── Frequency Sensor
    ├── Detector 0 (e.g., CPU frequency)
    └── Detector 1 (e.g., Bus frequency)
```

## Configuration

### Detector Configuration

Each detector is configured with:
- `reg_base`: Hardware register base address
- `irq`: Interrupt number
- `enabled`: Whether the detector is active
- `threshold_low`: Minimum value for normal range
- `threshold_high`: Maximum value for normal range
- `threshold_enabled`: Whether threshold monitoring is active

### Module Configuration

```c
const struct mod_sensor_manager_config sensor_manager_config = {
    .temp_detectors = {
        [0] = {
            .reg_base = 0x50000000, .irq = 32, .enabled = true,
            .threshold_low = 10, .threshold_high = 85, .threshold_enabled = true
        },
        [1] = {
            .reg_base = 0x50001000, .irq = 33, .enabled = true,
            .threshold_low = 15, .threshold_high = 90, .threshold_enabled = true
        },
    },
    .voltage_detectors = {
        [0] = {
            .reg_base = 0x50002000, .irq = 34, .enabled = true,
            .threshold_low = 3000, .threshold_high = 3600, .threshold_enabled = true
        },
        [1] = {
            .reg_base = 0x50003000, .irq = 35, .enabled = true,
            .threshold_low = 1100, .threshold_high = 1300, .threshold_enabled = true
        },
    },
    .freq_detectors = {
        [0] = {
            .reg_base = 0x50004000, .irq = 36, .enabled = true,
            .threshold_low = 100, .threshold_high = 2400, .threshold_enabled = true
        },
        [1] = {
            .reg_base = 0x50005000, .irq = 37, .enabled = false,
            .threshold_enabled = false
        },
    },
    .max_registrations_per_detector = 5,
};
```

## API Usage

### Registration for Notifications

#### Register for All Detectors (Wildcard)
```c
status = sensor_api->register_notification(
    SENSOR_TYPE_TEMPERATURE,
    UINT_MAX,  // All detectors
    temperature_callback,
    module_id);
```

#### Register for Specific Detector
```c
status = sensor_api->register_notification(
    SENSOR_TYPE_VOLTAGE,
    0,  // Detector 0 only
    voltage_callback,
    module_id);
```

### Callback Function

```c
static void temperature_callback(
    enum sensor_type type,
    unsigned int detector_id,
    enum sensor_interrupt_type interrupt_type,
    uint32_t value,
    fwk_id_t source_id)
{
    if (interrupt_type == SENSOR_INTERRUPT_THRESHOLD_EXCEEDED) {
        FWK_LOG_WARN("Temperature detector %d exceeded threshold: %u°C", detector_id, value);
        /* Take protective action */
    } else {
        FWK_LOG_INFO("Temperature detector %d returned to normal: %u°C", detector_id, value);
        /* Resume normal operation */
    }
}
```

### Reading Sensor Values

```c
uint32_t temp_value;
status = sensor_api->get_sensor_value(
    SENSOR_TYPE_TEMPERATURE,
    0,  // Detector ID
    &temp_value);
```

### Unregistration

```c
status = sensor_api->unregister_notification(
    SENSOR_TYPE_TEMPERATURE,
    UINT_MAX,  // All detectors or specific detector ID
    module_id);
```

## Example Client

See `module/example_client/src/example_client.c` for a complete example of how to use the sensor manager API.

## Threshold Monitoring

### How It Works

Each detector can be configured with threshold monitoring:

1. **Normal Range**: Defined by `threshold_low` and `threshold_high` values
2. **State Tracking**: The system tracks whether each detector is currently in normal range
3. **Interrupt Types**:
   - `SENSOR_INTERRUPT_THRESHOLD_EXCEEDED`: Triggered when value transitions from normal to out-of-range
   - `SENSOR_INTERRUPT_THRESHOLD_NORMAL`: Triggered when value transitions from out-of-range to normal

### State Transitions

```
Normal Range (threshold_low ≤ value ≤ threshold_high)
    ↓ (value goes outside range)
THRESHOLD_EXCEEDED interrupt
    ↓ (value returns to range)
THRESHOLD_NORMAL interrupt
    ↓ (back to normal state)
Normal Range
```

### Example Scenarios

**Temperature Monitoring:**
- Normal range: 10°C - 85°C
- Current: 75°C (normal) → 95°C → `THRESHOLD_EXCEEDED` interrupt
- Later: 95°C → 80°C → `THRESHOLD_NORMAL` interrupt

**Voltage Monitoring:**
- Normal range: 3.0V - 3.6V
- Current: 3.3V (normal) → 2.8V → `THRESHOLD_EXCEEDED` interrupt
- Later: 2.8V → 3.2V → `THRESHOLD_NORMAL` interrupt

## Key Changes from Previous Version

1. **Multiple Detectors**: Each sensor type now supports 2 detectors instead of 1
2. **Enhanced API**: All API functions now include detector_id parameter
3. **Wildcard Support**: Use UINT_MAX as detector_id to register for all detectors
4. **Independent Configuration**: Each detector has its own configuration
5. **Threshold Support**: Added configurable threshold monitoring with two interrupt types
6. **Enhanced Callbacks**: Callbacks now receive detector_id and interrupt_type information

## Migration Guide

If you're migrating from the previous single-detector version:

1. Update callback functions to include `detector_id` parameter
2. Update API calls to include `detector_id` parameter
3. Update configuration to use new detector array format
4. Consider whether you need specific detector registration or wildcard registration
