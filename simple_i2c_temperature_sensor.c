// A simple I2C temperature sensor  *
// This is a model driver for temperaure sensor
// Need to tweak accordingly for real driver implementation

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sysfs.h>
#include <linux/interrupt.h>
#include <linux/of.h>
#include <linux/mutex.h>

/* Device-specific structure */
struct sensor_dev {
    struct i2c_client *client;
    int irq;                      // Optional IRQ line
    int temperature;               // Cached temperature
    struct mutex lock;             // Protect temperature reads
};

/* Stub: hardware initialization */
static int sensor_hw_init(struct sensor_dev *dev)
{
    /* Normally write default registers via I2C */
    return 0;
}

/* Stub: read temperature register via I2C */
static int sensor_read_temp(struct sensor_dev *dev)
{
    /* Simulate read; replace with i2c_smbus_read_byte_data in real driver */
    return 25; // Example: 25C
}

/* IRQ handler for data-ready signal */
static irqreturn_t sensor_irq_handler(int irq, void *dev_id)
{
    struct sensor_dev *dev = dev_id;
    int temp;

    temp = sensor_read_temp(dev);

    mutex_lock(&dev->lock);
    dev->temperature = temp;
    mutex_unlock(&dev->lock);

    dev_info(&dev->client->dev, "Sensor IRQ: temperature updated: %dC\n", temp);
    return IRQ_HANDLED;
}

/* Sysfs show function */
static ssize_t temperature_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    struct sensor_dev *sdev = dev_get_drvdata(dev);
    int temp;

    mutex_lock(&sdev->lock);
    temp = sdev->temperature;
    mutex_unlock(&sdev->lock);

    return sprintf(buf, "%d\n", temp);
}

static DEVICE_ATTR_RO(temperature);

/* Probe function */
static int sensor_probe(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
    struct sensor_dev *dev;
    int ret;

    /* Allocate device structure */
    dev = devm_kzalloc(&client->dev, sizeof(*dev), GFP_KERNEL);
    if (!dev)
        return -ENOMEM;

    dev->client = client;
    mutex_init(&dev->lock);
    i2c_set_clientdata(client, dev);

    /* Hardware init */
    ret = sensor_hw_init(dev);
    if (ret) {
        dev_err(&client->dev, "Sensor hardware init failed\n");
        return ret;
    }

    /* Optional IRQ setup if defined in DT or platform */
    if (client->irq) {
        dev->irq = client->irq;
        ret = devm_request_threaded_irq(&client->dev, dev->irq,
                                        NULL, sensor_irq_handler,
                                        IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
                                        "sensor_irq", dev);
        if (ret) {
            dev_err(&client->dev, "Failed to request IRQ\n");
            return ret;
        }
    }

    /* Sysfs attribute */
    ret = device_create_file(&client->dev, &dev_attr_temperature);
    if (ret) {
        dev_err(&client->dev, "Failed to create sysfs attribute\n");
        return ret;
    }

    dev_info(&client->dev, "Temperature sensor initialized\n");
    return 0;
}

/* Remove function */
static int sensor_remove(struct i2c_client *client)
{
    struct sensor_dev *dev = i2c_get_clientdata(client);

    device_remove_file(&client->dev, &dev_attr_temperature);
    dev_info(&client->dev, "Temperature sensor removed\n");
    return 0;
}

/* Device ID table */
static const struct i2c_device_id sensor_id[] = {
    { "temp_sensor", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, sensor_id);

/* Device tree match */
#ifdef CONFIG_OF
static const struct of_device_id sensor_of_match[] = {
    { .compatible = "mycompany,temp_sensor", },
    { }
};
MODULE_DEVICE_TABLE(of, sensor_of_match);
#endif

/* I2C driver structure */
static struct i2c_driver sensor_driver = {
    .driver = {
        .name = "temp_sensor_driver",
        .of_match_table = of_match_ptr(sensor_of_match),
    },
    .probe = sensor_probe,
    .remove = sensor_remove,
    .id_table = sensor_id,
};

module_i2c_driver(sensor_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Salim Malik");
MODULE_DESCRIPTION("Simple I2C Temperature Sensor Driver with IRQ and sysfs");
