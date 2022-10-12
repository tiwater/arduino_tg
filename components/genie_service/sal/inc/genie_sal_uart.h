#ifndef __GENIE_SAL_UART_H__
#define __GENIE_SAL_UART_H__

#ifdef CONFIG_GENIE_MESH_PORT_YOC
#include "ulog/ulog.h"
#ifndef GENIE_MESH_DEFAULT_LOG_LEVEL
#define GENIE_MESH_DEFAULT_LOG_LEVEL LOG_DEBUG
#endif

int32_t genie_sal_uart_send_str(const char *fmt, ...);
#else
#define genie_sal_uart_send_str aos_cli_printf
#endif

int genie_sal_uart_init(void);
int32_t genie_sal_uart_send_one_byte(uint8_t byte);

#endif
