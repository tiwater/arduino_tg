#ifndef __TG7121B_LPM_H__
#define __TG7121B_LPM_H__

typedef void (*wakeup_cb_t)(void *arg);

void tg7121b_sleep_enable(void);
void tg7121b_sleep_disable(void);
void tg7121b_standby_enable(void);
uint32_t tg7121b_get_bootup_reason(void);
int32_t tg7121b_set_bootup_reason(uint32_t reason);
void tg7121b_io_wakeup_handler_unregister();
void tg7121b_io_wakeup_handler_register(wakeup_cb_t wakeup_cb);

#endif
