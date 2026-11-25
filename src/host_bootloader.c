/*
 * Reboot into the UF2 bootloader when the USB host sets a CDC ACM port to
 * 1200 baud (decision 0062). Reboot path mirrors ZMK's behavior_reset.c.
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/uart/cdc_acm.h>
#include <zephyr/drivers/usb/usb_dc.h>
#include <zephyr/init.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/reboot.h>

#if IS_ENABLED(CONFIG_RETENTION_BOOT_MODE)
#include <zephyr/retention/bootmode.h>
#endif

#include <dt-bindings/zmk/reset.h>

LOG_MODULE_REGISTER(totem_host_bootloader, LOG_LEVEL_INF);

#define BOOTLOADER_BAUD 1200

static void on_dte_rate(const struct device *dev, uint32_t rate) {
    if (rate != BOOTLOADER_BAUD) {
        return;
    }

#if IS_ENABLED(CONFIG_RETENTION_BOOT_MODE)
    // Set the boot mode before detaching, so a failure leaves USB working.
    int ret = bootmode_set(BOOT_MODE_TYPE_BOOTLOADER);
    if (ret < 0) {
        LOG_ERR("Failed to set the bootloader mode (%d)", ret);
        return;
    }
    usb_dc_detach();
    sys_reboot(SYS_REBOOT_WARM);
#else
    usb_dc_detach();
    sys_reboot(RST_UF2);
#endif
}

#if DT_HAS_COMPAT_STATUS_OKAY(zephyr_cdc_acm_uart)

#define CDC_DEV(node_id) DEVICE_DT_GET(node_id),

static int host_bootloader_init(void) {
    static const struct device *const devs[] = {
        DT_FOREACH_STATUS_OKAY(zephyr_cdc_acm_uart, CDC_DEV)};

    for (size_t i = 0; i < ARRAY_SIZE(devs); i++) {
        int err = cdc_acm_dte_rate_callback_set(devs[i], on_dte_rate);
        if (err < 0) {
            LOG_ERR("Failed to register 1200 baud callback on %s (%d)", devs[i]->name, err);
        }
    }

    return 0; // never fail boot
}

SYS_INIT(host_bootloader_init, APPLICATION, 99);

#endif
