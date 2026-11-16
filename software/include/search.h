#ifndef NTRLST_SEARCH_H
#define NTRLST_SEARCH_H

#include <stddef.h>

#include "json_handler.h"

struct ntrlst_observation {
		const char *facet;
		const char *value;
};

struct ntrlst_match {
		const struct ntrlst_species *species;
		double score;
		size_t hits;
		size_t conflicts;
		size_t unknown;
};

#define NTRLST_MAX_OBSERVATIONS 5
#define NTRLST_MAX_RESULTS 5

/* The score only orders image candidates, it's NOT a probability */
int ntrlst_search(const struct ntrlst_json_hndlr *json_hndlr,
				  const struct ntrlst_observation *observations,
				  size_t observation_count,
				  struct ntrlst_match *out, size_t limit,
				  size_t *result_count, char *err, size_t err_size);

#endif
