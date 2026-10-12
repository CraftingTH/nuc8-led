/*
 * Intel NUC 8 Front LED Driver (Bean Canyon)
 *
 * Direct MMIO control of the Embedded Controller for Intel NUC 8 systems.
 * Bypasses the disabled ACPI WMI SLED method (0xE1).
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/string.h>
#include <linux/timer.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("NUC 8 Contributors");
MODULE_DESCRIPTION("Intel NUC 8 MMIO LED Driver");
MODULE_VERSION("1.1.0");

#define PROC_NAME       "nuc_led"
#define NUC_MMIO_BASE   0xFE410500
#define NUC_MMIO_SIZE   0x0100

#define REG_OBLS        0x32  /* 0x01 = Custom, 0x02 = Activity */
#define REG_O0BN        0x60  /* Brightness (0-100) */
#define REG_O0BH        0x61  /* Behavior: 1=Solid, 2=Breathe, 4=Fade */
#define REG_O0FQ        0x62  /* Hardware Frequency: 0-2 */
#define REG_O0CR        0x63  /* Color: 1-7 */

#define COLOR_BLUE      1
#define COLOR_RED       2
#define COLOR_GREEN     3
#define COLOR_YELLOW    4
#define COLOR_AMBER     5
#define COLOR_PURPLE    6
#define COLOR_WHITE     7

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0)
#define timer_delete_sync(t) del_timer_sync(t)
#endif

static void __iomem *io_base;
static struct timer_list blink_timer;
static bool timer_running = false;
static bool blink_state = false;
static u8 active_brightness = 100;
static unsigned long blink_period_ms = 500;

static void blink_timer_func(struct timer_list *t)
{
	blink_state = !blink_state;
	iowrite8(blink_state ? active_brightness : 0x00, io_base + REG_O0BN);
	mod_timer(&blink_timer, jiffies + msecs_to_jiffies(blink_period_ms));
}

static void stop_blink_timer(void)
{
	if (timer_running) {
		timer_delete_sync(&blink_timer);
		timer_running = false;
	}
}

static u8 parse_color(const char *c)
{
	if (!c) return COLOR_GREEN;
	if (!strcmp(c, "blue"))   return COLOR_BLUE;
	if (!strcmp(c, "red"))    return COLOR_RED;
	if (!strcmp(c, "green"))  return COLOR_GREEN;
	if (!strcmp(c, "yellow")) return COLOR_YELLOW;
	if (!strcmp(c, "amber"))  return COLOR_AMBER;
	if (!strcmp(c, "purple")) return COLOR_PURPLE;
	if (!strcmp(c, "white"))  return COLOR_WHITE;
	return COLOR_GREEN;
}

