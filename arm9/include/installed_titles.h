#ifndef FRONTEND_INSTALLED_TITLES_H
#define FRONTEND_INSTALLED_TITLES_H
#include "types.h"
void installed_catalog_begin(InstalledCatalog *catalog, const char *nand_root, int language);
/* One directory record/title per step; true on completion or failure.
 * Only ready catalogs may publish entries; status explains failures. */
bool installed_catalog_step(InstalledCatalog *catalog);
void installed_catalog_cancel(InstalledCatalog *catalog);
bool installed_title_path(const InstalledCatalog *catalog, unsigned index, char path[PATH_CAP]);
bool installed_title_revalidate(const InstalledCatalog *catalog, unsigned index);
#endif
