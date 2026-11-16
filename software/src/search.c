#include "search.h"

#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

struct trait_weight {
		const char *facet;
		double weight;
};

static const struct trait_weight weights[] = {
		{ "body_form", 5.0 },
		{ "fertile_surface", 5.0 },
		{ "substrate", 2.7 },
		{ "cap_colour", 1.2 },
		{ "fertile_surface_colour", 2.5 },
		{ "cap_surface", 2.3 },
		{ "body_surface", 2.5 },
		{ "ring_presence", 2.1 },
		{ "ring_mobility", 3.0 },
		{ "stem_base", 2.2 },
		{ "volva_presence", 2.6 },
		{ "volva_form", 3.5 },
		{ "stem_surface", 3.0 },
		{ "growth", 1.6 },
		{ "staining", 3.5 },
		{ "habitat", 1.2 },
		{ "nearby_trees", 2.0 },
		{ "cap_shape", 1.6 },
		{ "gill_attachment", 2.3 },
		{ "gill_spacing", 1.5 },
		{ "stem_position", 2.2 },
		{ "special", 3.3 },
		{ "spore_print", 2.5 },
		{ "season", 0.5 },
};

static int fail(char *err, size_t n, const char *fmt, ...)
{
		va_list ap;

		va_start(ap, fmt);
		if (err && n)
				vsnprintf(err, n, fmt, ap);
		va_end(ap);

		return -1;
}

static bool known_value(const struct ntrlst_facet *facet,
						const char *value)
{
		for (size_t i = 0; i < facet->value_count; ++i) {
				if (!strcmp(facet->values[i], value))
						return true;
		}

		return false;
}

static double facet_weight(const char *facet)
{
		for (size_t i = 0; i < sizeof(weights) / sizeof(weights[0]); ++i) {
				if (!strcmp(weights[i].facet, facet))
						return weights[i].weight;
		}

		return 0;
}

static const struct ntrlst_trait *species_trait(
		const struct ntrlst_species *species, const char *facet)
{
		for (size_t i = 0; i < species->trait_count; ++i) {
				if (!strcmp(species->traits[i].facet, facet))
						return &species->traits[i];
		}

		return NULL;
}

static bool contains_value(const struct ntrlst_trait *trait,
						   const char *value)
{
		for (size_t i = 0; i < trait->value_count; ++i) {
				if (!strcmp(trait->values[i], value))
						return true;
		}

		return false;
}

static bool observed_facet(const struct ntrlst_observation *obs,
						   size_t count, const char *facet)
{
		for (size_t i = 0; i < count; ++i) {
				if (!strcmp(obs[i].facet, facet))
						return true;
		}

		return false;
}

static struct ntrlst_match score_species(
		const struct ntrlst_species *species,
		const struct ntrlst_observation *obs, size_t n)
{
		struct ntrlst_match m = { .species = species };

		for (size_t i = 0; i < n; ++i) {
				const char *facet = obs[i].facet;
				const char *value = obs[i].value;
				double w = facet_weight(facet);

				if (!strcmp(facet, "ring_presence") &&
					observed_facet(obs, n, "ring_mobility"))
						w *= 0.2;

				if (!strcmp(value, "absent") &&
					(!strcmp(facet, "ring_presence") ||
					 !strcmp(facet, "volva_presence")))
						w *= 0.55;

				const struct ntrlst_trait *trait = species_trait(species, facet);

				/* Missing traits are neutral */
				if (!trait) {
						++m.unknown;
				} else if (trait->not_applicable) {
						m.score -= w * 1.3;
						++m.conflicts;
				} else if (contains_value(trait, value)) {
						if (!strcmp(facet, "cap_colour") ||
							!strcmp(facet, "fertile_surface_colour")) {
								double factor = 1.0 - 0.1 * (trait->value_count - 1);

								w *= factor < 0.65 ? 0.65 : factor;
						}

						m.score += w;
						++m.hits;
				} else {
						const bool morphology = !strcmp(facet, "body_form") ||
								!strcmp(facet, "fertile_surface");

						m.score -= w * (morphology ? 1.3 : 0.7);
						++m.conflicts;
				}
		}

		m.score = nearbyint(m.score * 1000.0) / 1000.0;

		return m;
}

static bool better(const struct ntrlst_match *a,
				   const struct ntrlst_match *b)
{
		if (a->score != b->score)
				return a->score > b->score;
		if (a->hits != b->hits)
				return a->hits > b->hits;

		return strcmp(a->species->id, b->species->id) < 0;
}

int ntrlst_search(const struct ntrlst_json_hndlr *json_hndlr,
				  const struct ntrlst_observation *obs, size_t n,
				  struct ntrlst_match *out, size_t limit,
				  size_t *result_count, char *err, size_t err_size)
{
		if (!json_hndlr || !obs || !out || !result_count || !json_hndlr->species ||
			!json_hndlr->species_count || !json_hndlr->facets)
				return fail(err, err_size,
							"invalid arguments or empty database");

		*result_count = 0;

		if (n == 0 || n > NTRLST_MAX_OBSERVATIONS)
				return fail(err, err_size, "supply 1 to %d observations",
							NTRLST_MAX_OBSERVATIONS);
		if (limit == 0 || limit > NTRLST_MAX_RESULTS)
				return fail(err, err_size, "limit must be 1 to %d",
							NTRLST_MAX_RESULTS);

		/* TODO: Check contradictions across facets like absent ring and
		 * specified ring mobility, duplicates are. */
		for (size_t i = 0; i < n; ++i) {
				if (!obs[i].facet || !obs[i].value)
						return fail(err, err_size, "null facet or value");

				const struct ntrlst_facet *facet =
						ntrlst_find_facet(json_hndlr, obs[i].facet);

				if (!facet)
						return fail(err, err_size, "unknown facet '%s'",
									obs[i].facet);
				if (!known_value(facet, obs[i].value))
						return fail(err, err_size, "invalid %s=%s",
									obs[i].facet, obs[i].value);
				if (!facet_weight(obs[i].facet))
						return fail(err, err_size, "no weight for %s",
									obs[i].facet);

				for (size_t j = 0; j < i; ++j) {
						if (!strcmp(obs[i].facet, obs[j].facet))
								return fail(err, err_size,
											"duplicate facet '%s'",
											obs[i].facet);
				}
		}

		/* Maintain a sorted top-N, ties are okay. */
		for (size_t i = 0; i < json_hndlr->species_count; ++i) {
				struct ntrlst_match match = score_species(&json_hndlr->species[i],
														  obs, n);

				/* TODO: Revisit this, weak matches are hidden entirely,
				 * which can leave the field guide with no matches */
				if (match.score <= 0.0 || !match.hits)
						continue;

				size_t pos = 0;

				while (pos < *result_count && !better(&match, &out[pos]))
						++pos;
				if (pos >= limit)
						continue;
				if (*result_count < limit)
						++*result_count;
				for (size_t j = *result_count - 1; j > pos; --j)
						out[j] = out[j - 1];

				out[pos] = match;
		}

		if (err && err_size)
				err[0] = '\0';

		return 0;
}
