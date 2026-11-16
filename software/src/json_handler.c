#include "json_handler.h"

#include <jansson.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(char *err, size_t size, const char *fmt, ...)
{
		va_list ap;

		va_start(ap, fmt);

		if (size)
				vsnprintf(err, size, fmt, ap);

		va_end(ap);

		return -1;
}

static const char *string(json_t *obj, const char *field)
{
		json_t *value = json_object_get(obj, field);
		const char *s = json_string_value(value);

		return s && *s ? s : NULL;
}

static json_t *open_json(const char *dir, const char *name,
						 char *err, size_t n)
{
		char path[4096];
		json_error_t e;
		json_t *root;
		int len;

		len = snprintf(path, sizeof(path), "%s/%s", dir, name);
		if (len < 0 || (size_t)len >= sizeof(path)) {
				fail(err, n, "path too long");
				return NULL;
		}

		root = json_load_file(path, JSON_REJECT_DUPLICATES, &e);
		if (!root) {
				fail(err, n, "%s:%d:%d: %s", path, e.line, e.column, e.text);
		} else if (!json_is_object(root)) {
				fail(err, n, "%s: expected top-level object", path);
				json_decref(root);
				return NULL;
		}

		return root;
}

/*
 * note to future me:
 * schema versions feel a bit like an overkill for a small personal project
 * but I'm paranoid and quite a few people already asked how to contribute
 * or said they want to build a naturalist for themselves, and I don't
 * want to change sth in my data in 6 months and then someone can't load
 * their mushroom walk...
 * tl;dr: keep it
 */
static int check_schema(json_t *root, int expected, const char *which,
						char *err, size_t n)
{
		json_t *version = json_object_get(root, "schema_version");

		if (!json_is_integer(version) ||
			json_integer_value(version) != expected)
				return fail(err, n, "%s: schema_version must be %d",
							which, expected);

		return 0;
}

const struct ntrlst_facet *ntrlst_find_facet(
		const struct ntrlst_json_hndlr *json_hndlr, const char *id)
{
		size_t i;

		for (i = 0; i < json_hndlr->facet_count; i++) {
				if (!strcmp(json_hndlr->facets[i].id, id))
						return &json_hndlr->facets[i];
		}

		return NULL;
}

const struct ntrlst_species *ntrlst_find_species(
		const struct ntrlst_json_hndlr *json_hndlr, const char *id)
{
		size_t i;

		for (i = 0; i < json_hndlr->species_count; i++) {
				if (!strcmp(json_hndlr->species[i].id, id))
						return &json_hndlr->species[i];
		}

		return NULL;
}

const struct ntrlst_shortcut *ntrlst_find_shortcut(
		const struct ntrlst_json_hndlr *json_hndlr, const char *key)
{
		size_t i;

		for (i = 0; i < json_hndlr->shortcut_count; i++) {
				if (!strcmp(json_hndlr->shortcuts[i].key, key))
						return &json_hndlr->shortcuts[i];
		}

		return NULL;
}

static bool has_value(const struct ntrlst_facet *facet, const char *value)
{
		size_t i;

		for (i = 0; i < facet->value_count; i++) {
				if (!strcmp(facet->values[i], value))
						return true;
		}

		return false;
}

static int load_facets(struct ntrlst_json_hndlr *json_hndlr, json_t *root,
					   char *err, size_t n)
{
		json_t *array = json_object_get(root, "facets");
		size_t i, j, k;

		if (!json_is_array(array) || !json_array_size(array))
				return fail(err, n, "facets must be a nonempty array");

		json_hndlr->facet_count = json_array_size(array);
		json_hndlr->facets = calloc(json_hndlr->facet_count, sizeof(*json_hndlr->facets));
		if (!json_hndlr->facets)
				return fail(err, n, "OOM: facets");

		for (i = 0; i < json_hndlr->facet_count; i++) {
				json_t *item = json_array_get(array, i);
				struct ntrlst_facet *f = &json_hndlr->facets[i];
				json_t *values;

				if (!json_is_object(item) ||
					!(f->id = string(item, "id")) ||
					!(f->label_de = string(item, "label_de")) ||
					!(f->label_en = string(item, "label_en")))
						return fail(err, n, "facet %zu missing id/label", i);

				for (j = 0; j < i; j++) {
						if (!strcmp(json_hndlr->facets[j].id, f->id))
								return fail(err, n, "dup facet %s", f->id);
				}

				values = json_object_get(item, "values");
				if (!json_is_array(values) || !json_array_size(values))
						return fail(err, n, "%s has no values", f->id);

				f->value_count = json_array_size(values);
				f->values = calloc(f->value_count, sizeof(*f->values));
				if (!f->values)
						return fail(err, n, "OOM: values");

				for (j = 0; j < f->value_count; j++) {
						json_t *v = json_array_get(values, j);
						const char *id = string(v, "id");

						if (!json_is_object(v) || !id ||
							!string(v, "label_de") || !string(v, "label_en"))
								return fail(err, n, "bad value for %s", f->id);

						for (k = 0; k < j; k++) {
								if (!strcmp(f->values[k], id))
										return fail(err, n, "duplicate %s=%s",
													f->id, id);
						}

						f->values[j] = id;
				}
		}

		return 0;
}

