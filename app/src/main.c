#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/services/hids.h>

static const struct gpio_dt_spec button = GPIO_DT_SPEC_GET(DT_ALIAS(sw0), gpios); // or DT_NODELABEL(button_0)

static uint8_t hid_report_map[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        // Report ID (1)
    0x05, 0x07,        // Usage Page (Key Codes)
    0x19, 0xE0,        // Usage Minimum (Keyboard Left Control)
    0x29, 0xE7,        // Usage Maximum (Keyboard Right GUI)
    0x15, 0x00,        // Logical Minimum (0)
    0x25, 0x01,        // Logical Maximum (1)
    0x75, 0x01,        // Report Size (1)
    0x95, 0x08,        // Report Count (8)  -> Modifiers
    0x81, 0x02,        // Input (Data, Variable, Absolute)
    0x95, 0x01,        // Report Count (1)
    0x75, 0x08,        // Report Size (8)
    0x81, 0x01,        // Input (Constant)   -> Reserved byte
    0x95, 0x06,        // Report Count (6)
    0x75, 0x08,        // Report Size (8)
    0x15, 0x00,        // Logical Minimum (0)
    0x25, 0x65,        // Logical Maximum (101)
    0x05, 0x07,        // Usage Page (Key Codes)
    0x19, 0x00,        // Usage Minimum (Reserved)
    0x29, 0x65,        // Usage Maximum (Keyboard Application)
    0x81, 0x00,        // Input (Data, Array)
    0xC0               // End Collection
};

static struct bt_hids hids;
static bool connected = false;

static void button_pressed(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    if (!connected) return;

    // Press "A" (keycode 0x04)
    uint8_t report[8] = {0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00};
    bt_hids_inp_rep_send(&hids, 0, report, sizeof(report), NULL, NULL);

    k_msleep(50);  // Short press duration

    // Release
    memset(report, 0, sizeof(report));
    bt_hids_inp_rep_send(&hids, 0, report, sizeof(report), NULL, NULL);
}

static struct gpio_callback button_cb_data;

static void hids_connected(struct bt_hids *hids_obj)
{
    connected = true;
    printk("HID Connected\n");
}

static void hids_disconnected(struct bt_hids *hids_obj)
{
    connected = false;
    printk("HID Disconnected\n");
}

int main(void)
{
    int err;

    printk("Starting BLE HID Keyboard\n");

    if (!device_is_ready(button.port)) {
        printk("Button not ready\n");
        return 0;
    }
    gpio_pin_configure_dt(&button, GPIO_INPUT);
    gpio_pin_interrupt_configure_dt(&button, GPIO_INT_EDGE_TO_ACTIVE);
    gpio_init_callback(&button_cb_data, button_pressed, BIT(button.pin));
    gpio_add_callback(button.port, &button_cb_data);

    err = bt_enable(NULL);
    if (err) return err;

    static const struct bt_hids_init_param hids_init = {
        .report_map = hid_report_map,
        .report_map_size = sizeof(hid_report_map),
        .cb = {
            .connected = hids_connected,
            .disconnected = hids_disconnected,
        }
    };

    err = bt_hids_init(&hids, &hids_init);
    if (err) return err;

    bt_hids_notify_input(&hids, 0);  // Enable notifications

    while (1) {
        k_sleep(K_FOREVER);
    }
}
