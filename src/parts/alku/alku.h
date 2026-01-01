/**
 * ALKU (Opening Credits) Part
 *
 * Displays "A Future Crew Production" text with fades over scrolling horizon.
 * Port of the original ALKU/MAIN.C to cross-platform Sokol framework.
 */

#ifndef ALKU_H
#define ALKU_H

#include "part.h"

/**
 * Get the ALKU part structure for registration with part loader.
 * @return Pointer to static sr_part_t structure
 */
sr_part_t *alku_get_part(void);

#endif /* ALKU_H */
