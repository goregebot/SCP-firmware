/*
 * Arm SCP/MCP Software
 * Copyright (c) 2024, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     MPLL Clock Driver Module Implementation
 */

#include <mod_mpll_clock.h>
#include <mod_timer.h>

#include <fwk_assert.h>
#include <fwk_id.h>
#include <fwk_log.h>
#include <fwk_mm.h>
#include <fwk_module.h>
#include <fwk_status.h>

#include <stddef.h>
#include <stdint.h>

/* MPLL Control Register Bit Definitions */
#define MPLL_CTRL_ENABLE_POS        0
#define MPLL_CTRL_ENABLE_MASK       (1U << MPLL_CTRL_ENABLE_POS)
#define MPLL_CTRL_BYPASS_POS        1
#define MPLL_CTRL_BYPASS_MASK       (1U << MPLL_CTRL_BYPASS_POS)
#define MPLL_CTRL_RESET_POS         2
#define MPLL_CTRL_RESET_MASK        (1U << MPLL_CTRL_RESET_POS)

/* MPLL Status Register Bit Definitions */
#define MPLL_STATUS_LOCK_POS        0
#define MPLL_STATUS_LOCK_MASK       (1U << MPLL_STATUS_LOCK_POS)

/* MPLL Frequency Register Bit Definitions */
#define MPLL_FREQ_MULT_POS          0
#define MPLL_FREQ_MULT_MASK         (0xFFU << MPLL_FREQ_MULT_POS)
#define MPLL_FREQ_DIV_POS           8
#define MPLL_FREQ_DIV_MASK          (0xFFU << MPLL_FREQ_DIV_POS)
#define MPLL_FREQ_POSTDIV_POS       16
#define MPLL_FREQ_POSTDIV_MASK      (0xFFU << MPLL_FREQ_POSTDIV_POS)

/*!
 * \brief Device context structure.
 */
struct mpll_clock_dev_ctx {
    /*! Device configuration */
    const struct mod_mpll_clock_config *config;

    /*! Timer API for timeout operations */
    const struct mod_timer_api *timer_api;

    /*! Timer device ID */
    fwk_id_t timer_id;

    /*! Current frequency configuration */
    struct mod_mpll_clock_freq_config current_freq;

    /*! Device initialization status */
    bool initialized;
};

/*!
 * \brief Module context structure.
 */
static struct {
    /*! Device context table */
    struct mpll_clock_dev_ctx *dev_ctx_table;

    /*! Number of devices */
    unsigned int dev_count;
} module_ctx;

/*
 * Static helper functions
 */

/*!
 * \brief Read register with specified width.
 */
static uint32_t mpll_reg_read(uintptr_t base, uint32_t offset,
                              enum mod_mpll_clock_reg_width width)
{
    volatile void *reg_addr = (volatile void *)(base + offset);

    switch (width) {
    case MOD_MPLL_CLOCK_REG_WIDTH_8BIT:
        return *(volatile uint8_t *)reg_addr;

    case MOD_MPLL_CLOCK_REG_WIDTH_32BIT:
        return *(volatile uint32_t *)reg_addr;

    default:
        fwk_unexpected();
        return 0;
    }
}

/*!
 * \brief Write register with specified width.
 */
static void mpll_reg_write(uintptr_t base, uint32_t offset,
                           uint32_t value, uint32_t mask,
                           enum mod_mpll_clock_reg_width width)
{
    volatile void *reg_addr = (volatile void *)(base + offset);
    uint32_t current_value;

    switch (width) {
    case MOD_MPLL_CLOCK_REG_WIDTH_8BIT:
        if (mask != 0xFF) {
            current_value = *(volatile uint8_t *)reg_addr;
            value = (current_value & ~mask) | (value & mask);
        }
        *(volatile uint8_t *)reg_addr = (uint8_t)value;
        break;

    case MOD_MPLL_CLOCK_REG_WIDTH_32BIT:
        if (mask != 0xFFFFFFFF) {
            current_value = *(volatile uint32_t *)reg_addr;
            value = (current_value & ~mask) | (value & mask);
        }
        *(volatile uint32_t *)reg_addr = value;
        break;

    default:
        fwk_unexpected();
        break;
    }
}

