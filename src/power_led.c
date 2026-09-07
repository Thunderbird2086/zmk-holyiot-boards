#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

static const struct gpio_dt_spec power_led = GPIO_DT_SPEC_GET(DT_ALIAS(led1), gpios);

static int power_led_init(void)
{
    if (!gpio_is_ready_dt(&power_led)) {
        return -ENODEV;
    }

    gpio_pin_configure_dt(&power_led, GPIO_OUTPUT_ACTIVE);
    
    return 0;
}

SYS_INIT(power_led_init, POST_KERNEL, CONFIG_APPLICATION_INIT_PRIORITY);