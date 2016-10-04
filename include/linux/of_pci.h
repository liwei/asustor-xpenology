#ifndef __OF_PCI_H
#define __OF_PCI_H

#include <linux/pci.h>
#if defined (CONFIG_SYNO_LSP_MONACO) || defined(CONFIG_SYNO_LSP_ARMADA)
#include <linux/msi.h>
#endif /* CONFIG_SYNO_LSP_MONACO || CONFIG_SYNO_LSP_ARMADA */

struct pci_dev;
#if defined(CONFIG_SYNO_LSP_ARMADA)
struct of_phandle_args;
int of_irq_parse_pci(const struct pci_dev *pdev, struct of_phandle_args *out_irq);
#else /* CONFIG_SYNO_LSP_ARMADA */
struct of_irq;
int of_irq_map_pci(const struct pci_dev *pdev, struct of_irq *out_irq);
#endif /* CONFIG_SYNO_LSP_ARMADA */

struct device_node;
struct device_node *of_pci_find_child_device(struct device_node *parent,
					     unsigned int devfn);
#if defined(CONFIG_SYNO_LSP_ALPINE)
int of_pci_get_devfn(struct device_node *np);
int of_pci_parse_bus_range(struct device_node *node, struct resource *res);
#endif /* CONFIG_SYNO_LSP_ALPINE */

#if defined (CONFIG_SYNO_LSP_MONACO) || defined(CONFIG_SYNO_LSP_ARMADA)
int of_pci_get_devfn(struct device_node *np);
int of_pci_parse_bus_range(struct device_node *node, struct resource *res);

#if defined(CONFIG_OF) && defined(CONFIG_PCI_MSI)
int of_pci_msi_chip_add(struct msi_chip *chip);
void of_pci_msi_chip_remove(struct msi_chip *chip);
struct msi_chip *of_pci_find_msi_chip_by_node(struct device_node *of_node);
#else
static inline int of_pci_msi_chip_add(struct msi_chip *chip) { return -EINVAL; }
static inline void of_pci_msi_chip_remove(struct msi_chip *chip) { }
static inline struct msi_chip *
of_pci_find_msi_chip_by_node(struct device_node *of_node) { return NULL; }
#endif
#endif /* CONFIG_SYNO_LSP_MONACO || CONFIG_SYNO_LSP_ARMADA */

#endif
