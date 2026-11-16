#ifndef NTRLST_H
#define NTRLST_H

#include <stdbool.h>
#include <stddef.h>

#include "search.h"
#include "journal.h"

enum ntrlst_screen {
		NTRLST_SCREEN_SEARCH,
		NTRLST_SCREEN_RESULTS,
		NTRLST_SCREEN_DETAIL,
		NTRLST_SCREEN_BASKET,
		NTRLST_SCREEN_CHEAT,
		NTRLST_SCREEN_CONTEXT,
		NTRLST_SCREEN_POWEROFF,
		NTRLST_SCREEN_WALK,
		NTRLST_SCREEN_EDITOR,
		NTRLST_SCREEN_END_WALK,
		NTRLST_SCREEN_REMOVE_FINDING,
};

enum ntrlst_change {
		NTRLST_DECK_ERROR = -1,
		NTRLST_DECK_NOOP = 0,
		NTRLST_DECK_BUFFERED = 1,
		NTRLST_DECK_REDRAW = 2,
		NTRLST_DECK_SHUTDOWN = 3,
};

#define NTRLST_BASKET_PAGE_SIZE 5
#define NTRLST_CHEAT_PAGES 2

enum ntrlst_edit_target {
		NTRLST_EDIT_LOCATION,
		NTRLST_EDIT_WALK_NOTE,
		NTRLST_EDIT_FINDING_NOTE
};

struct ntrlst_deck {
		const struct ntrlst_json_hndlr *json_hndlr;
		struct ntrlst_journal *journal;
		enum ntrlst_screen screen;

		struct ntrlst_observation obs[NTRLST_MAX_OBSERVATIONS];
		const char *obs_keys[NTRLST_MAX_OBSERVATIONS];
		size_t obs_count;

		struct ntrlst_match matches[NTRLST_MAX_RESULTS];
		size_t match_count;
		const struct ntrlst_species *selected;

		const struct ntrlst_species **basket;
		size_t basket_count;
		size_t basket_capacity;
		size_t basket_page;

		enum ntrlst_screen help_return;
		unsigned cheat_page;

		enum ntrlst_screen basket_return;
		enum ntrlst_screen detail_return;
		enum ntrlst_screen saved_detail_return;
		const struct ntrlst_species *saved_detail_selected;
		bool saved_detail_active;

		enum ntrlst_screen walk_return;
		bool walk_just_finished;

		enum ntrlst_screen poweroff_return;

		enum ntrlst_screen editor_return;
		enum ntrlst_edit_target edit_target;
		const struct ntrlst_species *edit_species;
		char edit_text[NTRLST_WALK_NOTE_SIZE];
		size_t edit_length;
};

int ntrlst_deck_init(struct ntrlst_deck *deck,
					const struct ntrlst_json_hndlr *json_hndlr);
void ntrlst_deck_destroy(struct ntrlst_deck *deck);

int ntrlst_deck_attach_journal(struct ntrlst_deck *deck,
							  struct ntrlst_journal *journal);

enum ntrlst_change ntrlst_deck_press(struct ntrlst_deck *deck,
									const char *key,
									char *err, size_t err_size);

bool ntrlst_deck_in_basket(const struct ntrlst_deck *deck,
						  const struct ntrlst_species *species);
const char *ntrlst_screen_name(enum ntrlst_screen screen);

#endif
