/*
 * Reversed by LazyC0DEr
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License. See the file COPYING in the main directory of this archive for
 * more details.
 */

#include <mach/mt_pm_ldo.h>
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#include <cust_i2c.h>

#include "lcm_drv.h"
#include "tps65132_i2c.h"

static LCM_UTIL_FUNCS lcm_util;
static raw_spinlock_t boe_SpinLock;
static int boe_value_0 = 0, boe_value_1 = 0, boe_value_2 = 0, global_brightness_level = 0;

#define dsi_set_cmdq_V2(cmd, count, ppara, force_update)	lcm_util.dsi_set_cmdq_V2(cmd, count, ppara, force_update)
#define dsi_set_cmdq(pdata, queue_size, force_update)		lcm_util.dsi_set_cmdq(pdata, queue_size, force_update)
#define read_reg_v2(cmd, buffer, buffer_size)			lcm_util.dsi_dcs_read_lcm_reg_v2(cmd, buffer, buffer_size)

#define MDELAY(n)						(lcm_util.mdelay(n))
#define UDELAY(n)						(lcm_util.udelay(n))

#define REGFLAG_DELAY						0xFC
#define REGFLAG_END_OF_TABLE					0xFD

struct LCM_setting_table {
    unsigned char cmd;
    unsigned char count;
    unsigned char para_list[64];
};

static struct LCM_setting_table lcm_backlight_level_setting[] = {
    {0xFF, 1, {0x00}},
    {REGFLAG_DELAY, 1, {}},
    {0xFB, 1, {0x01}},
    {0x51, 1, {0xFF}},
    {REGFLAG_END_OF_TABLE, 0, {}}
};

static struct LCM_setting_table lcm_backlight_disable[] = {
    {0x55, 1, {0x00}},
    {REGFLAG_END_OF_TABLE, 0, {}}
};

static struct LCM_setting_table lcm_backlight_enable[] = {
    {0x55, 1, {0x01}},
    {REGFLAG_END_OF_TABLE, 0, {}}
};

static struct LCM_setting_table lcm_suspend_setting[] = {
    {0xff, 0x01, {0x00}},
    {REGFLAG_DELAY, 1,{}},
    {0x28, 0x00, {}},
    {REGFLAG_DELAY, 20,{}},
    {0x10, 0x00, {}},
    {REGFLAG_DELAY, 120,{}},
    {REGFLAG_END_OF_TABLE, 0x00,{}}
};

