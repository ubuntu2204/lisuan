/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#ifndef __GF_IRQ_H_
#define __GF_IRQ_H_

void gf_irq_init(struct lisuan_device *dev_priv);
void gf_irq_fini(struct lisuan_device *dev_priv);
int gf_irq_install(struct lisuan_device *dev_priv);
void gf_irq_uninstall(struct lisuan_device *dev_priv);
void gf_irq_postinstall(struct lisuan_device *dev_priv);
void gf_irq_reset(struct lisuan_device *ls_dev);

#endif
