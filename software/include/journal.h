#ifndef NTRLST_JOURNAL_H
#define NTRLST_JOURNAL_H

#include <stdbool.h>
#include <stddef.h>

#include "json_handler.h"

#define NTRLST_WALK_MAX_FINDINGS 128
#define NTRLST_WALK_LOCATION_SIZE 96
#define NTRLST_WALK_NOTE_SIZE 256

struct ntrlst_finding {
		char species_id[96];
		char recorded_at[48];
		char note[NTRLST_WALK_NOTE_SIZE];
};

struct ntrlst_journal {
		char directory[1024];
		char id[64];
		char started_at[48];
		char location[NTRLST_WALK_LOCATION_SIZE];
		char note[NTRLST_WALK_NOTE_SIZE];
		struct ntrlst_finding findings[NTRLST_WALK_MAX_FINDINGS];
		size_t finding_count;
		bool active;
};

int ntrlst_journal_open(struct ntrlst_journal *j, const char *directory,
						const struct ntrlst_json_hndlr *json_hndlr, char *err, size_t n);
int ntrlst_journal_start(struct ntrlst_journal *j, char *err, size_t n);
int ntrlst_journal_finish(struct ntrlst_journal *j,
						  char *err, size_t n);
int ntrlst_journal_set_location(struct ntrlst_journal *j,
								const char *text, char *err, size_t n);
int ntrlst_journal_set_note(struct ntrlst_journal *j,
							const char *text, char *err, size_t n);
int ntrlst_journal_set_finding_note(struct ntrlst_journal *j,
									const char *id, const char *text,
									char *err, size_t n);
int ntrlst_journal_add(struct ntrlst_journal *j, const char *id,
					   char *err, size_t n);
int ntrlst_journal_remove(struct ntrlst_journal *j, const char *id,
						  char *err, size_t n);
const struct ntrlst_finding *ntrlst_journal_find(
		const struct ntrlst_journal *j, const char *id);

#endif