static struct LCM_setting_table lcm_initialization_setting[] = {
    {0xff, 0x01, {0x01}},
    {REGFLAG_DELAY, 1,{}},
    {0xfb, 0x01, {0x01}},
    {0x00, 0x01, {0x01}},
    {0x01, 0x01, {0x55}},
    {0x02, 0x01, {0x59}},
    {0x04, 0x01, {0x0c}},
    {0x05, 0x01, {0x3a}},
    {0x06, 0x01, {0x55}},
    {0x07, 0x01, {0xd5}},
    {0x0d, 0x01, {0x9d}},
    {0x0e, 0x01, {0x9d}},
    {0x0f, 0x01, {0xe0}},
    {0x10, 0x01, {0x03}},
    {0x11, 0x01, {0x3c}},
    {0x12, 0x01, {0x50}},
    {0x15, 0x01, {0x60}},
    {0x16, 0x01, {0x14}},
    {0x17, 0x01, {0x14}},
    {0x44, 0x01, {0x68}},
    {0x45, 0x01, {0x88}},
    {0x46, 0x01, {0x78}},
    {0x68, 0x01, {0x13}},
    {0x6d, 0x01, {0x33}},
    {0x75, 0x01, {0x00}},
    {0x76, 0x01, {0x00}},
    {0x77, 0x01, {0x00}},
    {0x78, 0x01, {0x31}},
    {0x79, 0x01, {0x00}},
    {0x7a, 0x01, {0x5e}},
    {0x7b, 0x01, {0x00}},
    {0x7c, 0x01, {0x7e}},
    {0x7d, 0x01, {0x00}},
    {0x7e, 0x01, {0x98}},
    {0x7f, 0x01, {0x00}},
    {0x80, 0x01, {0xae}},
    {0x81, 0x01, {0x00}},
    {0x82, 0x01, {0xc1}},
    {0x83, 0x01, {0x00}},
    {0x84, 0x01, {0xd2}},
    {0x85, 0x01, {0x00}},
    {0x86, 0x01, {0xe1}},
    {0x87, 0x01, {0x01}},
    {0x88, 0x01, {0x13}},
    {0x89, 0x01, {0x01}},
    {0x8a, 0x01, {0x3a}},
    {0x8b, 0x01, {0x01}},
    {0x8c, 0x01, {0x77}},
    {0x8d, 0x01, {0x01}},
    {0x8e, 0x01, {0xa6}},
    {0x8f, 0x01, {0x01}},
    {0x90, 0x01, {0xef}},
    {0x91, 0x01, {0x02}},
    {0x92, 0x01, {0x26}},
    {0x93, 0x01, {0x02}},
    {0x94, 0x01, {0x28}},
    {0x95, 0x01, {0x02}},
    {0x96, 0x01, {0x5a}},
    {0x97, 0x01, {0x02}},
    {0x98, 0x01, {0x93}},
    {0x99, 0x01, {0x02}},
    {0x9a, 0x01, {0xb8}},
    {0x9b, 0x01, {0x02}},
    {0x9c, 0x01, {0xea}},
    {0x9d, 0x01, {0x03}},
    {0x9e, 0x01, {0x16}},
    {0x9f, 0x01, {0x03}},
    {0xa0, 0x01, {0x43}},
    {0xa2, 0x01, {0x03}},
    {0xa3, 0x01, {0x48}},
    {0xa4, 0x01, {0x03}},
    {0xa5, 0x01, {0x4a}},
    {0xa6, 0x01, {0x03}},
    {0xa7, 0x01, {0x6e}},
    {0xa9, 0x01, {0x03}},
    {0xaa, 0x01, {0x7c}},
    {0xab, 0x01, {0x03}},
    {0xac, 0x01, {0x8c}},
    {0xad, 0x01, {0x03}},
    {0xae, 0x01, {0x9c}},
    {0xaf, 0x01, {0x03}},
    {0xb0, 0x01, {0xa0}},
    {0xb1, 0x01, {0x03}},
    {0xb2, 0x01, {0xce}},
    {0xb3, 0x01, {0x00}},
    {0xb4, 0x01, {0x00}},
    {0xb5, 0x01, {0x00}},
    {0xb6, 0x01, {0x31}},
    {0xb7, 0x01, {0x00}},
    {0xb8, 0x01, {0x5e}},
    {0xb9, 0x01, {0x00}},
    {0xba, 0x01, {0x7e}},
    {0xbb, 0x01, {0x00}},
    {0xbc, 0x01, {0x98}},
    {0xbd, 0x01, {0x00}},
    {0xbe, 0x01, {0xae}},
    {0xbf, 0x01, {0x00}},
    {0xc0, 0x01, {0xc1}},
    {0xc1, 0x01, {0x00}},
    {0xc2, 0x01, {0xd2}},
    {0xc3, 0x01, {0x00}},
    {0xc4, 0x01, {0xe1}},
    {0xc5, 0x01, {0x01}},
    {0xc6, 0x01, {0x13}},
    {0xc7, 0x01, {0x01}},
    {0xc8, 0x01, {0x3a}},
    {0xc9, 0x01, {0x01}},
    {0xca, 0x01, {0x77}},
    {0xcb, 0x01, {0x01}},
    {0xcc, 0x01, {0xa6}},
    {0xcd, 0x01, {0x01}},
    {0xce, 0x01, {0xef}},
    {0xcf, 0x01, {0x02}},
    {0xd0, 0x01, {0x26}},
    {0xd1, 0x01, {0x02}},
    {0xd2, 0x01, {0x28}},
    {0xd3, 0x01, {0x02}},
    {0xd4, 0x01, {0x5a}},
    {0xd5, 0x01, {0x02}},
    {0xd6, 0x01, {0x93}},
    {0xd7, 0x01, {0x02}},
    {0xd8, 0x01, {0xb8}},
    {0xd9, 0x01, {0x02}},
    {0xda, 0x01, {0xea}},
    {0xdb, 0x01, {0x03}},
    {0xdc, 0x01, {0x16}},
    {0xdd, 0x01, {0x03}},
    {0xde, 0x01, {0x43}},
    {0xdf, 0x01, {0x03}},
    {0xe0, 0x01, {0x48}},
    {0xe1, 0x01, {0x03}},
    {0xe2, 0x01, {0x4a}},
    {0xe3, 0x01, {0x03}},
    {0xe4, 0x01, {0x6e}},
    {0xe5, 0x01, {0x03}},
    {0xe6, 0x01, {0x7c}},
    {0xe7, 0x01, {0x03}},
    {0xe8, 0x01, {0x8c}},
    {0xe9, 0x01, {0x03}},
    {0xea, 0x01, {0x9c}},
    {0xeb, 0x01, {0x03}},
    {0xec, 0x01, {0xa0}},
    {0xed, 0x01, {0x03}},
    {0xee, 0x01, {0xce}},
    {0xef, 0x01, {0x00}},
    {0xf0, 0x01, {0xd6}},
    {0xf1, 0x01, {0x00}},
    {0xf2, 0x01, {0xdd}},
    {0xf3, 0x01, {0x00}},
    {0xf4, 0x01, {0xeb}},
    {0xf5, 0x01, {0x00}},
    {0xf6, 0x01, {0xf7}},
    {0xf7, 0x01, {0x01}},
    {0xf8, 0x01, {0x04}},
    {0xf9, 0x01, {0x01}},
    {0xfa, 0x01, {0x0f}},
    {0xff, 0x01, {0x02}},
    {REGFLAG_DELAY, 1,{}},
    {0xfb, 0x01, {0x01}},
    {0x00, 0x01, {0x01}},
    {0x01, 0x01, {0x19}},
    {0x02, 0x01, {0x01}},
    {0x03, 0x01, {0x22}},
    {0x04, 0x01, {0x01}},
    {0x05, 0x01, {0x2b}},
    {0x06, 0x01, {0x01}},
    {0x07, 0x01, {0x4d}},
    {0x08, 0x01, {0x01}},
    {0x09, 0x01, {0x69}},
    {0x0a, 0x01, {0x01}},
    {0x0b, 0x01, {0x99}},
    {0x0c, 0x01, {0x01}},
    {0x0d, 0x01, {0xc0}},
    {0x0e, 0x01, {0x01}},
    {0x0f, 0x01, {0xfe}},
    {0x10, 0x01, {0x02}},
    {0x11, 0x01, {0x2f}},
    {0x12, 0x01, {0x02}},
    {0x13, 0x01, {0x31}},
    {0x14, 0x01, {0x02}},
    {0x15, 0x01, {0x62}},
    {0x16, 0x01, {0x02}},
    {0x17, 0x01, {0x99}},
    {0x18, 0x01, {0x02}},
    {0x19, 0x01, {0xbf}},
    {0x1a, 0x01, {0x02}},
    {0x1b, 0x01, {0xf6}},
    {0x1c, 0x01, {0x03}},
    {0x1d, 0x01, {0x1c}},
    {0x1e, 0x01, {0x03}},
    {0x1f, 0x01, {0x4a}},
    {0x20, 0x01, {0x03}},
    {0x21, 0x01, {0x58}},
    {0x22, 0x01, {0x03}},
    {0x23, 0x01, {0x64}},
    {0x24, 0x01, {0x03}},
    {0x25, 0x01, {0xa9}},
    {0x26, 0x01, {0x03}},
    {0x27, 0x01, {0xae}},
    {0x28, 0x01, {0x03}},
    {0x29, 0x01, {0xb3}},
    {0x2a, 0x01, {0x03}},
    {0x2b, 0x01, {0xbf}},
    {0x2d, 0x01, {0x03}},
    {0x2f, 0x01, {0xcc}},
    {0x30, 0x01, {0x03}},
    {0x31, 0x01, {0xce}},
    {0x32, 0x01, {0x00}},
    {0x33, 0x01, {0xd6}},
    {0x34, 0x01, {0x00}},
    {0x35, 0x01, {0xdd}},
    {0x36, 0x01, {0x00}},
    {0x37, 0x01, {0xeb}},
    {0x38, 0x01, {0x00}},
    {0x39, 0x01, {0xf7}},
    {0x3a, 0x01, {0x01}},
    {0x3b, 0x01, {0x04}},
    {0x3d, 0x01, {0x01}},
    {0x3f, 0x01, {0x0f}},
    {0x40, 0x01, {0x01}},
    {0x41, 0x01, {0x19}},
    {0x42, 0x01, {0x01}},
    {0x43, 0x01, {0x22}},
    {0x44, 0x01, {0x01}},
    {0x45, 0x01, {0x2b}},
    {0x46, 0x01, {0x01}},
    {0x47, 0x01, {0x4d}},
    {0x48, 0x01, {0x01}},
    {0x49, 0x01, {0x69}},
    {0x4a, 0x01, {0x01}},
    {0x4b, 0x01, {0x99}},
    {0x4c, 0x01, {0x01}},
    {0x4d, 0x01, {0xc0}},
    {0x4e, 0x01, {0x01}},
    {0x4f, 0x01, {0xfe}},
    {0x50, 0x01, {0x02}},
    {0x51, 0x01, {0x2f}},
    {0x52, 0x01, {0x02}},
    {0x53, 0x01, {0x31}},
    {0x54, 0x01, {0x02}},
    {0x55, 0x01, {0x62}},
    {0x56, 0x01, {0x02}},
    {0x58, 0x01, {0x99}},
    {0x59, 0x01, {0x02}},
    {0x5a, 0x01, {0xbf}},
    {0x5b, 0x01, {0x02}},
    {0x5c, 0x01, {0xf6}},
    {0x5d, 0x01, {0x03}},
    {0x5e, 0x01, {0x1c}},
    {0x5f, 0x01, {0x03}},
    {0x60, 0x01, {0x4a}},
    {0x61, 0x01, {0x03}},
    {0x62, 0x01, {0x58}},
    {0x63, 0x01, {0x03}},
    {0x64, 0x01, {0x64}},
    {0x65, 0x01, {0x03}},
    {0x66, 0x01, {0xa9}},
    {0x67, 0x01, {0x03}},
    {0x68, 0x01, {0xae}},
    {0x69, 0x01, {0x03}},
    {0x6a, 0x01, {0xb3}},
    {0x6b, 0x01, {0x03}},
    {0x6c, 0x01, {0xbf}},
    {0x6d, 0x01, {0x03}},
    {0x6e, 0x01, {0xcc}},
    {0x6f, 0x01, {0x03}},
    {0x70, 0x01, {0xce}},
    {0x71, 0x01, {0x00}},
    {0x72, 0x01, {0xcd}},
    {0x73, 0x01, {0x00}},
    {0x74, 0x01, {0xd4}},
    {0x75, 0x01, {0x00}},
    {0x76, 0x01, {0xe1}},
    {0x77, 0x01, {0x00}},
    {0x78, 0x01, {0xed}},
    {0x79, 0x01, {0x00}},
    {0x7a, 0x01, {0xfa}},
    {0x7b, 0x01, {0x01}},
    {0x7c, 0x01, {0x05}},
    {0x7d, 0x01, {0x01}},
    {0x7e, 0x01, {0x0f}},
    {0x7f, 0x01, {0x01}},
    {0x80, 0x01, {0x19}},
    {0x81, 0x01, {0x01}},
    {0x82, 0x01, {0x22}},
    {0x83, 0x01, {0x01}},
    {0x84, 0x01, {0x44}},
    {0x85, 0x01, {0x01}},
    {0x86, 0x01, {0x61}},
    {0x87, 0x01, {0x01}},
    {0x88, 0x01, {0x92}},
    {0x89, 0x01, {0x01}},
    {0x8a, 0x01, {0xba}},
    {0x8b, 0x01, {0x01}},
    {0x8c, 0x01, {0xf9}},
    {0x8d, 0x01, {0x02}},
    {0x8e, 0x01, {0x2c}},
    {0x8f, 0x01, {0x02}},
    {0x90, 0x01, {0x2e}},
    {0x91, 0x01, {0x02}},
    {0x92, 0x01, {0x5f}},
    {0x93, 0x01, {0x02}},
    {0x94, 0x01, {0x97}},
    {0x95, 0x01, {0x02}},
    {0x96, 0x01, {0xbe}},
    {0x97, 0x01, {0x02}},
    {0x98, 0x01, {0xf4}},
    {0x99, 0x01, {0x03}},
    {0x9a, 0x01, {0x22}},
    {0x9b, 0x01, {0x03}},
    {0x9c, 0x01, {0xb5}},
    {0x9d, 0x01, {0x03}},
    {0x9e, 0x01, {0xb7}},
    {0x9f, 0x01, {0x03}},
    {0xa0, 0x01, {0xb9}},
    {0xa2, 0x01, {0x03}},
    {0xa3, 0x01, {0xbb}},
    {0xa4, 0x01, {0x03}},
    {0xa5, 0x01, {0xbd}},
    {0xa6, 0x01, {0x03}},
    {0xa7, 0x01, {0xbf}},
    {0xa9, 0x01, {0x03}},
    {0xaa, 0x01, {0xc6}},
    {0xab, 0x01, {0x03}},
    {0xac, 0x01, {0xcd}},
    {0xad, 0x01, {0x03}},
    {0xae, 0x01, {0xce}},
    {0xaf, 0x01, {0x00}},
    {0xb0, 0x01, {0xcd}},
    {0xb1, 0x01, {0x00}},
    {0xb2, 0x01, {0xd4}},
    {0xb3, 0x01, {0x00}},
    {0xb4, 0x01, {0xe1}},
    {0xb5, 0x01, {0x00}},
    {0xb6, 0x01, {0xed}},
    {0xb7, 0x01, {0x00}},
    {0xb8, 0x01, {0xfa}},
    {0xb9, 0x01, {0x01}},
    {0xba, 0x01, {0x05}},
    {0xbb, 0x01, {0x01}},
    {0xbc, 0x01, {0x0f}},
    {0xbd, 0x01, {0x01}},
    {0xbe, 0x01, {0x19}},
    {0xbf, 0x01, {0x01}},
    {0xc0, 0x01, {0x22}},
    {0xc1, 0x01, {0x01}},
    {0xc2, 0x01, {0x44}},
    {0xc3, 0x01, {0x01}},
    {0xc4, 0x01, {0x61}},
    {0xc5, 0x01, {0x01}},
    {0xc6, 0x01, {0x92}},
    {0xc7, 0x01, {0x01}},
    {0xc8, 0x01, {0xba}},
    {0xc9, 0x01, {0x01}},
    {0xca, 0x01, {0xf9}},
    {0xcb, 0x01, {0x02}},
    {0xcc, 0x01, {0x2c}},
    {0xcd, 0x01, {0x02}},
    {0xce, 0x01, {0x2e}},
    {0xcf, 0x01, {0x02}},
    {0xd0, 0x01, {0x5f}},
    {0xd1, 0x01, {0x02}},
    {0xd2, 0x01, {0x97}},
    {0xd3, 0x01, {0x02}},
    {0xd4, 0x01, {0xbe}},
    {0xd5, 0x01, {0x02}},
    {0xd6, 0x01, {0xf4}},
    {0xd7, 0x01, {0x03}},
    {0xd8, 0x01, {0x22}},
    {0xd9, 0x01, {0x03}},
    {0xda, 0x01, {0xb5}},
    {0xdb, 0x01, {0x03}},
    {0xdc, 0x01, {0xb7}},
    {0xdd, 0x01, {0x03}},
    {0xde, 0x01, {0xb9}},
    {0xdf, 0x01, {0x03}},
    {0xe0, 0x01, {0xbb}},
    {0xe1, 0x01, {0x03}},
    {0xe2, 0x01, {0xbd}},
    {0xe3, 0x01, {0x03}},
    {0xe4, 0x01, {0xbf}},
    {0xe5, 0x01, {0x03}},
    {0xe6, 0x01, {0xc6}},
    {0xe7, 0x01, {0x03}},
    {0xe8, 0x01, {0xcd}},
    {0xe9, 0x01, {0x03}},
    {0xea, 0x01, {0xce}},
    {0xff, 0x01, {0x05}},
    {REGFLAG_DELAY, 1,{}},
    {0xfb, 0x01, {0x01}},
    {0x00, 0x01, {0x35}},
    {0x01, 0x01, {0x08}},
    {0x02, 0x01, {0x06}},
    {0x03, 0x01, {0x04}},
    {0x04, 0x01, {0x34}},
    {0x05, 0x01, {0x1a}},
    {0x06, 0x01, {0x1a}},
    {0x07, 0x01, {0x16}},
    {0x08, 0x01, {0x16}},
    {0x09, 0x01, {0x22}},
    {0x0a, 0x01, {0x22}},
    {0x0b, 0x01, {0x1e}},
    {0x0c, 0x01, {0x1e}},
    {0x0d, 0x01, {0x05}},
    {0x0e, 0x01, {0x40}},
    {0x0f, 0x01, {0x40}},
    {0x10, 0x01, {0x40}},
    {0x11, 0x01, {0x40}},
    {0x12, 0x01, {0x40}},
    {0x13, 0x01, {0x40}},
    {0x14, 0x01, {0x35}},
    {0x15, 0x01, {0x09}},
    {0x16, 0x01, {0x07}},
    {0x17, 0x01, {0x04}},
    {0x18, 0x01, {0x34}},
    {0x19, 0x01, {0x1c}},
    {0x1a, 0x01, {0x1c}},
    {0x1b, 0x01, {0x18}},
    {0x1c, 0x01, {0x18}},
    {0x1d, 0x01, {0x24}},
    {0x1e, 0x01, {0x24}},
    {0x1f, 0x01, {0x20}},
    {0x20, 0x01, {0x20}},
    {0x21, 0x01, {0x05}},
    {0x22, 0x01, {0x40}},
    {0x23, 0x01, {0x40}},
    {0x24, 0x01, {0x40}},
    {0x25, 0x01, {0x40}},
    {0x26, 0x01, {0x40}},
    {0x27, 0x01, {0x40}},
    {0x28, 0x01, {0x35}},
    {0x29, 0x01, {0x07}},
    {0x2a, 0x01, {0x09}},
    {0x2b, 0x01, {0x04}},
    {0x2d, 0x01, {0x34}},
    {0x2f, 0x01, {0x20}},
    {0x30, 0x01, {0x20}},
    {0x31, 0x01, {0x24}},
    {0x32, 0x01, {0x24}},
    {0x33, 0x01, {0x18}},
    {0x34, 0x01, {0x18}},
    {0x35, 0x01, {0x1c}},
    {0x36, 0x01, {0x1c}},
    {0x37, 0x01, {0x05}},
    {0x38, 0x01, {0x40}},
    {0x39, 0x01, {0x40}},
    {0x3a, 0x01, {0x40}},
    {0x3b, 0x01, {0x40}},
    {0x3d, 0x01, {0x40}},
    {0x3f, 0x01, {0x40}},
    {0x40, 0x01, {0x35}},
    {0x41, 0x01, {0x06}},
    {0x42, 0x01, {0x08}},
    {0x43, 0x01, {0x04}},
    {0x44, 0x01, {0x34}},
    {0x45, 0x01, {0x1e}},
    {0x46, 0x01, {0x1e}},
    {0x47, 0x01, {0x22}},
    {0x48, 0x01, {0x22}},
    {0x49, 0x01, {0x16}},
    {0x4a, 0x01, {0x16}},
    {0x4b, 0x01, {0x1a}},
    {0x4c, 0x01, {0x1a}},
    {0x4d, 0x01, {0x05}},
    {0x4e, 0x01, {0x40}},
    {0x4f, 0x01, {0x40}},
    {0x50, 0x01, {0x40}},
    {0x51, 0x01, {0x40}},
    {0x52, 0x01, {0x40}},
    {0x53, 0x01, {0x40}},
    {0x54, 0x01, {0x08}},
    {0x55, 0x01, {0x06}},
    {0x56, 0x01, {0x08}},
    {0x58, 0x01, {0x06}},
    {0x59, 0x01, {0x1b}},
    {0x5a, 0x01, {0x1b}},
    {0x5b, 0x01, {0x48}},
    {0x5c, 0x01, {0x0e}},
    {0x5d, 0x01, {0x01}},
    {0x65, 0x01, {0x00}},
    {0x66, 0x01, {0x44}},
    {0x67, 0x01, {0x00}},
    {0x68, 0x01, {0x48}},
    {0x69, 0x01, {0x0e}},
    {0x6a, 0x01, {0x06}},
    {0x6b, 0x01, {0x20}},
    {0x6c, 0x01, {0x08}},
    {0x6d, 0x01, {0x00}},
    {0x76, 0x01, {0x00}},
    {0x77, 0x01, {0x00}},
    {0x78, 0x01, {0x02}},
    {0x79, 0x01, {0x00}},
    {0x7a, 0x01, {0x0a}},
    {0x7b, 0x01, {0x05}},
    {0x7c, 0x01, {0x00}},
    {0x7d, 0x01, {0x0d}},
    {0x7e, 0x01, {0x33}},
    {0x7f, 0x01, {0x33}},
    {0x80, 0x01, {0x33}},
    {0x81, 0x01, {0x00}},
    {0x82, 0x01, {0x00}},
    {0x83, 0x01, {0x00}},
    {0x84, 0x01, {0x30}},
    {0x85, 0x01, {0xff}},
    {0x86, 0x01, {0xff}},
    {0xbb, 0x01, {0x88}},
    {0xb7, 0x01, {0xff}},
    {0xb8, 0x01, {0x00}},
    {0xba, 0x01, {0x13}},
    {0xbc, 0x01, {0x95}},
    {0xbd, 0x01, {0xaa}},
    {0xbe, 0x01, {0x08}},
    {0xbf, 0x01, {0xa3}},
    {0xc8, 0x01, {0x00}},
    {0xc9, 0x01, {0x00}},
    {0xca, 0x01, {0x00}},
    {0xcb, 0x01, {0x00}},
    {0xcc, 0x01, {0x12}},
    {0xcf, 0x01, {0x44}},
    {0xd0, 0x01, {0x00}},
    {0xd1, 0x01, {0x00}},
    {0xd4, 0x01, {0x15}},
    {0xd5, 0x01, {0xbf}},
    {0xd6, 0x01, {0x22}},
    {0x90, 0x01, {0x78}},
    {0x91, 0x01, {0x10}},
    {0x92, 0x01, {0x10}},
    {0x97, 0x01, {0x08}},
    {0x98, 0x01, {0x00}},
    {0x99, 0x01, {0x00}},
    {0x9b, 0x01, {0x68}},
    {0x9c, 0x01, {0x0a}},
    {0xff, 0x01, {0x00}},
    {REGFLAG_DELAY, 1,{}},
    {0x36, 0x01, {0x00}},
    {0xff, 0x01, {0x01}},
    {REGFLAG_DELAY, 1,{}},
    {0xfb, 0x01, {0x01}},
    {0x6e, 0x01, {0x00}},
    {0xff, 0x01, {0x04}},
    {REGFLAG_DELAY, 1,{}},
    {0xfb, 0x01, {0x01}},
    {0x08, 0x01, {0x06}},
    {0xff, 0x01, {0x00}},
    {REGFLAG_DELAY, 1,{}},
    {0xfb, 0x01, {0x01}},
    {0x51, 0x01, {0xff}},
    {0x53, 0x01, {0x24}},
    {0x55, 0x01, {0x00}},
    {0xd3, 0x01, {0x12}},
    {0xd4, 0x01, {0x10}},
    {0x11, 0x00, {}},
    {REGFLAG_DELAY, 120,{}},
    {0x29, 0x00, {}},
    {REGFLAG_DELAY, 20,{}},
    {REGFLAG_END_OF_TABLE, 0x00,{}}
};

