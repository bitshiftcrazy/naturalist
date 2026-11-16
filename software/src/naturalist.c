#include "naturalist.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static enum ntrlst_change fail(char *err, size_t size, const char *reason)
{
		if (err && size)
				snprintf(err, size, "%s", reason);

		return NTRLST_DECK_ERROR;
}

const char *ntrlst_screen_name(enum ntrlst_screen screen)
{
		switch (screen) {
		case NTRLST_SCREEN_SEARCH:
				return "search";
		case NTRLST_SCREEN_RESULTS:
				return "results";
		case NTRLST_SCREEN_DETAIL:
				return "detail";
		case NTRLST_SCREEN_BASKET:
				return "basket";
		case NTRLST_SCREEN_CHEAT:
				return "cheat";
		case NTRLST_SCREEN_CONTEXT:
				return "context";
		case NTRLST_SCREEN_POWEROFF:
				return "poweroff";
		case NTRLST_SCREEN_WALK:
				return "walk";
		case NTRLST_SCREEN_EDITOR:
				return "editor";
		case NTRLST_SCREEN_END_WALK:
				return "end_walk";
		case NTRLST_SCREEN_REMOVE_FINDING:
				return "remove_finding";
		}
		return "invalid";
}

int ntrlst_deck_init(struct ntrlst_deck *deck,
					const struct ntrlst_json_hndlr *json_hndlr)
{
		if (!deck || !json_hndlr)
				return -1;

		memset(deck, 0, sizeof(*deck));

		deck->basket = calloc(json_hndlr->species_count, sizeof(*deck->basket));
		if (!deck->basket)
				return -1;

		deck->basket_capacity = json_hndlr->species_count;
		deck->json_hndlr = json_hndlr;
		deck->screen = NTRLST_SCREEN_SEARCH;

		return 0;
}

int ntrlst_deck_attach_journal(struct ntrlst_deck *deck,
							  struct ntrlst_journal *journal)
{
		if (!deck || !deck->json_hndlr || !journal)
				return -1;

		if (journal->finding_count > deck->basket_capacity)
				return -1;

		for (size_t i = 0; i < journal->finding_count; ++i) {
				const struct ntrlst_species *sp = ntrlst_find_species(
						deck->json_hndlr, journal->findings[i].species_id);

				if (!sp)
						return -1;

				deck->basket[i] = sp;
		}

		deck->basket_count = journal->finding_count;
		deck->journal = journal;

		return 0;
}

void ntrlst_deck_destroy(struct ntrlst_deck *deck)
{
		if (!deck)
				return;

		free(deck->basket);
		memset(deck, 0, sizeof(*deck));
}

bool ntrlst_deck_in_basket(const struct ntrlst_deck *deck,
						  const struct ntrlst_species *species)
{
		if (!deck || !species)
				return false;

		for (size_t i = 0; i < deck->basket_count; i++)
				if (deck->basket[i] == species)
						return true;

		return false;
}

static size_t basket_pages(const struct ntrlst_deck *deck)
{
		size_t n = (deck->basket_count + NTRLST_BASKET_PAGE_SIZE - 1) /
				NTRLST_BASKET_PAGE_SIZE;

		return n ? n : 1;
}

static int selection_slot(const char *key)
{
		if (strlen(key) == 1) {
				if (key[0] >= 'A' && key[0] <= 'E')
						return key[0] - 'A';
				if (key[0] >= '1' && key[0] <= '5')
						return key[0] - '1';
		}
		return -1;
}

static bool is_key(const char *key, const char *value)
{
		return !strcmp(key, value);
}

static void remove_observation(struct ntrlst_deck *deck, size_t index)
{
		for (size_t i = index; i + 1 < deck->obs_count; ++i) {
				deck->obs[i] = deck->obs[i + 1];
				deck->obs_keys[i] = deck->obs_keys[i + 1];
		}

		--deck->obs_count;
}

static enum ntrlst_change buffer_trait(
		struct ntrlst_deck *deck,
		const struct ntrlst_shortcut *binding)
{
		for (size_t i = 0; i < deck->obs_count; i++) {
				if (strcmp(deck->obs[i].facet, binding->facet))
						continue;
				if (!strcmp(deck->obs[i].value, binding->value)) {
						remove_observation(deck, i);
				} else {
						remove_observation(deck, i);
						deck->obs[deck->obs_count] =
								(struct ntrlst_observation){
								.facet = binding->facet,
								.value = binding->value
						};
						deck->obs_keys[deck->obs_count++] = binding->key;
				}

				return NTRLST_DECK_BUFFERED;
		}

		if (deck->obs_count >= NTRLST_MAX_OBSERVATIONS)
				return NTRLST_DECK_NOOP;

		deck->obs[deck->obs_count] = (struct ntrlst_observation){
				.facet = binding->facet,
				.value = binding->value
		};
		deck->obs_keys[deck->obs_count++] = binding->key;

		return NTRLST_DECK_BUFFERED;
}

