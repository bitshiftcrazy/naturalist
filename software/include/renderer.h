#ifndef NTRLST_RENDERER_H
#define NTRLST_RENDERER_H

#include <stddef.h>

#include "naturalist.h"

#define NTRLST_SCREEN_WIDTH 600
#define NTRLST_SCREEN_HEIGHT 400

/* quick last minute hack for basic English language support
 * because most people interested in the naturalist on social
 * media don't speak german, needs fixing
 * but a proper localisation system is future kati's problem */
//#define NTRLST_LANG_DE
#define NTRLST_LANG_EN

#if defined(NTRLST_LANG_DE) == defined(NTRLST_LANG_EN)
#error "Select only one language"
#endif

int ntrlst_render_png(const struct ntrlst_deck *deck,
					  const char *software_root,
					  const char *output_path,
					  char *err, size_t err_size);

#endif