/*!
 * \brief Calculate PLL parameters for target frequency.
 */
static int calculate_pll_params(const struct mod_mpll_clock_config *config,
                                uint64_t target_freq,
                                struct mod_mpll_clock_freq_config *freq_config)
{
    uint32_t multiplier, divider, post_divider;
    uint64_t vco_freq;

    if ((target_freq < config->min_frequency) ||
        (target_freq > config->max_frequency)) {
        return FWK_E_RANGE;
    }

    /* Simple PLL calculation - can be enhanced for specific hardware */
    post_divider = 1;
    divider = 1;
    multiplier = (uint32_t)(target_freq / config->ref_frequency);

    /* Ensure multiplier is within reasonable range */
    if (multiplier < 1 || multiplier > 255) {
        return FWK_E_RANGE;
    }

    vco_freq = config->ref_frequency * multiplier / divider;
    freq_config->frequency = vco_freq / post_divider;
    freq_config->multiplier = multiplier;
    freq_config->divider = divider;
    freq_config->post_divider = post_divider;

    return FWK_SUCCESS;
}

/*!
 * \brief Wait for PLL lock with timeout.
 */
static bool mpll_wait_lock_condition(void *data)
{
    struct mpll_clock_dev_ctx *ctx = (struct mpll_clock_dev_ctx *)data;
    uint32_t status;

    status = mpll_reg_read(ctx->config->reg_base,
                           ctx->config->status_reg_offset,
                           ctx->config->default_width);

    return (status & MPLL_STATUS_LOCK_MASK) != 0;
}

/*
 * API function implementations
 */

static int mpll_clock_setmpll(fwk_id_t dev_id,
                              const struct mod_mpll_clock_freq_config *freq_config)
{
    struct mpll_clock_dev_ctx *ctx;
    uint32_t freq_reg_value;
    uint32_t ctrl_reg_value;
    int status;

    if (!fwk_module_is_valid_element_id(dev_id)) {
        return FWK_E_PARAM;
    }

    if (freq_config == NULL) {
        return FWK_E_PARAM;
    }

    ctx = &module_ctx.dev_ctx_table[fwk_id_get_element_idx(dev_id)];

    if (!ctx->initialized) {
        return FWK_E_INIT;
    }

    /* Disable PLL before configuration */
    ctrl_reg_value = mpll_reg_read(ctx->config->reg_base,
                                   ctx->config->control_reg_offset,
                                   ctx->config->default_width);
    ctrl_reg_value &= ~MPLL_CTRL_ENABLE_MASK;
    mpll_reg_write(ctx->config->reg_base,
                   ctx->config->control_reg_offset,
                   ctrl_reg_value, 0xFFFFFFFF,
                   ctx->config->default_width);

    /* Configure frequency parameters */
    freq_reg_value = ((freq_config->multiplier << MPLL_FREQ_MULT_POS) & MPLL_FREQ_MULT_MASK) |
                     ((freq_config->divider << MPLL_FREQ_DIV_POS) & MPLL_FREQ_DIV_MASK) |
                     ((freq_config->post_divider << MPLL_FREQ_POSTDIV_POS) & MPLL_FREQ_POSTDIV_MASK);

    mpll_reg_write(ctx->config->reg_base,
                   ctx->config->freq_reg_offset,
                   freq_reg_value, 0xFFFFFFFF,
                   ctx->config->default_width);

    /* Enable PLL */
    ctrl_reg_value |= MPLL_CTRL_ENABLE_MASK;
    mpll_reg_write(ctx->config->reg_base,
                   ctx->config->control_reg_offset,
                   ctrl_reg_value, 0xFFFFFFFF,
                   ctx->config->default_width);

    /* Wait for PLL lock */
    if (ctx->timer_api != NULL) {
        status = ctx->timer_api->wait(ctx->timer_id,
                                      ctx->config->lock_timeout_us,
                                      mpll_wait_lock_condition,
                                      ctx);
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR("[MPLL] PLL failed to lock within timeout");
            return FWK_E_TIMEOUT;
        }
    }

    /* Update current configuration */
    ctx->current_freq = *freq_config;

    FWK_LOG_INFO("[MPLL] PLL configured: freq=%lluHz, mult=%u, div=%u, postdiv=%u",
                 freq_config->frequency, freq_config->multiplier,
                 freq_config->divider, freq_config->post_divider);

    return FWK_SUCCESS;
}