static enum ntrlst_change toggle_basket(struct ntrlst_deck *deck)
{
		for (size_t i = 0; i < deck->basket_count; i++) {
				if (deck->basket[i] != deck->selected)
						continue;
				if (deck->journal && deck->journal->active) {
						char err[256] = {0};
						if (ntrlst_journal_remove(deck->journal,
												  deck->selected->id, err, sizeof(err)))
								return NTRLST_DECK_ERROR;
				}

				for (size_t j = i; j + 1 < deck->basket_count; j++)
						deck->basket[j] = deck->basket[j + 1];
				--deck->basket_count;

				return NTRLST_DECK_REDRAW;
		}

		if (deck->basket_count >= deck->basket_capacity)
				return NTRLST_DECK_ERROR;

		if (deck->journal && deck->journal->active) {
				char err[256] = {0};
				if (ntrlst_journal_add(deck->journal, deck->selected->id,
									   err, sizeof(err)))
						return NTRLST_DECK_ERROR;
		}

		deck->basket[deck->basket_count++] = deck->selected;

		return NTRLST_DECK_REDRAW;
}

static void begin_edit(struct ntrlst_deck *deck,
					   enum ntrlst_edit_target target,
					   enum ntrlst_screen ret,
					   const struct ntrlst_species *species,
					   const char *current)
{
		deck->edit_target = target;
		deck->editor_return = ret;
		deck->edit_species = species;
		snprintf(deck->edit_text, sizeof(deck->edit_text), "%s",
				 current ? current : "");
		deck->edit_length = strlen(deck->edit_text);
		deck->screen = NTRLST_SCREEN_EDITOR;
}

static enum ntrlst_change edit_key(struct ntrlst_deck *deck,
								   const char *key, char *err, size_t n)
{
		if (is_key(key, "Escape")) {
				deck->screen = deck->editor_return;
				return NTRLST_DECK_REDRAW;
		}

		if (is_key(key, "Tab"))
				return NTRLST_DECK_REDRAW;
		if (is_key(key, "Enter")) {
				if (!deck->journal || !deck->journal->active)
						return fail(err, n, "No active walk");

				int result;
				switch (deck->edit_target) {
				case NTRLST_EDIT_LOCATION:
						result = ntrlst_journal_set_location(deck->journal,
															 deck->edit_text, err, n);
						break;
				case NTRLST_EDIT_WALK_NOTE:
						result = ntrlst_journal_set_note(deck->journal,
														 deck->edit_text, err, n);
						break;
				case NTRLST_EDIT_FINDING_NOTE:
						if (!deck->edit_species)
								return fail(err, n, "No selected finding");
						result = ntrlst_journal_set_finding_note(
								deck->journal, deck->edit_species->id,
								deck->edit_text, err, n);
						break;
				default:
						return fail(err, n, "Invalid editor target");
				}
				if (result)
						return NTRLST_DECK_ERROR;
				deck->screen = deck->editor_return;

				return NTRLST_DECK_REDRAW;
		}

		if (is_key(key, "Backspace")) {
				if (deck->edit_length)
						deck->edit_text[--deck->edit_length] = 0;

				return NTRLST_DECK_BUFFERED;
		}

		char letter = 0;
		if (strlen(key) == 1 && ((key[0] >= 'A' && key[0] <= 'Z') ||
								 (key[0] >= '0' && key[0] <= '9') ||
								 key[0] == '.' || key[0] == ',' ||
								 key[0] == '-')) {
				letter = key[0] >= 'A' && key[0] <= 'Z'
						? (char)(key[0] - 'A' + 'a') : key[0];
		} else if (!strncmp(key, "Shift+", 6) && strlen(key) == 7 &&
				   key[6] >= 'A' && key[6] <= 'Z')
				letter = key[6];
		else if (is_key(key, "Space"))
				letter = ' ';

		size_t limit = deck->edit_target == NTRLST_EDIT_LOCATION
				? NTRLST_WALK_LOCATION_SIZE : NTRLST_WALK_NOTE_SIZE;

		if (letter && deck->edit_length + 1 < limit) {
				deck->edit_text[deck->edit_length++] = letter;
				deck->edit_text[deck->edit_length] = 0;

				return NTRLST_DECK_BUFFERED;
		}

		return NTRLST_DECK_NOOP;
}