static int load_species(struct ntrlst_json_hndlr *json_hndlr, json_t *root,
						char *err, size_t n)
{
		json_t *array = json_object_get(root, "mushrooms");
		size_t i, j, k;

		if (!json_is_array(array) || !json_array_size(array))
				return fail(err, n, "expected non empty mushrooms array");

		json_hndlr->species_count = json_array_size(array);
		json_hndlr->species = calloc(json_hndlr->species_count, sizeof(*json_hndlr->species));
		if (!json_hndlr->species)
				return fail(err, n, "OOM: mushrooms");

		for (i = 0; i < json_hndlr->species_count; i++) {
				json_t *item = json_array_get(array, i);
				struct ntrlst_species *s = &json_hndlr->species[i];
				json_t *names_de, *names_en, *traits;
				char expected[512];
				void *iter;
				size_t t = 0;
				int len;

				if (!json_is_object(item) ||
					!(s->id = string(item, "id")) ||
					!(s->scientific_name = string(item, "scientific_name")) ||
					!(s->image = string(item, "image")))
						return fail(err, n, "entry %zu missing id/name/image", i);

				for (j = 0; j < i; j++) {
						if (!strcmp(json_hndlr->species[j].id, s->id))
								return fail(err, n, "dup species %s", s->id);
				}

				names_de = json_object_get(item, "common_names_de");
				if (!json_is_array(names_de) || !json_array_size(names_de) ||
					!(s->common_name_de = json_string_value(json_array_get(names_de, 0))) ||
					!*s->common_name_de)
						return fail(err, n, "%s has no German name", s->id);

				for (j = 0; j < json_array_size(names_de); j++) {
						if (!json_is_string(json_array_get(names_de, j)))
								return fail(err, n, "%s has invalid common_names_de",
											s->id);
				}

				names_en = json_object_get(item, "common_names_en");
				if (!json_is_array(names_en) || !json_array_size(names_en) ||
					!(s->common_name_en = json_string_value(json_array_get(names_en, 0))) ||
					!*s->common_name_en)
						return fail(err, n, "%s has no English name", s->id);

				for (j = 0; j < json_array_size(names_en); j++) {
						if (!json_is_string(json_array_get(names_en, j)))
								return fail(err, n, "%s has invalid common_names_en",
											s->id);
				}

				len = snprintf(expected, sizeof(expected),
							   "assets/mushrooms/%s.png", s->id);
				if (len < 0 || (size_t)len >= sizeof(expected) ||
					strcmp(expected, s->image))
						return fail(err, n, "unexpected image path for %s",
									s->id);

				traits = json_object_get(item, "traits");
				if (!json_is_object(traits) || !json_object_size(traits))
						return fail(err, n, " %s has no traits", s->id);

				s->trait_count = json_object_size(traits);
				s->traits = calloc(s->trait_count, sizeof(*s->traits));
				if (!s->traits)
						return fail(err, n, "OOM: traits");

				iter = json_object_iter(traits);
				while (iter) {
						const char *facet_id = json_object_iter_key(iter);
						json_t *values = json_object_iter_value(iter);
						const struct ntrlst_facet *f;
						struct ntrlst_trait *trait = &s->traits[t++];

						f = ntrlst_find_facet(json_hndlr, facet_id);
						trait->facet = facet_id;
						if (!f)
								return fail(err, n, "%s unknown facet %s",
											s->id, facet_id);

						if (json_is_null(values)) {
								trait->not_applicable = true;
						} else if (json_is_array(values) && json_array_size(values)) {
								trait->value_count = json_array_size(values);
								trait->values = calloc(trait->value_count,
													   sizeof(*trait->values));
								if (!trait->values)
										return fail(err, n, "OOM: trait values");

								for (j = 0; j < trait->value_count; j++) {
										const char *v = json_string_value(json_array_get(values, j));

										if (!v || !has_value(f, v))
												return fail(err, n, "%s invalid %s[%zu]",
															s->id, facet_id, j);

										for (k = 0; k < j; k++) {
												if (!strcmp(trait->values[k], v))
														return fail(err, n, "%s duplicated %s=%s",
																	s->id, facet_id, v);
										}

										trait->values[j] = v;
								}
						} else {
								return fail(err, n,
											"%s/%s must be nonempty array or null",
											s->id, facet_id);
						}

						iter = json_object_iter_next(traits, iter);
				}
		}

		return 0;
}