static void push_table(struct LCM_setting_table *table, unsigned int count, unsigned char force_update)
{
    unsigned int i;
    for (i = 0; i < count; i++) {
        unsigned cmd;
        cmd = table[i].cmd;

        switch (cmd) {
            case REGFLAG_DELAY:
                if (table[i].count <= 10)
                    MDELAY(table[i].count);
                else
                    MDELAY(table[i].count);
                break;
            case REGFLAG_END_OF_TABLE:
                break;
            default:
                dsi_set_cmdq_V2(cmd, table[i].count, table[i].para_list, force_update);
        }
    }
}

static int get_backlight_pos(struct LCM_setting_table *table, unsigned int count)
{
    int i;
    for (i = count - 1; i >= 0; i--) {
        if (table[i].cmd == 0x51)
            return i;
    }
    return -1;
}

static void tps65132_enable(bool enable){
    int i;
    mt_set_gpio_mode(GPIO_MHL_RST_B_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_MHL_RST_B_PIN, GPIO_DIR_OUT);
    mt_set_gpio_mode(GPIO_MHL_EINT_PIN, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_MHL_EINT_PIN, GPIO_DIR_OUT);
    if (enable) {
        mt_set_gpio_out(GPIO_MHL_EINT_PIN, GPIO_OUT_ONE);
        MDELAY(12);
        mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ONE);
        MDELAY(12);
        for (i = 0; i < 3; i++) {
            if ((tps65132_write_bytes(0, 0xF) & 0x1f) == 0)
                break;
            MDELAY(5);
        }
    } else {
        mt_set_gpio_out(GPIO_MHL_RST_B_PIN, GPIO_OUT_ZERO);
        MDELAY(12);
        mt_set_gpio_out(GPIO_MHL_EINT_PIN, GPIO_OUT_ZERO);
        MDELAY(12);
    }
}

