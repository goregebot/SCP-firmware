/*
 * Arm SCP/MCP Software
 * Copyright (c) 2024, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     Example configuration for MPLL Clock module
 */

#include <mod_mpll_clock.h>
#include <mod_timer.h>

#include <fwk_element.h>
#include <fwk_id.h>
#include <fwk_module.h>
#include <fwk_module_idx.h>

/* Example memory map definitions */
#define MPLL_BASE_ADDR          0x50000000UL
#define MPLL_CTRL_REG_OFFSET    0x00
#define MPLL_STATUS_REG_OFFSET  0x04
#define MPLL_FREQ_REG_OFFSET    0x08

/* Example clock indices */
enum mpll_clock_idx {
    MPLL_CLOCK_IDX_MAIN,
    MPLL_CLOCK_IDX_SECONDARY,
    MPLL_CLOCK_IDX_COUNT,
};

/*
 * MPLL Clock element configuration
 */
static const struct fwk_element mpll_clock_element_table[] = {
    [MPLL_CLOCK_IDX_MAIN] = {
        .name = "MAIN_MPLL",
        .data = &(struct mod_mpll_clock_config) {
            .reg_base = MPLL_BASE_ADDR,
            .control_reg_offset = MPLL_CTRL_REG_OFFSET,
            .status_reg_offset = MPLL_STATUS_REG_OFFSET,
            .freq_reg_offset = MPLL_FREQ_REG_OFFSET,
            .default_width = MOD_MPLL_CLOCK_REG_WIDTH_32BIT,
            .ref_frequency = 24000000ULL,  /* 24 MHz reference */
            .min_frequency = 100000000ULL, /* 100 MHz minimum */
            .max_frequency = 2000000000ULL, /* 2 GHz maximum */
            .lock_timeout_us = 1000,       /* 1ms timeout */
            .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
        },
    },
    [MPLL_CLOCK_IDX_SECONDARY] = {
        .name = "SECONDARY_MPLL",
        .data = &(struct mod_mpll_clock_config) {
            .reg_base = MPLL_BASE_ADDR + 0x100,
            .control_reg_offset = MPLL_CTRL_REG_OFFSET,
            .status_reg_offset = MPLL_STATUS_REG_OFFSET,
            .freq_reg_offset = MPLL_FREQ_REG_OFFSET,
            .default_width = MOD_MPLL_CLOCK_REG_WIDTH_8BIT,
            .ref_frequency = 24000000ULL,  /* 24 MHz reference */
            .min_frequency = 50000000ULL,  /* 50 MHz minimum */
            .max_frequency = 1000000000ULL, /* 1 GHz maximum */
            .lock_timeout_us = 2000,       /* 2ms timeout */
            .timer_id = FWK_ID_ELEMENT_INIT(FWK_MODULE_IDX_TIMER, 0),
        },
    },
    [MPLL_CLOCK_IDX_COUNT] = { 0 }, /* Termination entry */
};

static const struct fwk_element *mpll_clock_get_element_table(fwk_id_t module_id)
{
    return mpll_clock_element_table;
}

/*
 * MPLL Clock module configuration
 */
const struct fwk_module_config config_mpll_clock = {
    .elements = FWK_MODULE_DYNAMIC_ELEMENTS(mpll_clock_get_element_table),
};

/*
 * Example usage functions
 */

/* Example function to set MPLL frequency */
int example_set_mpll_frequency(fwk_id_t mpll_id, uint64_t target_freq)
{
    const struct mod_mpll_clock_api *mpll_api;
    struct mod_mpll_clock_freq_config freq_config;
    int status;

    /* Bind to MPLL clock API */
    status = fwk_module_bind(mpll_id,
                             MOD_MPLL_CLOCK_API_ID_DRIVER,
                             &mpll_api);
    if (status != FWK_SUCCESS) {
        return status;
    }

    /* Calculate PLL parameters for target frequency */
    /* This is a simplified example - real implementation would need
     * proper PLL parameter calculation */
    freq_config.frequency = target_freq;
    freq_config.multiplier = (uint32_t)(target_freq / 24000000ULL);
    freq_config.divider = 1;
    freq_config.post_divider = 1;

    /* Set the MPLL frequency */
    return mpll_api->setmpll(mpll_id, &freq_config);
}

/* Example function to read MPLL frequency */
int example_get_mpll_frequency(fwk_id_t mpll_id, uint64_t *current_freq)
{
    const struct mod_mpll_clock_api *mpll_api;
    struct mod_mpll_clock_freq_config freq_config;
    int status;

    /* Bind to MPLL clock API */
    status = fwk_module_bind(mpll_id,
                             MOD_MPLL_CLOCK_API_ID_DRIVER,
                             &mpll_api);
    if (status != FWK_SUCCESS) {
        return status;
    }

    /* Get current MPLL configuration */
    status = mpll_api->getmpll(mpll_id, &freq_config);
    if (status == FWK_SUCCESS) {
        *current_freq = freq_config.frequency;
    }

    return status;
}

/* Example function to read/write registers directly */
int example_mpll_register_access(fwk_id_t mpll_id)
{
    const struct mod_mpll_clock_api *mpll_api;
    struct mod_mpll_clock_reg_params reg_params;
    uint32_t reg_value;
    int status;

    /* Bind to MPLL clock API */
    status = fwk_module_bind(mpll_id,
                             MOD_MPLL_CLOCK_API_ID_DRIVER,
                             &mpll_api);
    if (status != FWK_SUCCESS) {
        return status;
    }

    /* Example: Read control register (32-bit access) */
    reg_params.offset = MPLL_CTRL_REG_OFFSET;
    reg_params.width = MOD_MPLL_CLOCK_REG_WIDTH_32BIT;
    
    status = mpll_api->read_reg(mpll_id, &reg_params, &reg_value);
    if (status != FWK_SUCCESS) {
        return status;
    }

    /* Example: Write to control register with mask (byte access) */
    reg_params.offset = MPLL_CTRL_REG_OFFSET;
    reg_params.width = MOD_MPLL_CLOCK_REG_WIDTH_8BIT;
    reg_params.value = 0x01;  /* Enable bit */
    reg_params.mask = 0x01;   /* Only modify enable bit */
    
    return mpll_api->write_reg(mpll_id, &reg_params);
}