static int load_shortcuts(struct ntrlst_json_hndlr *json_hndlr, json_t *root,
						  char *err, size_t n)
{
		json_t *array = json_object_get(root, "bindings");
		size_t i, j;

		if (!json_is_array(array) || !json_array_size(array))
				return fail(err, n, "bindings must be a non empty array");

		json_hndlr->shortcut_count = json_array_size(array);
		json_hndlr->shortcuts = calloc(json_hndlr->shortcut_count, sizeof(*json_hndlr->shortcuts));
		if (!json_hndlr->shortcuts)
				return fail(err, n, "OOM: shortcuts");

		for (i = 0; i < json_hndlr->shortcut_count; i++) {
				json_t *item = json_array_get(array, i);
				struct ntrlst_shortcut *b = &json_hndlr->shortcuts[i];
				const struct ntrlst_facet *f;

				if (!json_is_object(item) ||
					!(b->key = string(item, "key")) ||
					!(b->facet = string(item, "facet")) ||
					!(b->value = string(item, "value")) ||
					!(b->label_de = string(item, "label_de")) ||
					!(b->label_en = string(item, "label_en")))
						return fail(err, n, "binding %zu missing required field", i);

				for (j = 0; j < i; j++) {
						if (!strcmp(json_hndlr->shortcuts[j].key, b->key))
								return fail(err, n, "duplicate key %s", b->key);
				}

				f = ntrlst_find_facet(json_hndlr, b->facet);
				if (!f || !has_value(f, b->value))
						return fail(err, n, "%s uses unknown trait %s=%s",
									b->key, b->facet, b->value);
		}

		return 0;
}

int ntrlst_json_hndlr_open(struct ntrlst_json_hndlr *json_hndlr, const char *dir,
				   char *err, size_t n)
{
		json_t *vocab, *species, *shortcuts;
		const char *version, *species_version, *shortcuts_version;

		if (!json_hndlr || !dir)
				return fail(err, n, "Invalid database arguments");

		memset(json_hndlr, 0, sizeof(*json_hndlr));

		json_hndlr->vocabulary_json = open_json(dir, "trait_vocabulary.json", err, n);
		if (!json_hndlr->vocabulary_json)
				goto error;

		json_hndlr->species_json = open_json(dir, "mushrooms.json", err, n);
		if (!json_hndlr->species_json)
				goto error;

		json_hndlr->shortcuts_json = open_json(dir, "shortcuts.json", err, n);
		if (!json_hndlr->shortcuts_json)
				goto error;

		vocab = json_hndlr->vocabulary_json;
		species = json_hndlr->species_json;
		shortcuts = json_hndlr->shortcuts_json;

		if (check_schema(vocab, 2, "vocabulary", err, n) ||
			check_schema(species, 2, "mushrooms", err, n) ||
			check_schema(shortcuts, 1, "shortcuts", err, n))
				goto error;

		version = string(vocab, "vocabulary_version");
		species_version = string(species, "trait_vocabulary_version");
		shortcuts_version = string(shortcuts, "trait_vocabulary_version");

		if (!version || !species_version || !shortcuts_version ||
			strcmp(version, species_version) ||
			strcmp(version, shortcuts_version)) {
				fail(err, n, "Trait vocabulary versions disagree between JSON files");
				goto error;
		}

		if (load_facets(json_hndlr, vocab, err, n) ||
			load_species(json_hndlr, species, err, n) ||
			load_shortcuts(json_hndlr, shortcuts, err, n))
				goto error;

		return 0;

error:
		ntrlst_json_hndlr_close(json_hndlr);
		return -1;
}

void ntrlst_json_hndlr_close(struct ntrlst_json_hndlr *json_hndlr)
{
		size_t i, j;

		if (!json_hndlr)
				return;

		if (json_hndlr->species) {
				for (i = 0; i < json_hndlr->species_count; i++) {
						struct ntrlst_species *s = &json_hndlr->species[i];

						if (s->traits) {
								for (j = 0; j < s->trait_count; j++)
										free(s->traits[j].values);
						}

						free(s->traits);
				}
		}

		if (json_hndlr->facets) {
				for (i = 0; i < json_hndlr->facet_count; i++)
						free(json_hndlr->facets[i].values);
		}

		free(json_hndlr->species);
		free(json_hndlr->facets);
		free(json_hndlr->shortcuts);

		json_decref(json_hndlr->species_json);
		json_decref(json_hndlr->vocabulary_json);
		json_decref(json_hndlr->shortcuts_json);

		memset(json_hndlr, 0, sizeof(*json_hndlr));
}
