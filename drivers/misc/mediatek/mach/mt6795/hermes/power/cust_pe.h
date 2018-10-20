#ifndef _CUST_PE_H_
#define _CUST_PE_H_

/* Pump Express support (fast charging) */
#ifdef CONFIG_MTK_PUMP_EXPRESS_PLUS_SUPPORT
#define TA_START_BATTERY_SOC	1
#define TA_STOP_BATTERY_SOC 	95
#define TA_AC_9V_INPUT_CURRENT	CHARGE_CURRENT_2000_00_MA
#define TA_AC_7V_INPUT_CURRENT	CHARGE_CURRENT_2000_00_MA
#define TA_AC_CHARGING_CURRENT	CHARGE_CURRENT_2000_00_MA
#define TA_9V_SUPPORT

#undef V_CHARGER_MAX
#ifdef TA_9V_SUPPORT
#define V_CHARGER_MAX 13000				// 10.5 V
#else
#define V_CHARGER_MAX 13000				// 7.5 V
#endif
#endif

#endif /* _CUST_PE_H_ */