static ssize_t nuc_proc_write(struct file *filp, const char __user *buff,
                              size_t len, loff_t *data)
{
	char input[128];
	char *str, *tokens[5];
	int count = 0;
	long brightness = 100;
	u8 color = COLOR_GREEN;
	char *mode_str = "solid";
	char *freq_str = NULL;

	if (len >= sizeof(input))
		return -EINVAL;
	if (copy_from_user(input, buff, len))
		return -EFAULT;

	input[len] = '\0';
	str = strim(input);

	if (!strcmp(str, "auto") || !strcmp(str, "hdd,auto")) {
		stop_blink_timer();
		iowrite8(0x02, io_base + REG_OBLS);
		return len;
	}

	while (str && count < 5)
		tokens[count++] = strsep(&str, ",");

	if (count < 2)
		return -EINVAL;

	if (count == 2) {
		if (kstrtol(tokens[0], 10, &brightness)) return -EINVAL;
		color = parse_color(tokens[1]);
	} else if (count == 4) {
		if (kstrtol(tokens[1], 10, &brightness)) return -EINVAL;
		mode_str = tokens[2];
		color = parse_color(tokens[3]);
	} else if (count == 5) {
		if (kstrtol(tokens[1], 10, &brightness)) return -EINVAL;
		mode_str = tokens[2];
		freq_str = tokens[3];
		color = parse_color(tokens[4]);
	}

	if (brightness < 0) brightness = 0;
	if (brightness > 100) brightness = 100;
	active_brightness = (u8)brightness;

	stop_blink_timer();

	iowrite8(active_brightness, io_base + REG_O0BN);
	iowrite8(color,             io_base + REG_O0CR);
	iowrite8(0x01,              io_base + REG_OBLS);

	if (!strcmp(mode_str, "blink")) {
		blink_period_ms = 500;
		if (freq_str) {
			if (!strcmp(freq_str, "2.0hz") || !strcmp(freq_str, "fast"))
				blink_period_ms = 250;
			else if (!strcmp(freq_str, "0.5hz") || !strcmp(freq_str, "slow"))
				blink_period_ms = 1000;
		}
		iowrite8(0x01, io_base + REG_O0BH);
		blink_state = true;
		timer_running = true;
		mod_timer(&blink_timer, jiffies + msecs_to_jiffies(blink_period_ms));
	} else if (!strcmp(mode_str, "breathe")) {
		u8 hw_freq = 0x01;
		if (freq_str) {
			if (!strcmp(freq_str, "slow") || !strcmp(freq_str, "0.5hz"))
				hw_freq = 0x02;
			else if (!strcmp(freq_str, "fast") || !strcmp(freq_str, "2.0hz"))
				hw_freq = 0x00;
		}
		iowrite8(hw_freq, io_base + REG_O0FQ);
		iowrite8(0x02,    io_base + REG_O0BH);
	} else if (!strcmp(mode_str, "fade") || !strcmp(mode_str, "pulse")) {
		iowrite8(0x04, io_base + REG_O0BH);
	} else {
		iowrite8(0x00, io_base + REG_O0FQ);
		iowrite8(0x01, io_base + REG_O0BH);
	}

	return len;
}

static ssize_t nuc_proc_read(struct file *filp, char __user *buff,
                             size_t len, loff_t *data)
{
	char msg[256];
	size_t msg_len;
	u8 state = ioread8(io_base + REG_OBLS);
	u8 bn    = ioread8(io_base + REG_O0BN);
	u8 bh    = ioread8(io_base + REG_O0BH);
	u8 fq    = ioread8(io_base + REG_O0FQ);
	u8 cr    = ioread8(io_base + REG_O0CR);

	snprintf(msg, sizeof(msg),
	         "NUC8 LED: State=0x%02x BN=%d%% BH=0x%02x FQ=0x%02x Color=%d (Timer=%s)\n",
	         state, bn, bh, fq, cr, timer_running ? "active" : "off");

	msg_len = strlen(msg);
	if (*data >= msg_len) return 0;
	if (len < msg_len - *data) msg_len = len;
	if (copy_to_user(buff, msg + *data, msg_len)) return -EFAULT;
	*data += msg_len;
	return msg_len;
}

static const struct proc_ops proc_operations = {
	.proc_read  = nuc_proc_read,
	.proc_write = nuc_proc_write,
};

static int __init nuc_init(void)
{
	io_base = ioremap(NUC_MMIO_BASE, NUC_MMIO_SIZE);
	if (!io_base)
		return -ENOMEM;

	timer_setup(&blink_timer, blink_timer_func, 0);

	if (!proc_create(PROC_NAME, 0666, NULL, &proc_operations)) {
		iounmap(io_base);
		return -ENOMEM;
	}

	pr_info("nuc_led: driver initialized\n");
	return 0;
}

static void __exit nuc_exit(void)
{
	stop_blink_timer();
	remove_proc_entry(PROC_NAME, NULL);
	if (io_base)
		iounmap(io_base);
	pr_info("nuc_led: driver unloaded\n");
}

module_init(nuc_init);
module_exit(nuc_exit);
