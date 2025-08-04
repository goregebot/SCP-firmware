# MPLL Clock Driver Module

## Overview

The MPLL Clock Driver Module provides a hardware abstraction layer for configuring Main PLL (MPLL) clock settings in SCP-firmware. This module supports both byte-level (8-bit) and word-level (32-bit) register access operations.

## Features

- **setmpll**: Configure MPLL frequency with automatic PLL lock detection
- **getmpll**: Read current MPLL frequency configuration
- **read_reg**: Direct register read with configurable access width
- **write_reg**: Direct register write with configurable access width and masking
- Support for multiple MPLL instances
- Timeout-based PLL lock detection using timer module
- Flexible register access (8-bit or 32-bit)

## API Functions

### setmpll
```c
int (*setmpll)(fwk_id_t dev_id, const struct mod_mpll_clock_freq_config *freq_config);
```
Configures the MPLL to output the specified frequency. Handles the complete PLL configuration sequence including lock detection.

### getmpll
```c
int (*getmpll)(fwk_id_t dev_id, struct mod_mpll_clock_freq_config *freq_config);
```
Reads the current MPLL configuration and calculates the output frequency based on register values.

### read_reg
```c
int (*read_reg)(fwk_id_t dev_id, const struct mod_mpll_clock_reg_params *reg_params, uint32_t *value);
```
Reads a register value using either byte (8-bit) or word (32-bit) access.

### write_reg
```c
int (*write_reg)(fwk_id_t dev_id, const struct mod_mpll_clock_reg_params *reg_params);
```
Writes a register value using either byte (8-bit) or word (32-bit) access. Supports masked writes for partial register updates.

## Configuration

### Module Configuration Structure
```c
struct mod_mpll_clock_config {
    uintptr_t reg_base;                           // Base address of MPLL registers
    uint32_t control_reg_offset;                  // Control register offset
    uint32_t status_reg_offset;                   // Status register offset  
    uint32_t freq_reg_offset;                     // Frequency register offset
    enum mod_mpll_clock_reg_width default_width;  // Default register access width
    uint64_t ref_frequency;                       // Reference frequency in Hz
    uint64_t min_frequency;                       // Minimum output frequency
    uint64_t max_frequency;                       // Maximum output frequency
    uint32_t lock_timeout_us;                     // PLL lock timeout in microseconds
};
```

### Example Configuration
```c
static const struct fwk_element mpll_clock_element_table[] = {
    [0] = {
        .name = "MAIN_MPLL",
        .data = &(struct mod_mpll_clock_config) {
            .reg_base = 0x50000000UL,
            .control_reg_offset = 0x00,
            .status_reg_offset = 0x04,
            .freq_reg_offset = 0x08,
            .default_width = MOD_MPLL_CLOCK_REG_WIDTH_32BIT,
            .ref_frequency = 24000000ULL,    // 24 MHz
            .min_frequency = 100000000ULL,   // 100 MHz
            .max_frequency = 2000000000ULL,  // 2 GHz
            .lock_timeout_us = 1000,         // 1ms
        },
    },
    [1] = { 0 }, // Termination entry
};
```

## Usage Examples

### Setting MPLL Frequency
```c
const struct mod_mpll_clock_api *mpll_api;
struct mod_mpll_clock_freq_config freq_config;

// Bind to API
fwk_module_bind(mpll_id, MOD_MPLL_CLOCK_API_ID_DRIVER, &mpll_api);

// Configure frequency
freq_config.frequency = 1000000000ULL;  // 1 GHz
freq_config.multiplier = 42;            // Calculated value
freq_config.divider = 1;
freq_config.post_divider = 1;

mpll_api->setmpll(mpll_id, &freq_config);
```

### Reading Current Frequency
```c
struct mod_mpll_clock_freq_config current_config;

mpll_api->getmpll(mpll_id, &current_config);
printf("Current frequency: %llu Hz\n", current_config.frequency);
```

### Direct Register Access
```c
struct mod_mpll_clock_reg_params reg_params;
uint32_t reg_value;

// Read 32-bit register
reg_params.offset = 0x00;
reg_params.width = MOD_MPLL_CLOCK_REG_WIDTH_32BIT;
mpll_api->read_reg(mpll_id, &reg_params, &reg_value);

// Write 8-bit register with mask
reg_params.offset = 0x00;
reg_params.width = MOD_MPLL_CLOCK_REG_WIDTH_8BIT;
reg_params.value = 0x01;
reg_params.mask = 0x01;  // Only modify bit 0
mpll_api->write_reg(mpll_id, &reg_params);
```

## Register Layout

The module expects the following register layout:

- **Control Register**: PLL enable, bypass, and reset control
- **Status Register**: PLL lock status
- **Frequency Register**: Multiplier, divider, and post-divider values

### Control Register Bits
- Bit 0: PLL Enable
- Bit 1: PLL Bypass  
- Bit 2: PLL Reset

### Status Register Bits
- Bit 0: PLL Lock Status

### Frequency Register Bits
- Bits 7:0: Multiplier
- Bits 15:8: Divider
- Bits 23:16: Post-divider

## Dependencies

- Framework core modules
- Timer module (optional, for PLL lock timeout)

## Integration

1. Add the module to your product's CMakeLists.txt
2. Include the module in your firmware configuration
3. Configure the MPLL clock elements in your config file
4. Bind to the MPLL clock API in your modules

See `example_config.c` for a complete configuration example.