static void boe_senddata(unsigned char value){
    int i;
    raw_spin_lock_irq(&boe_SpinLock);
    mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
    UDELAY(15);
    for (i = 7; i >= 0; i--) {
        if ((value >> i) & 1) {
            mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ZERO);
            UDELAY(10);
            mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
            UDELAY(30);
        } else {
            mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ZERO);
            UDELAY(30);
            mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
            UDELAY(10);
        }
    }
    mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ZERO);
    UDELAY(15);
    mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
    UDELAY(350);
    raw_spin_unlock_irq(&boe_SpinLock);
}

static void lcm_set_util_funcs(const LCM_UTIL_FUNCS *util)
{
    memcpy(&lcm_util, util, sizeof(LCM_UTIL_FUNCS));
}

static void lcm_get_params(LCM_PARAMS *params)
{
    memset(params, 0, sizeof(LCM_PARAMS));

    params->physical_width = 68;
    params->dsi.mode = 1;
    params->dsi.noncont_clock = 1;
    params->dsi.esd_check_enable = 1;
    params->dsi.customization_esd_check_enable = 1;
    params->dsi.lcm_esd_check_table[0].count = 1;
    params->physical_height = 121;
    params->dsi.vertical_backporch = 17;
    params->dsi.vertical_frontporch = 22;
    params->dsi.PLL_CLOCK = 481;
    params->dsi.lcm_esd_check_table[0].cmd = 10;
    params->dsi.lcm_esd_check_table[0].para_list[0] = 0x9C;
    params->type = 2;
    params->dsi.data_format.format = 2;
    params->dsi.PS = 2;
    params->dsi.HS_TRAIL = 2;
    params->dsi.vertical_sync_active = 2;
    params->width = 1080;
    params->dsi.horizontal_active_pixel = 1080;
    params->height = 1920;
    params->dsi.vertical_active_line = 1920;
    params->dsi.switch_mode_enable = 0;
    params->dsi.data_format.color_order = 0;
    params->dsi.data_format.trans_seq = 0;
    params->dsi.data_format.padding = 0;
    params->dsi.LANE_NUM = 4;
    params->dsi.horizontal_sync_active = 4;
    params->dsi.horizontal_backporch = 76;
    params->dsi.horizontal_frontporch = 76;
}

