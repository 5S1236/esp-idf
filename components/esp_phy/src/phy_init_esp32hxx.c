/*
 * SPDX-FileCopyrightText: 2015-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "esp_attr.h"
#include "freertos/portmacro.h"
#include "esp_phy_init.h"
#include "esp_private/phy.h"

#define PHY_ENABLE_VERSION_PRINT 1

static DRAM_ATTR portMUX_TYPE s_phy_int_mux = portMUX_INITIALIZER_UNLOCKED;

extern void phy_version_print(void);
static _lock_t s_phy_access_lock;

/* Reference count of enabling PHY */
static uint8_t s_phy_access_ref = 0;
static bool s_phy_is_enabled = false;

uint32_t IRAM_ATTR phy_enter_critical(void)
{
    if (xPortInIsrContext()) {
        portENTER_CRITICAL_ISR(&s_phy_int_mux);

    } else {
        portENTER_CRITICAL(&s_phy_int_mux);
    }
    // Interrupt level will be stored in current tcb, so always return zero.
    return 0;
}

void IRAM_ATTR phy_exit_critical(uint32_t level)
{
    // Param level don't need any more, ignore it.
    if (xPortInIsrContext()) {
        portEXIT_CRITICAL_ISR(&s_phy_int_mux);
    } else {
        portEXIT_CRITICAL(&s_phy_int_mux);
    }
}

void esp_phy_enable(void)
{
    _lock_acquire(&s_phy_access_lock);
    if (s_phy_access_ref == 0) {
        if (!s_phy_is_enabled) {
            register_chipv7_phy(NULL, NULL, PHY_RF_CAL_FULL);
            phy_version_print();
            s_phy_is_enabled = true;
        } else {
            phy_wakeup_init();
        }
        phy_track_pll_init();
    }

    s_phy_access_ref++;

    _lock_release(&s_phy_access_lock);
}

void esp_phy_disable(void)
{
    _lock_acquire(&s_phy_access_lock);

    if (s_phy_access_ref) {
        s_phy_access_ref--;
    }

    if (s_phy_access_ref == 0) {
        phy_track_pll_deinit();
        phy_close_rf();
        phy_xpd_tsens();
    }

    _lock_release(&s_phy_access_lock);
}