static enum ntrlst_change run_search(struct ntrlst_deck *deck,
									 char *err, size_t err_size)
{
		if (!deck->obs_count)
				return NTRLST_DECK_NOOP;

		size_t count = 0;

		if (ntrlst_search(deck->json_hndlr, deck->obs, deck->obs_count,
						  deck->matches, NTRLST_MAX_RESULTS, &count,
						  err, err_size))
				return NTRLST_DECK_ERROR;

		deck->match_count = count;
		deck->screen = NTRLST_SCREEN_RESULTS;

		return NTRLST_DECK_REDRAW;
}

enum ntrlst_change ntrlst_deck_press(struct ntrlst_deck *deck,
									const char *key,
									char *err, size_t err_size)
{
		if (!deck || !deck->json_hndlr || !key)
				return fail(err, err_size, "Invalid deck or key");
		if (err && err_size)
				err[0] = '\0';

		if (deck->screen == NTRLST_SCREEN_POWEROFF) {
				if (is_key(key, "Escape") || is_key(key, "Backspace") ||
					is_key(key, "Ctrl+Alt+P")) {
						deck->screen = deck->poweroff_return;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Shift+P"))
						return NTRLST_DECK_SHUTDOWN;

				return NTRLST_DECK_NOOP;
		}

		if (is_key(key, "Ctrl+Alt+P")) {
				deck->poweroff_return = deck->screen;
				deck->screen = NTRLST_SCREEN_POWEROFF;
				return NTRLST_DECK_REDRAW;
		}


		if (is_key(key, "Ctrl+Alt+W") && deck->journal &&
			deck->screen != NTRLST_SCREEN_EDITOR &&
			deck->screen != NTRLST_SCREEN_END_WALK &&
			deck->screen != NTRLST_SCREEN_REMOVE_FINDING) {
				if (deck->screen == NTRLST_SCREEN_WALK)
						deck->screen = deck->walk_return;
				else {
						deck->walk_return = deck->screen;
						deck->walk_just_finished = false;
						deck->screen = NTRLST_SCREEN_WALK;
				}

				return NTRLST_DECK_REDRAW;
		}

		if (deck->screen == NTRLST_SCREEN_EDITOR)
				return edit_key(deck, key, err, err_size);

		if (deck->screen == NTRLST_SCREEN_END_WALK) {
				if (is_key(key, "Escape") || is_key(key, "Backspace")) {
						deck->screen = NTRLST_SCREEN_WALK;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Shift+E")) {
						if (!deck->journal ||
							ntrlst_journal_finish(deck->journal, err,
												  err_size))
								return NTRLST_DECK_ERROR;

						deck->walk_just_finished = true;
						deck->basket_count = 0;
						deck->basket_page = 0;
						deck->screen = NTRLST_SCREEN_WALK;

						return NTRLST_DECK_REDRAW;
				}

				return NTRLST_DECK_NOOP;
		}

		if (deck->screen == NTRLST_SCREEN_REMOVE_FINDING) {
				if (is_key(key, "Escape") || is_key(key, "Backspace")) {
						deck->screen = NTRLST_SCREEN_DETAIL;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Shift+B")) {
						enum ntrlst_change change = toggle_basket(deck);
						if (change == NTRLST_DECK_ERROR)
								return fail(err, err_size,
											"Cannot remove finding");
						deck->screen = NTRLST_SCREEN_DETAIL;

						return NTRLST_DECK_REDRAW;
				}

				return NTRLST_DECK_NOOP;
		}

		/* Searching is possible from the cheatsheet, too */
		if (deck->screen == NTRLST_SCREEN_CHEAT) {
				if (is_key(key, "Escape") || is_key(key, "Tab") ||
					is_key(key, "F1") || is_key(key, "F2")) {
						deck->screen = deck->help_return;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Enter"))
						return run_search(deck, err, err_size);
				if (is_key(key, "Backspace")) {
						if (!deck->obs_count)
								return NTRLST_DECK_NOOP;
						--deck->obs_count;
						return NTRLST_DECK_BUFFERED;
				}

				if (is_key(key, "Right") || is_key(key, "Down") ||
					is_key(key, "PageDown")) {
						deck->cheat_page = (deck->cheat_page + 1) %
								NTRLST_CHEAT_PAGES;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Left") || is_key(key, "Up") ||
					is_key(key, "PageUp")) {
						deck->cheat_page =
								(deck->cheat_page + NTRLST_CHEAT_PAGES - 1) %
								NTRLST_CHEAT_PAGES;
						return NTRLST_DECK_REDRAW;
				}
				const struct ntrlst_shortcut *binding =
						ntrlst_find_shortcut(deck->json_hndlr, key);

				return binding ? buffer_trait(deck, binding) : NTRLST_DECK_NOOP;
		}

		if (deck->screen == NTRLST_SCREEN_CONTEXT) {
				if (is_key(key, "Escape") || is_key(key, "Backspace") ||
					is_key(key, "Tab") || is_key(key, "F1") ||
					is_key(key, "F2")) {
						deck->screen = deck->help_return;

						return NTRLST_DECK_REDRAW;
				}

				return NTRLST_DECK_NOOP;
		}

		if (is_key(key, "F1") || is_key(key, "F2")) {
				deck->help_return = deck->screen;
				deck->screen = is_key(key, "F1") ? NTRLST_SCREEN_CONTEXT :
						NTRLST_SCREEN_CHEAT;
				deck->cheat_page = 0;

				return NTRLST_DECK_REDRAW;
		}

		switch (deck->screen) {
		case NTRLST_SCREEN_WALK:
				if (is_key(key, "Escape") || is_key(key, "Backspace")) {
						deck->screen = deck->walk_return;
						return NTRLST_DECK_REDRAW;
				}
				if (!deck->journal)
						return NTRLST_DECK_NOOP;

				if (!deck->journal->active && is_key(key, "N")) {
						if (ntrlst_journal_start(deck->journal, err,
												 err_size))
								return NTRLST_DECK_ERROR;

						for (size_t i = 0; i < deck->basket_count; ++i)
								if (ntrlst_journal_add(deck->journal,
													   deck->basket[i]->id, err, err_size))
										return NTRLST_DECK_ERROR;

						deck->basket_page = 0;
						deck->walk_just_finished = false;

						return NTRLST_DECK_REDRAW;
				}
				if (!deck->journal->active)
						return NTRLST_DECK_NOOP;

				if (is_key(key, "L")) {
						begin_edit(deck, NTRLST_EDIT_LOCATION,
								   NTRLST_SCREEN_WALK, NULL, deck->journal->location);
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "N")) {
						begin_edit(deck, NTRLST_EDIT_WALK_NOTE,
								   NTRLST_SCREEN_WALK, NULL, deck->journal->note);
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "E")) {
						deck->screen = NTRLST_SCREEN_END_WALK;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Tab")) {
						deck->basket_return = NTRLST_SCREEN_WALK;
						deck->basket_page = 0;
						deck->screen = NTRLST_SCREEN_BASKET;
						return NTRLST_DECK_REDRAW;
				}

				return NTRLST_DECK_NOOP;

		case NTRLST_SCREEN_SEARCH:
				if (is_key(key, "Enter"))
						return run_search(deck, err, err_size);
				if (is_key(key, "Tab")) {
						deck->basket_return = NTRLST_SCREEN_SEARCH;
						deck->saved_detail_active = false;
						deck->basket_page = 0;
						deck->screen = NTRLST_SCREEN_BASKET;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Escape")) {
						if (!deck->obs_count)
								return NTRLST_DECK_NOOP;
						deck->obs_count = 0;
						return NTRLST_DECK_BUFFERED;
				}

				if (is_key(key, "Backspace")) {
						if (!deck->obs_count)
								return NTRLST_DECK_NOOP;
						--deck->obs_count;
						return NTRLST_DECK_BUFFERED;
				}
				{
						const struct ntrlst_shortcut *binding =
								ntrlst_find_shortcut(deck->json_hndlr, key);

						return binding ? buffer_trait(deck, binding) :
								NTRLST_DECK_NOOP;
				}

		case NTRLST_SCREEN_RESULTS:
				if (is_key(key, "Escape")) {
						deck->obs_count = 0;
						deck->match_count = 0;
						deck->screen = NTRLST_SCREEN_SEARCH;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Backspace") || is_key(key, "Enter")) {
						deck->screen = NTRLST_SCREEN_SEARCH;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Tab")) {
						deck->basket_return = NTRLST_SCREEN_RESULTS;
						deck->saved_detail_active = false;
						deck->basket_page = 0;
						deck->screen = NTRLST_SCREEN_BASKET;
						return NTRLST_DECK_REDRAW;
				}
				{
						int slot = selection_slot(key);
						if (slot >= 0 && (size_t)slot < deck->match_count) {
								deck->selected = deck->matches[slot].species;
								deck->detail_return = NTRLST_SCREEN_RESULTS;
								deck->screen = NTRLST_SCREEN_DETAIL;
								return NTRLST_DECK_REDRAW;
						}
				}

				return NTRLST_DECK_NOOP;

		case NTRLST_SCREEN_DETAIL:
				if (is_key(key, "Escape") || is_key(key, "Backspace")) {
						deck->screen = deck->detail_return;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "B")) {
						if (!deck->selected)
								return fail(err, err_size,
											"No selected species");

						if (deck->journal && deck->journal->active) {
								const struct ntrlst_finding *finding;

								finding = ntrlst_journal_find(deck->journal,
															  deck->selected->id);

								if (finding && finding->note[0]) {
										deck->screen = NTRLST_SCREEN_REMOVE_FINDING;
										return NTRLST_DECK_REDRAW;
								}
						}

						enum ntrlst_change outcome = toggle_basket(deck);
						if (outcome == NTRLST_DECK_ERROR)
								return fail(err, err_size,
											"Basket capacity exceeded");
						return outcome;
				}

				if (is_key(key, "N") && deck->selected && deck->journal &&
					deck->journal->active) {
						if (!ntrlst_deck_in_basket(deck, deck->selected)) {
								if (toggle_basket(deck) == NTRLST_DECK_ERROR)
										return fail(err, err_size,
													"Cannot save finding");
						}

						const struct ntrlst_finding *finding =
								ntrlst_journal_find(deck->journal,
													deck->selected->id);
						begin_edit(deck, NTRLST_EDIT_FINDING_NOTE,
								   NTRLST_SCREEN_DETAIL, deck->selected,
								   finding ? finding->note : "");
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Tab")) {
						if (deck->detail_return == NTRLST_SCREEN_BASKET) {
								deck->screen = NTRLST_SCREEN_BASKET;
								return NTRLST_DECK_REDRAW;
						}

						deck->saved_detail_return = deck->detail_return;
						deck->saved_detail_selected = deck->selected;
						deck->saved_detail_active = true;
						deck->basket_return = NTRLST_SCREEN_DETAIL;
						deck->basket_page = 0;
						deck->screen = NTRLST_SCREEN_BASKET;
						return NTRLST_DECK_REDRAW;
				}

				return NTRLST_DECK_NOOP;

		case NTRLST_SCREEN_BASKET:
				if (is_key(key, "Tab") || is_key(key, "Escape") ||
					is_key(key, "Backspace")) {
						deck->screen = deck->basket_return;
						if (deck->screen == NTRLST_SCREEN_DETAIL &&
							deck->saved_detail_active) {
								deck->selected = deck->saved_detail_selected;
								deck->detail_return = deck->saved_detail_return;
								deck->saved_detail_active = false;
						}
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Enter") || is_key(key, "Right") ||
					is_key(key, "PageDown") ||
					is_key(key, "Down")) {
						size_t pages = basket_pages(deck);
						if (pages == 1)
								return NTRLST_DECK_NOOP;
						deck->basket_page = (deck->basket_page + 1) % pages;
						return NTRLST_DECK_REDRAW;
				}

				if (is_key(key, "Left") || is_key(key, "PageUp") ||
					is_key(key, "Up")) {
						size_t pages = basket_pages(deck);
						if (pages == 1)
								return NTRLST_DECK_NOOP;
						deck->basket_page =
								(deck->basket_page + pages - 1) % pages;
						return NTRLST_DECK_REDRAW;
				}
				{
						int slot = selection_slot(key);
						size_t idx =
								deck->basket_page * NTRLST_BASKET_PAGE_SIZE +
								(slot < 0 ? 0U : (size_t)slot);
						if (slot >= 0 && idx < deck->basket_count) {
								deck->selected = deck->basket[idx];
								deck->detail_return = NTRLST_SCREEN_BASKET;
								deck->screen = NTRLST_SCREEN_DETAIL;
								return NTRLST_DECK_REDRAW;
						}
				}

				return NTRLST_DECK_NOOP;

		case NTRLST_SCREEN_CHEAT:
		case NTRLST_SCREEN_CONTEXT:
		case NTRLST_SCREEN_POWEROFF:
		case NTRLST_SCREEN_EDITOR:
		case NTRLST_SCREEN_END_WALK:
		case NTRLST_SCREEN_REMOVE_FINDING:
				break;
		}

		return fail(err, err_size, "Invalid screen");
}
