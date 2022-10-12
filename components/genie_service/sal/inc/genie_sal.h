#ifndef __GENIE_SAL_H__
#define __GENIE_SAL_H__

#include "genie_sal_libc.h"
#include "genie_sal_os.h"

#include "genie_sal_gpio.h"

#ifdef CONFIG_GENIE_OTA
#include "genie_sal_ota.h"
#endif

#ifdef CONFIG_PM_SLEEP
#include "genie_sal_lpm.h"
#endif

#include "genie_sal_ble.h"
#include "genie_sal_uart.h"
#include "genie_sal_provision.h"
#include "genie_sal_storage.h"
#include "genie_sal_event.h"

#endif