static int mpll_clock_getmpll(fwk_id_t dev_id,
                              struct mod_mpll_clock_freq_config *freq_config)
{
    struct mpll_clock_dev_ctx *ctx;
    uint32_t freq_reg_value;

    if (!fwk_module_is_valid_element_id(dev_id)) {
        return FWK_E_PARAM;
    }

    if (freq_config == NULL) {
        return FWK_E_PARAM;
    }

    ctx = &module_ctx.dev_ctx_table[fwk_id_get_element_idx(dev_id)];

    if (!ctx->initialized) {
        return FWK_E_INIT;
    }

    /* Read current frequency register */
    freq_reg_value = mpll_reg_read(ctx->config->reg_base,
                                   ctx->config->freq_reg_offset,
                                   ctx->config->default_width);

    /* Extract PLL parameters */
    freq_config->multiplier = (freq_reg_value & MPLL_FREQ_MULT_MASK) >> MPLL_FREQ_MULT_POS;
    freq_config->divider = (freq_reg_value & MPLL_FREQ_DIV_MASK) >> MPLL_FREQ_DIV_POS;
    freq_config->post_divider = (freq_reg_value & MPLL_FREQ_POSTDIV_MASK) >> MPLL_FREQ_POSTDIV_POS;

    /* Calculate actual frequency */
    if (freq_config->divider == 0 || freq_config->post_divider == 0) {
        freq_config->frequency = 0;
    } else {
        freq_config->frequency = (ctx->config->ref_frequency * freq_config->multiplier) /
                                 (freq_config->divider * freq_config->post_divider);
    }

    return FWK_SUCCESS;
}

static int mpll_clock_read_reg(fwk_id_t dev_id,
                               const struct mod_mpll_clock_reg_params *reg_params,
                               uint32_t *value)
{
    struct mpll_clock_dev_ctx *ctx;

    if (!fwk_module_is_valid_element_id(dev_id)) {
        return FWK_E_PARAM;
    }

    if ((reg_params == NULL) || (value == NULL)) {
        return FWK_E_PARAM;
    }

    if (reg_params->width >= MOD_MPLL_CLOCK_REG_WIDTH_COUNT) {
        return FWK_E_PARAM;
    }

    ctx = &module_ctx.dev_ctx_table[fwk_id_get_element_idx(dev_id)];

    if (!ctx->initialized) {
        return FWK_E_INIT;
    }

    *value = mpll_reg_read(ctx->config->reg_base,
                           reg_params->offset,
                           reg_params->width);

    return FWK_SUCCESS;
}

static int mpll_clock_write_reg(fwk_id_t dev_id,
                                const struct mod_mpll_clock_reg_params *reg_params)
{
    struct mpll_clock_dev_ctx *ctx;

    if (!fwk_module_is_valid_element_id(dev_id)) {
        return FWK_E_PARAM;
    }

    if (reg_params == NULL) {
        return FWK_E_PARAM;
    }

    if (reg_params->width >= MOD_MPLL_CLOCK_REG_WIDTH_COUNT) {
        return FWK_E_PARAM;
    }

    ctx = &module_ctx.dev_ctx_table[fwk_id_get_element_idx(dev_id)];

    if (!ctx->initialized) {
        return FWK_E_INIT;
    }

    mpll_reg_write(ctx->config->reg_base,
                   reg_params->offset,
                   reg_params->value,
                   reg_params->mask,
                   reg_params->width);

    return FWK_SUCCESS;
}

/*
 * API structure
 */
