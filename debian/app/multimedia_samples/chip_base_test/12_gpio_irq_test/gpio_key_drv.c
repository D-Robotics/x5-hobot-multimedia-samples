#include <linux/module.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/slab.h>
#include <linux/jiffies.h>

struct gpio_key {
	int gpio;
	struct gpio_desc *gpiod;
	int flag;
	int irq;
	u32 debounce_interval;
	unsigned long last_interrupt;
};

static struct gpio_key *gpio_keys_hobot;

static irqreturn_t gpio_key_isr(int irq, void *dev_id)
{
	struct gpio_key *key = dev_id;
	unsigned long now = jiffies;
	int val = gpiod_get_value(key->gpiod);

	if (time_after(now, key->last_interrupt + key->debounce_interval)) {
		printk(KERN_INFO "gpio_key: IRQ GPIO %d value=%d\n", key->gpio, val);
		key->last_interrupt = now;
	}

	return IRQ_HANDLED;
}

static int gpio_key_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int count, i, err;
	u32 debounce_interval = 100; // 默认100ms
	enum of_gpio_flags flag;

	printk("%s: probe start\n", __func__);

	count = of_gpio_count(node);
	if (!count) {
		dev_err(&pdev->dev, "No GPIO defined in DT\n");
		return -EINVAL;
	}

	gpio_keys_hobot = kzalloc(sizeof(struct gpio_key) * count, GFP_KERNEL);
	if (!gpio_keys_hobot)
		return -ENOMEM;

	if (of_property_read_u32(node, "debounce-interval", &debounce_interval))
		dev_info(&pdev->dev, "No debounce-interval in DT, use default %ums\n", debounce_interval);
	else
		dev_info(&pdev->dev, "debounce-interval: %u ms\n", debounce_interval);

	for (i = 0; i < count; i++) {
		gpio_keys_hobot[i].gpio = of_get_gpio_flags(node, i, &flag);
		if (gpio_keys_hobot[i].gpio < 0) {
			dev_err(&pdev->dev, "of_get_gpio_flags failed for index %d\n", i);
			err = gpio_keys_hobot[i].gpio;
			goto free_mem;
		}

		gpio_keys_hobot[i].gpiod = gpio_to_desc(gpio_keys_hobot[i].gpio);
		if (!gpio_keys_hobot[i].gpiod) {
			dev_err(&pdev->dev, "gpio_to_desc failed for gpio %d\n", gpio_keys_hobot[i].gpio);
			err = -ENODEV;
			goto free_mem;
		}

		gpio_keys_hobot[i].flag = flag & OF_GPIO_ACTIVE_LOW;
		gpio_keys_hobot[i].irq = gpio_to_irq(gpio_keys_hobot[i].gpio);
		if (gpio_keys_hobot[i].irq < 0) {
			dev_err(&pdev->dev, "gpio_to_irq failed for gpio %d\n", gpio_keys_hobot[i].gpio);
			err = gpio_keys_hobot[i].irq;
			goto free_mem;
		}

		gpio_keys_hobot[i].debounce_interval = debounce_interval * HZ / 1000; // ms -> jiffies
		gpio_keys_hobot[i].last_interrupt = jiffies;

		err = request_irq(
			gpio_keys_hobot[i].irq,
			gpio_key_isr,
			IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
			dev_name(&pdev->dev),
			&gpio_keys_hobot[i]);
		if (err) {
			dev_err(&pdev->dev, "request_irq failed for gpio %d irq %d\n",
					gpio_keys_hobot[i].gpio, gpio_keys_hobot[i].irq);
			goto free_irq;
		}
	}

	platform_set_drvdata(pdev, gpio_keys_hobot);
	dev_info(&pdev->dev, "gpio_key probe done.\n");
	return 0;

free_irq:
	while (--i >= 0)
		free_irq(gpio_keys_hobot[i].irq, &gpio_keys_hobot[i]);
free_mem:
	kfree(gpio_keys_hobot);
	return err;
}

static int gpio_key_remove(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	int count, i;

	count = of_gpio_count(node);
	for (i = 0; i < count; i++)
		free_irq(gpio_keys_hobot[i].irq, &gpio_keys_hobot[i]);

	kfree(gpio_keys_hobot);
	dev_info(&pdev->dev, "gpio_key removed.\n");
	return 0;
}

static const struct of_device_id hobot_keys[] = {
	{ .compatible = "hobot,gpio_key" },
	{ },
};
MODULE_DEVICE_TABLE(of, hobot_keys);

static struct platform_driver gpio_keys_driver = {
	.probe = gpio_key_probe,
	.remove = gpio_key_remove,
	.driver = {
		.name = "hobot_gpio_key",
		.of_match_table = hobot_keys,
	},
};

static int __init gpio_key_init(void)
{
	printk("%s: init\n", __func__);
	return platform_driver_register(&gpio_keys_driver);
}

static void __exit gpio_key_exit(void)
{
	printk("%s: exit\n", __func__);
	platform_driver_unregister(&gpio_keys_driver);
}

module_init(gpio_key_init);
module_exit(gpio_key_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Hobot GPIO Key Driver");