static void lcm_init(void)
{
    tps65132_enable(1);
    MDELAY(20);
    mt_set_gpio_mode(GPIO_LCM_RST, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(5);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(5);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(5);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(5);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(20);
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
    MDELAY(20);
}

static void lcm_suspend(void)
{
    push_table(lcm_suspend_setting, sizeof(lcm_suspend_setting) / sizeof(struct LCM_setting_table), 1);
    tps65132_enable(0);
    MDELAY(10);
    mt_set_gpio_mode(GPIO_LCM_RST, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(20);
}

static void lcm_resume(void)
{
    int bpos;
    tps65132_enable(1);
    MDELAY(15);
    mt_set_gpio_mode(GPIO_LCM_RST, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(5);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(5);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(5);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(5);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(20);
    bpos = get_backlight_pos(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table));
    if (bpos >= 0)
        lcm_initialization_setting[bpos].para_list[0] = (unsigned char)global_brightness_level;
    push_table(lcm_initialization_setting, sizeof(lcm_initialization_setting) / sizeof(struct LCM_setting_table), 1);
}


static unsigned int lcm_compare_id(void)
{
    unsigned int id = 0, vendor_id = 0;
    unsigned char buffer[2];
    unsigned int array[16];

    mt_set_gpio_mode(GPIO_LCM_RST, GPIO_MODE_00);
    mt_set_gpio_dir(GPIO_LCM_RST, GPIO_DIR_OUT);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(1);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ZERO);
    MDELAY(10);
    mt_set_gpio_out(GPIO_LCM_RST, GPIO_OUT_ONE);
    MDELAY(10);

    array[0] = 0x23700;
    dsi_set_cmdq(array, 1, 1);
    array[0] = 0xFF1500;
    dsi_set_cmdq(array, 1, 1);
    array[0] = 0x1FB1500;
    dsi_set_cmdq(array, 1, 1);
    MDELAY(10);
    read_reg_v2(0xF4, buffer, 1);
    id = buffer[0];
    MDELAY(20);
    MDELAY(10);
    read_reg_v2(0x4, buffer, 1);
    vendor_id = buffer[0];
    MDELAY(20);

    return (id == 0x32 && vendor_id == 0x68)?1:0;
}

static void lcm_setbacklight_cmdq(void* handle, unsigned int level)
{
    if (level != boe_value_0) {
        boe_value_0 = level;
        boe_value_1 = level;
        if (boe_value_1 != 0 || boe_value_2 != 0 ) {
            mt_set_gpio_mode(GPIO_MHL_POWER_CTRL_PIN, GPIO_MODE_00);
            mt_set_gpio_dir(GPIO_MHL_POWER_CTRL_PIN, GPIO_DIR_OUT);

            if (level) {
                if (level - 1 > 3) {
                    if (level > 255)
                        level = 255;
                } else
                    level = 4;

                global_brightness_level = level;

                if (level < 31)
                    boe_senddata(64 - level * 2);
                else {
                    if (boe_value_1 >= 31) {
                        if (boe_value_2 >= 31) {
                            mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ONE);
                            MDELAY(10);
                        } else
                            boe_senddata(0);
                    }
                }
            } else {
                mt_set_gpio_out(GPIO_MHL_POWER_CTRL_PIN, GPIO_OUT_ZERO);
                MDELAY(30);
            }

            lcm_backlight_level_setting[3].para_list[0] = (unsigned char)level;
            push_table(lcm_backlight_level_setting, sizeof(lcm_backlight_level_setting) / sizeof(struct LCM_setting_table), 1);
            boe_value_2 = boe_value_1;
        }
    }
}

static void lcm_cabc_enable_cmdq(unsigned int mode)
{
    if (mode)
        push_table(lcm_backlight_enable, sizeof(lcm_backlight_enable) / sizeof(struct LCM_setting_table), 1);
    else
        push_table(lcm_backlight_disable, sizeof(lcm_backlight_disable) / sizeof(struct LCM_setting_table), 1);
}

LCM_DRIVER nt35532_fhd_boe_vdo_lcm_drv = {
    .name = "nt35532_fhd_boe_vdo_lcm",
    .set_util_funcs = lcm_set_util_funcs,
    .get_params = lcm_get_params,
    .init = lcm_init,
    .suspend = lcm_suspend,
    .resume = lcm_resume,
    .compare_id = lcm_compare_id,
    .set_backlight_cmdq = lcm_setbacklight_cmdq,
    .set_pwm = lcm_cabc_enable_cmdq
};
