#ifndef NTRLST_JSON_HNDLR_H
#define NTRLST_JSON_HNDLR_H

#include <stdbool.h>
#include <stddef.h>

struct ntrlst_trait {
		const char *facet;
		const char **values;
		size_t value_count;
		bool not_applicable;
};

struct ntrlst_species {
		const char *id;
		const char *scientific_name;
		const char *common_name_de;
		const char *common_name_en;
		const char *image;
		struct ntrlst_trait *traits;
		size_t trait_count;
};

struct ntrlst_facet {
		const char *id;
		const char *label_de;
		const char *label_en;
		const char **values;
		size_t value_count;
};

struct ntrlst_shortcut {
		const char *key;
		const char *facet;
		const char *value;
		const char *label_de;
		const char *label_en;
};

struct ntrlst_json_hndlr {
		void *species_json;
		void *vocabulary_json;
		void *shortcuts_json;
		struct ntrlst_species *species;
		size_t species_count;
		struct ntrlst_facet *facets;
		size_t facet_count;
		struct ntrlst_shortcut *shortcuts;
		size_t shortcut_count;
};

int ntrlst_json_hndlr_open(struct ntrlst_json_hndlr *json_hndlr, const char *data_dir,
				   char *err, size_t err_size);
void ntrlst_json_hndlr_close(struct ntrlst_json_hndlr *json_hndlr);

const struct ntrlst_species *ntrlst_find_species(
		const struct ntrlst_json_hndlr *json_hndlr, const char *id);
const struct ntrlst_shortcut *ntrlst_find_shortcut(
		const struct ntrlst_json_hndlr *json_hndlr, const char *key);
const struct ntrlst_facet *ntrlst_find_facet(
		const struct ntrlst_json_hndlr *json_hndlr, const char *id);

#endif