static const struct mod_mpll_clock_api mpll_clock_api = {
    .setmpll = mpll_clock_setmpll,
    .getmpll = mpll_clock_getmpll,
    .read_reg = mpll_clock_read_reg,
    .write_reg = mpll_clock_write_reg,
};

/*
 * Framework handler functions
 */

static int mpll_clock_init(fwk_id_t module_id,
                           unsigned int element_count,
                           const void *data)
{
    if (element_count == 0) {
        return FWK_SUCCESS;
    }

    module_ctx.dev_count = element_count;
    module_ctx.dev_ctx_table = fwk_mm_calloc(element_count,
                                             sizeof(struct mpll_clock_dev_ctx));

    return FWK_SUCCESS;
}

static int mpll_clock_element_init(fwk_id_t element_id,
                                   unsigned int sub_element_count,
                                   const void *data)
{
    struct mpll_clock_dev_ctx *ctx;

    if (data == NULL) {
        return FWK_E_PARAM;
    }

    ctx = &module_ctx.dev_ctx_table[fwk_id_get_element_idx(element_id)];
    ctx->config = (const struct mod_mpll_clock_config *)data;

    /* Set timer_id from configuration */
    ctx->timer_id = ctx->config->timer_id;

    return FWK_SUCCESS;
}

static int mpll_clock_bind(fwk_id_t id, unsigned int round)
{
    struct mpll_clock_dev_ctx *ctx;
    int status;

    /* Only bind on the first round */
    if (round != 0) {
        return FWK_SUCCESS;
    }

    /* Only bind to elements, not the module */
    if (!fwk_id_is_type(id, FWK_ID_TYPE_ELEMENT)) {
        return FWK_SUCCESS;
    }

    ctx = &module_ctx.dev_ctx_table[fwk_id_get_element_idx(id)];

    /* Bind to timer API if timer ID is provided */
    if (!fwk_id_is_equal(ctx->timer_id, FWK_ID_NONE)) {
        status = fwk_module_bind(ctx->timer_id,
                                 MOD_TIMER_API_ID_TIMER,
                                 &ctx->timer_api);
        if (status != FWK_SUCCESS) {
            FWK_LOG_ERR("[MPLL] Failed to bind to timer API");
            return status;
        }
    }

    return FWK_SUCCESS;
}

static int mpll_clock_start(fwk_id_t id)
{
    struct mpll_clock_dev_ctx *ctx;

    /* Only start elements, not the module */
    if (!fwk_id_is_type(id, FWK_ID_TYPE_ELEMENT)) {
        return FWK_SUCCESS;
    }

    ctx = &module_ctx.dev_ctx_table[fwk_id_get_element_idx(id)];

    /* Mark device as initialized */
    ctx->initialized = true;

    FWK_LOG_INFO("[MPLL] Device %u initialized", fwk_id_get_element_idx(id));

    return FWK_SUCCESS;
}

static int mpll_clock_process_bind_request(fwk_id_t source_id,
                                           fwk_id_t target_id,
                                           fwk_id_t api_id,
                                           const void **api)
{
    enum mod_mpll_clock_api_idx api_idx;

    /* Only allow binding to elements */
    if (!fwk_id_is_type(target_id, FWK_ID_TYPE_ELEMENT)) {
        return FWK_E_ACCESS;
    }

    api_idx = (enum mod_mpll_clock_api_idx)fwk_id_get_api_idx(api_id);

    switch (api_idx) {
    case MOD_MPLL_CLOCK_API_IDX_DRIVER:
        *api = &mpll_clock_api;
        return FWK_SUCCESS;

    default:
        return FWK_E_PARAM;
    }
}

/*
 * Module descriptor
 */
const struct fwk_module module_mpll_clock = {
    .type = FWK_MODULE_TYPE_DRIVER,
    .api_count = MOD_MPLL_CLOCK_API_COUNT,
    .init = mpll_clock_init,
    .element_init = mpll_clock_element_init,
    .bind = mpll_clock_bind,
    .start = mpll_clock_start,
    .process_bind_request = mpll_clock_process_bind_request,
};
