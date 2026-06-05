#ifndef __REGULATOR_KEY_H
#define __REGULATOR_KEY_H

extern struct pmic_keys_info *pwrkey;
extern irqreturn_t mtk_pmic_keys_irq_handler_thread(int pressed, void *data);

#endif
