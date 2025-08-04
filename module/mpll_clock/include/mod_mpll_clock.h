/*
 * Arm SCP/MCP Software
 * Copyright (c) 2024, Arm Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Description:
 *     MPLL Clock Driver Module
 */

#ifndef MOD_MPLL_CLOCK_H
#define MOD_MPLL_CLOCK_H

#include <fwk_id.h>
#include <fwk_module_idx.h>

#include <stdint.h>
#include <stdbool.h>

/*!
 * \addtogroup GroupModules Modules
 * \{
 */

/*!
 * \defgroup GroupMPLLClock MPLL Clock Driver
 *
 * \brief MPLL Clock hardware abstraction layer.
 *
 * \details This module provides an interface for configuring MPLL (Main PLL)
 *          clock settings with support for both byte and word-level register
 *          access operations.
 *
 * \{
 */

/*!
 * \brief MPLL Clock module API indices.
 */
enum mod_mpll_clock_api_idx {
    /*! MPLL Clock driver API index */
    MOD_MPLL_CLOCK_API_IDX_DRIVER,

    /*! Number of APIs */
    MOD_MPLL_CLOCK_API_COUNT,
};

/*!
 * \brief MPLL Clock driver API ID.
 */
#define MOD_MPLL_CLOCK_API_ID_DRIVER \
    FWK_ID_API(FWK_MODULE_IDX_MPLL_CLOCK, MOD_MPLL_CLOCK_API_IDX_DRIVER)

/*!
 * \brief Register access width types.
 */
enum mod_mpll_clock_reg_width {
    /*! 8-bit (byte) register access */
    MOD_MPLL_CLOCK_REG_WIDTH_8BIT,

    /*! 32-bit (word) register access */
    MOD_MPLL_CLOCK_REG_WIDTH_32BIT,

    /*! Number of register width types */
    MOD_MPLL_CLOCK_REG_WIDTH_COUNT,
};

/*!
 * \brief MPLL configuration parameters.
 */
struct mod_mpll_clock_config {
    /*! Base address of MPLL control registers */
    uintptr_t reg_base;

    /*! MPLL control register offset */
    uint32_t control_reg_offset;

    /*! MPLL status register offset */
    uint32_t status_reg_offset;

    /*! MPLL frequency register offset */
    uint32_t freq_reg_offset;

    /*! Default register access width */
    enum mod_mpll_clock_reg_width default_width;

    /*! MPLL reference frequency in Hz */
    uint64_t ref_frequency;

    /*! Minimum output frequency in Hz */
    uint64_t min_frequency;

    /*! Maximum output frequency in Hz */
    uint64_t max_frequency;

    /*! MPLL lock timeout in microseconds */
    uint32_t lock_timeout_us;

    /*! Timer device ID for timeout operations (optional) */
    fwk_id_t timer_id;
};

/*!
 * \brief MPLL register access parameters.
 */
struct mod_mpll_clock_reg_params {
    /*! Register offset from base address */
    uint32_t offset;

    /*! Register access width */
    enum mod_mpll_clock_reg_width width;

    /*! Value to write (for write operations) */
    uint32_t value;

    /*! Bit mask for partial register updates */
    uint32_t mask;
};

/*!
 * \brief MPLL frequency configuration.
 */
struct mod_mpll_clock_freq_config {
    /*! Target frequency in Hz */
    uint64_t frequency;

    /*! PLL multiplier value */
    uint32_t multiplier;

    /*! PLL divider value */
    uint32_t divider;

    /*! Post-divider value */
    uint32_t post_divider;
};

/*!
 * \brief MPLL Clock driver interface.
 */
struct mod_mpll_clock_api {
    /*!
     * \brief Set MPLL frequency configuration.
     *
     * \details Configure the MPLL to output the specified frequency.
     *          This function handles the complete PLL configuration sequence
     *          including lock detection.
     *
     * \param dev_id Element identifier of the MPLL clock device.
     * \param freq_config Pointer to frequency configuration parameters.
     *
     * \retval ::FWK_SUCCESS The operation succeeded.
     * \retval ::FWK_E_PARAM Invalid parameters provided.
     * \retval ::FWK_E_TIMEOUT PLL failed to lock within timeout period.
     * \retval ::FWK_E_DEVICE Hardware error occurred.
     *
     * \return Status code representing the result of the operation.
     */
    int (*setmpll)(fwk_id_t dev_id,
                   const struct mod_mpll_clock_freq_config *freq_config);

    /*!
     * \brief Get current MPLL frequency configuration.
     *
     * \details Read the current MPLL configuration and calculate the
     *          output frequency based on register values.
     *
     * \param dev_id Element identifier of the MPLL clock device.
     * \param[out] freq_config Pointer to store current frequency configuration.
     *
     * \retval ::FWK_SUCCESS The operation succeeded.
     * \retval ::FWK_E_PARAM Invalid parameters provided.
     * \retval ::FWK_E_DEVICE Hardware error occurred.
     *
     * \return Status code representing the result of the operation.
     */
    int (*getmpll)(fwk_id_t dev_id,
                   struct mod_mpll_clock_freq_config *freq_config);

    /*!
     * \brief Read register value with specified width.
     *
     * \details Read a register value using either byte (8-bit) or
     *          word (32-bit) access.
     *
     * \param dev_id Element identifier of the MPLL clock device.
     * \param reg_params Pointer to register access parameters.
     * \param[out] value Pointer to store the read value.
     *
     * \retval ::FWK_SUCCESS The operation succeeded.
     * \retval ::FWK_E_PARAM Invalid parameters provided.
     * \retval ::FWK_E_DEVICE Hardware error occurred.
     *
     * \return Status code representing the result of the operation.
     */
    int (*read_reg)(fwk_id_t dev_id,
                    const struct mod_mpll_clock_reg_params *reg_params,
                    uint32_t *value);

    /*!
     * \brief Write register value with specified width.
     *
     * \details Write a register value using either byte (8-bit) or
     *          word (32-bit) access. Supports masked writes for partial
     *          register updates.
     *
     * \param dev_id Element identifier of the MPLL clock device.
     * \param reg_params Pointer to register access parameters.
     *
     * \retval ::FWK_SUCCESS The operation succeeded.
     * \retval ::FWK_E_PARAM Invalid parameters provided.
     * \retval ::FWK_E_DEVICE Hardware error occurred.
     *
     * \return Status code representing the result of the operation.
     */
    int (*write_reg)(fwk_id_t dev_id,
                     const struct mod_mpll_clock_reg_params *reg_params);
};

/*!
 * \}
 */

/*!
 * \}
 */

#endif /* MOD_MPLL_CLOCK_H */
