#include "renderer.h"

#include <cairo.h>
#include <pango/pangocairo.h>
#include <glib-object.h>

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const char *common_name(const struct ntrlst_species *sp)
{
#ifdef NTRLST_LANG_DE
		return sp->common_name_de;
#else
		return sp->common_name_en;
#endif
}

static void line(cairo_t *cr, double x1, double y1, double x2,
				 double y2, double width)
{
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_set_line_width(cr, width);

		double dx = width == 1 && x1 == x2 ? 0.5 : 0;
		double dy = width == 1 && y1 == y2 ? 0.5 : 0;

		cairo_move_to(cr, x1 + dx, y1 + dy);
		cairo_line_to(cr, x2 + dx, y2 + dy);
		cairo_stroke(cr);
}

static void text(cairo_t *cr, int x, int y, int width, int px,
				 bool bold, bool italic, int max_lines, bool center,
				 const char *value)
{
		if (!value || !*value || width <= 0 || x >= NTRLST_SCREEN_WIDTH || y >= NTRLST_SCREEN_HEIGHT)
				return;

		if (x + width > NTRLST_SCREEN_WIDTH)
				width = NTRLST_SCREEN_WIDTH - x;

		if (width <= 0)
				return;

		cairo_surface_t *scratch = cairo_image_surface_create(CAIRO_FORMAT_A8,
															  1, 1);
		cairo_t *scratch_cr = cairo_create(scratch);

		cairo_font_options_t *opts = cairo_font_options_create();
		cairo_font_options_set_antialias(opts, CAIRO_ANTIALIAS_GRAY);
		cairo_font_options_set_hint_style(opts, CAIRO_HINT_STYLE_FULL);
		cairo_font_options_set_hint_metrics(opts, CAIRO_HINT_METRICS_ON);
		cairo_set_font_options(scratch_cr, opts);

		PangoLayout *layout = pango_cairo_create_layout(scratch_cr);
		PangoFontDescription *fd = pango_font_description_new();
		bool heading = bold && px >= 23;

		pango_font_description_set_family(fd,
										  heading ? "IBM Plex Serif" : "Atkinson Hyperlegible Next");
		pango_font_description_set_absolute_size(fd, px * PANGO_SCALE);

		pango_font_description_set_weight(fd,
										  heading ? PANGO_WEIGHT_SEMIBOLD : PANGO_WEIGHT_MEDIUM);

		if (italic)
				pango_font_description_set_style(fd, PANGO_STYLE_ITALIC);

		pango_layout_set_font_description(layout, fd);
		pango_layout_set_width(layout, width * PANGO_SCALE);
		pango_layout_set_wrap(layout, PANGO_WRAP_WORD_CHAR);
		pango_layout_set_ellipsize(layout, PANGO_ELLIPSIZE_END);

		if (max_lines > 0)
				pango_layout_set_height(layout, -max_lines);

		pango_layout_set_alignment(layout,
								   center ? PANGO_ALIGN_CENTER : PANGO_ALIGN_LEFT);
		pango_layout_set_text(layout, value, -1);

		int layout_width = 0, layout_height = 0;
		pango_layout_get_pixel_size(layout, &layout_width, &layout_height);

		int height = layout_height + 2;

		if (height > NTRLST_SCREEN_HEIGHT - y)
				height = NTRLST_SCREEN_HEIGHT - y;

		if (height > 0) {
				cairo_surface_t *mask = cairo_image_surface_create(CAIRO_FORMAT_A8,
																   width, height);

				if (cairo_surface_status(mask) == CAIRO_STATUS_SUCCESS) {
						cairo_t *mask_cr = cairo_create(mask);
						cairo_set_font_options(mask_cr, opts);

						pango_cairo_update_layout(mask_cr, layout);
						cairo_set_source_rgb(mask_cr, 1, 1, 1);
						cairo_move_to(mask_cr, 0, 0);
						pango_cairo_show_layout(mask_cr, layout);
						cairo_destroy(mask_cr);

						cairo_surface_flush(mask);
						unsigned char *pixels = cairo_image_surface_get_data(mask);
						int stride = cairo_image_surface_get_stride(mask);

						unsigned char minimum_coverage = px <= 16 ? 76 : 99;

						for (int row = 0; row < height; ++row)
								for (int col = 0; col < width; ++col) {
										unsigned char *a = &pixels[row * stride + col];
										*a = *a >= minimum_coverage ? 255 : 0;
								}

						cairo_surface_mark_dirty(mask);
						cairo_set_source_rgb(cr, 1, 1, 1);
						cairo_mask_surface(cr, mask, x, y);
				}

				cairo_surface_destroy(mask);
		}

		pango_font_description_free(fd);
		g_object_unref(layout);
		cairo_font_options_destroy(opts);
		cairo_destroy(scratch_cr);
		cairo_surface_destroy(scratch);
}

static void header(cairo_t *cr, const char *title, const char *subtitle)
{
		text(cr, 18, 8, 564, 25, true, false, 1, false, title);
		line(cr, 18, 47, 582, 47, 2);

		if (subtitle)
				text(cr, 18, 54, 564, 17, false, false, 1, false,
					 subtitle);
}

static void footer(cairo_t *cr, const char *value)
{
		line(cr, 18, 366, 582, 366, 1);

		text(cr, 18, 374, 564, 15, false, false, 1, false, value);
}

static void missing_illustration(cairo_t *cr, int x, int y, int size)
{
		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_set_line_width(cr, 2);
		cairo_rectangle(cr, x + 2, y + 2, size - 4, size - 4);
		cairo_stroke(cr);

		text(cr, x + 5, y + size / 2 - 16, size - 10, 15,
			 true, false, 1, true, "PNG");
#ifdef NTRLST_LANG_DE
		text(cr, x + 5, y + size / 2 + 4, size - 10, 12,
			 true, false, 1, true, "FEHLT");
#endif
#ifdef NTRLST_LANG_EN
		text(cr, x + 5, y + size / 2 + 4, size - 10, 12,
			 true, false, 1, true, "MISSING");
#endif
}

static void illustration(cairo_t *cr, const char *root,
						 const struct ntrlst_species *sp,
						 int x, int y, int size)
{
		if (!sp || !sp->image || strstr(sp->image, "..")) {
				missing_illustration(cr, x, y, size);
				return;
		}

		char path[1024];
		int n = snprintf(path, sizeof(path), "%s/%s", root, sp->image);

		if (n <= 0 || (size_t)n >= sizeof(path)) {
				missing_illustration(cr, x, y, size);
				return;
		}

		cairo_surface_t *png = cairo_image_surface_create_from_png(path);
		if (cairo_surface_status(png) != CAIRO_STATUS_SUCCESS) {
				missing_illustration(cr, x, y, size);
				cairo_surface_destroy(png);
				return;
		}

		double sw = cairo_image_surface_get_width(png);
		double sh = cairo_image_surface_get_height(png);

		if (sw <= 0 || sh <= 0) {
				missing_illustration(cr, x, y, size);
				cairo_surface_destroy(png);
				return;
		}

		double scale = (double)size / sw;
		if (sh * scale > size)
				scale = (double)size / sh;

		cairo_save(cr);
		cairo_translate(cr, x + (size - sw * scale) / 2.,
						y + (size - sh * scale) / 2.);
		cairo_scale(cr, scale, scale);
		cairo_set_source_surface(cr, png, 0, 0);
		cairo_pattern_set_filter(cairo_get_source(cr), CAIRO_FILTER_BILINEAR);
		cairo_paint(cr);

		cairo_restore(cr);
		cairo_surface_destroy(png);
}

static void draw_search(cairo_t *cr)
{
		header(cr, "NATURALIST", NULL);
#ifdef NTRLST_LANG_DE
		text(cr, 26, 77, 548, 27, true, false, 1, true,
			 "3–5 Merkmale eingeben");
		text(cr, 26, 119, 548, 19, false, false, 1, true,
			 "Mit ENTER bestätigen");
		line(cr, 28, 163, 572, 163, 1);

		text(cr, 34, 186, 240, 21, true, false, 1, false,
			 "L  Lamellen");
		text(cr, 315, 186, 250, 21, true, false, 1, false,
			 "P  Poren");
		text(cr, 34, 232, 240, 21, true, false, 1, false,
			 "F  Leisten");
		text(cr, 315, 232, 250, 21, true, false, 1, false,
			 "S  Stacheln");
		line(cr, 28, 277, 572, 277, 1);

		text(cr, 26, 294, 265, 17, false, false, 1, false,
			 "Fn + ?  Cheatsheet");
		text(cr, 306, 294, 275, 17, false, false, 1, false,
			 "Ctrl + Alt + P  Ausschalten");

		text(cr, 24, 334, 552, 14, false, false, 1, true,
			 "Das Display braucht bis zu 30s für einen Refresh!");

		footer(cr, "BACK: Merkmal | TAB: Korb | Strg+Alt+W: Spaziergang");
#endif
#ifdef NTRLST_LANG_EN
		text(cr, 26, 77, 548, 27, true, false, 1, true,
			 "Enter 3 - 5 traits");
		text(cr, 26, 119, 548, 19, false, false, 1, true,
			 "ENTER to confirm");
		line(cr, 28, 163, 572, 163, 1);

		text(cr, 34, 186, 240, 21, true, false, 1, false,
			 "L  Gills");
		text(cr, 315, 186, 250, 21, true, false, 1, false,
			 "P  Pores");
		text(cr, 34, 232, 240, 21, true, false, 1, false,
			 "F  Ridges");
		text(cr, 315, 232, 250, 21, true, false, 1, false,
			 "S  Teeth");
		line(cr, 28, 277, 572, 277, 1);

		text(cr, 26, 294, 265, 17, false, false, 1, false,
			 "Fn + ?  Cheatsheet");
		text(cr, 306, 294, 275, 17, false, false, 1, false,
			 "Ctrl + Alt + P  Power off");

		text(cr, 24, 334, 552, 14, false, false, 1, true,
			 "A display refresh takes up to 30s!");

		footer(cr, "BACK: Trait | TAB: Basket | Strg+Alt+W: Walk");
#endif
}

static void draw_summary(cairo_t *cr, const struct ntrlst_deck *deck)
{
		char summary[2048] = {0};
		size_t used = 0;

		for (size_t i = 0; i < deck->obs_count; ++i) {
				const struct ntrlst_shortcut *binding =
						ntrlst_find_shortcut(deck->json_hndlr, deck->obs_keys[i]);
				if (!binding)
						continue;

				char key[32];

				if (!strncmp(binding->key, "Shift+", 6))
						snprintf(key, sizeof(key), "^%s", binding->key + 6);
				else
						snprintf(key, sizeof(key), "%s", binding->key);

#ifdef NTRLST_LANG_DE
				int n = snprintf(summary + used, sizeof(summary) - used,
								 "%s%s=%s", i ? "  |  " : "", key, binding->label_de);
#endif
#ifdef NTRLST_LANG_EN
				int n = snprintf(summary + used, sizeof(summary) - used,
								 "%s%s=%s", i ? "  |  " : "", key, binding->label_en);
#endif

				if (n < 0 || (size_t)n >= sizeof(summary) - used)
						break;

				used += (size_t)n;
		}
#ifdef NTRLST_LANG_DE
		text(cr, 18, 78, 564, 14, true, false, 2, false,
			 *summary ? summary : "(keine)");
#endif
#ifdef NTRLST_LANG_EN
		text(cr, 18, 78, 564, 14, true, false, 2, false,
			 *summary ? summary : "(none)");
#endif
}

static void draw_results(cairo_t *cr, const struct ntrlst_deck *deck,
						 const char *root)
{
#ifdef NTRLST_LANG_DE
		header(cr, "NATURALIST  /  TREFFER", NULL);
#endif
#ifdef NTRLST_LANG_EN
		header(cr, "NATURALIST  /  MATCHES", NULL);
#endif
		draw_summary(cr, deck);

		if (deck->match_count == 0) {
#ifdef NTRLST_LANG_DE
				text(cr, 28, 160, 544, 26, true, false, 2, true,
					 "Keine passenden Treffer.");
				text(cr, 28, 218, 544, 18, false, false, 1, true,
					 "ESC: neue Suche   |   Fn+?: Cheatsheet");
#endif
#ifdef NTRLST_LANG_EN
				text(cr, 28, 160, 544, 26, true, false, 2, true,
					 "No matches.");
				text(cr, 28, 218, 544, 18, false, false, 1, true,
					 "ESC: new search   |   Fn+?: Cheatsheet");
#endif

		} else {
				for (size_t i = 0; i < deck->match_count && i < 5; ++i) {
						int x = 10 + (int)i * 116;
						const struct ntrlst_species *sp =
								deck->matches[i].species;
						char letter[2] = {(char)('A' + i), 0};

						text(cr, x, 122, 116, 24, true, false, 1, true, letter);
						illustration(cr, root, sp, x + 13, 158, 90);
						text(cr, x + 2, 258, 112, 15, true, false, 3, true,
							 common_name(sp));
				}
#ifdef NTRLST_LANG_DE
				text(cr, 18, 330, 565, 16, true, false, 1, false,
					 "A–E / Fn+1–5: Auswahl  |  TAB: Korb");
#endif
#ifdef NTRLST_LANG_EN
				text(cr, 18, 330, 565, 16, true, false, 1, false,
					 "A–E / Fn+1–5: Selection  |  TAB: Basket");
#endif
		}
#ifdef NTRLST_LANG_DE
		footer(cr, "ESC: neu | ENTER/BACK: ändern | Fn+?: Cheatsheet");
#endif
#ifdef NTRLST_LANG_EN
		footer(cr, "ESC: new | ENTER/BACK: change | Fn+?: Cheatsheet");
#endif
}

static void draw_detail(cairo_t *cr, const struct ntrlst_deck *deck,
						const char *root)
{
#ifdef NTRLST_LANG_DE
		header(cr, "NATURALIST  /  ANSICHT", NULL);
#endif
#ifdef NTRLST_LANG_EN
		header(cr, "NATURALIST  /  PREVIEW", NULL);
#endif

		if (deck->selected) {
				illustration(cr, root, deck->selected, 14, 73, 282);

				text(cr, 306, 92, 270, 23, true, false, 2, false,
					 common_name(deck->selected));
				text(cr, 306, 169, 270, 18, false, true, 3, false,
					 deck->selected->scientific_name);
#ifdef NTRLST_LANG_DE
				text(cr, 306, 262, 270, 16, true, false, 2, false,
					 ntrlst_deck_in_basket(deck, deck->selected)
					 ? "[B] Aus Korb entfernen" : "[B] In Korb legen");
#endif
#ifdef NTRLST_LANG_EN
				text(cr, 306, 262, 270, 16, true, false, 2, false,
					 ntrlst_deck_in_basket(deck, deck->selected)
					 ? "[B] Remove from basket" : "[B] Place in basket");
#endif

				const struct ntrlst_finding *finding =
						deck->journal && deck->journal->active ?
						ntrlst_journal_find(deck->journal, deck->selected->id) : NULL;

				if (finding && finding->note[0])
						text(cr, 306, 298, 270, 15, false, false, 2,
							 false, finding->note);
		}

#ifdef NTRLST_LANG_DE
		footer(cr, "B: Korb | N: Notiz (bei Spaziergang) | BACK: zurück");
#endif
#ifdef NTRLST_LANG_EN
		footer(cr, "B: Basket | N: Note (for walks) | BACK: back");
#endif
}

static void draw_basket(cairo_t *cr, const struct ntrlst_deck *deck,
						const char *root)
{
		size_t pages = (deck->basket_count + NTRLST_BASKET_PAGE_SIZE - 1)
				/ NTRLST_BASKET_PAGE_SIZE;

		if (!pages)
				pages = 1;
		char subtitle[160];

		if (deck->journal && deck->journal->active)
#ifdef NTRLST_LANG_DE
				snprintf(subtitle, sizeof(subtitle),
						 "%zu gespeicherte Funde | Seite %zu/%zu",
						 deck->basket_count, deck->basket_page + 1, pages);
		else
				snprintf(subtitle, sizeof(subtitle), "Seite %zu/%zu",
						 deck->basket_page + 1, pages);
		header(cr, "NATURALIST  /  KORB", subtitle);
#endif
#ifdef NTRLST_LANG_EN
		snprintf(subtitle, sizeof(subtitle),
				 "%zu saved finds | page %zu/%zu",
				 deck->basket_count, deck->basket_page + 1, pages);
		else
				snprintf(subtitle, sizeof(subtitle), "page %zu/%zu",
						 deck->basket_page + 1, pages);
		header(cr, "NATURALIST  /  BASKET", subtitle);
#endif

		if (!deck->basket_count) {
#ifdef NTRLST_LANG_DE
				text(cr, 26, 172, 548, 25, true, false, 1, true,
					 "Noch keine Pilze im Korb.");
#endif
#ifdef NTRLST_LANG_EN
				text(cr, 26, 172, 548, 25, true, false, 1, true,
					 "Nothing in your basket yet.");
#endif

		} else {
				size_t start = deck->basket_page * NTRLST_BASKET_PAGE_SIZE;

				for (size_t i = start; i < deck->basket_count && i < start + 5; ++i) {
						int slot = (int)(i - start);
						int y = 80 + slot * 56;
						char letter[2] = {(char)('A' + slot), 0};

						text(cr, 20, y + 12, 24, 17, true, false, 1, false, letter);
						illustration(cr, root, deck->basket[i], 46, y, 56);
						text(cr, 115, y + 6, 465, 18, true, false, 1, false,
							 common_name(deck->basket[i]));

						const struct ntrlst_finding *f =
								deck->journal && deck->journal->active ?
								ntrlst_journal_find(deck->journal,
													deck->basket[i]->id) : NULL;

						if (f && f->note[0])
								text(cr, 115, y + 27, 465, 14, false, false, 1,
									 false, f->note);
				}
		}

#ifdef NTRLST_LANG_DE
		footer(cr, "A–E: ansehen | ENTER: nächste Seite | TAB: zurück");
#endif
#ifdef NTRLST_LANG_EN
		footer(cr, "A–E: view | ENTER: next page | TAB: back");
#endif
}

static void draw_cheat(cairo_t *cr, const struct ntrlst_deck *deck)
{
		bool shifted = deck->cheat_page == 1;
		char title[70];

#ifdef NTRLST_LANG_DE
		snprintf(title, sizeof(title), "CHEATSHEET  %u/2", deck->cheat_page + 1);
		header(cr, title, shifted ? "SHIFT + Buchstabe" : NULL);
#endif
#ifdef NTRLST_LANG_EN
		snprintf(title, sizeof(title), "CHEAT SHEET  %u/2", deck->cheat_page + 1);
		header(cr, title, shifted ? "SHIFT + Letter" : NULL);
#endif
		size_t count = 0;

		for (size_t i = 0; i < deck->json_hndlr->shortcut_count; ++i)
				if ((!strncmp(deck->json_hndlr->shortcuts[i].key,
							  "Shift+", 6)) == shifted)
						++count;

		size_t per_col = (count + 1) / 2;
		if (!per_col)
				per_col = 1;

		size_t j = 0;

		for (size_t i = 0; i < deck->json_hndlr->shortcut_count; ++i) {
				const struct ntrlst_shortcut *s = &deck->json_hndlr->shortcuts[i];

				if ((!strncmp(s->key, "Shift+", 6)) != shifted)
						continue;

				size_t col = j / per_col, row = j % per_col;

				if (col > 1 || row > 11)
						break;
				int x = 18 + (int)col * 291;
				int y = 81 + (int)row * 23;
				const char *label = shifted ? s->key + 6 : s->key;

				text(cr, x, y, 27, 17, true, false, 1, false, label);
#ifdef NTRLST_LANG_DE
				text(cr, x + 31, y, 248, 16, false, false, 1,
					 false, s->label_de);
#endif
#ifdef NTRLST_LANG_EN
				text(cr, x + 31, y, 248, 16, false, false, 1,
					 false, s->label_en);
#endif
				++j;
		}
#ifdef NTRLST_LANG_DE
		footer(cr, "ENTER: Treffer | ←/→: Seite | BACK: Merkmal | ESC: zurück");
#endif
#ifdef NTRLST_LANG_EN
		footer(cr, "ENTER: Matches | ←/→: Page | BACK: Trait | ESC: back");
#endif
}

static void draw_poweroff(cairo_t *cr)
{
#ifdef NTRLST_LANG_DE
		header(cr, "NATURALIST  /  AUSSCHALTEN", NULL);
		text(cr, 24, 88, 552, 25, true, false, 2, true,
			 "Naturalist wirklich ausschalten?");
		text(cr, 32, 159, 536, 18, false, false, 1, true,
			 "Funde werden automatisch gespeichert");
		line(cr, 80, 216, 520, 216, 1);

		text(cr, 32, 236, 536, 21, true, false, 1, true,
			 "SHIFT + P  =  ausschalten");
		text(cr, 32, 287, 536, 18, false, false, 1, true,
			 "ESC / BACK  =  abbrechen");

		footer(cr, "Denk an den Schalter!");
#endif
#ifdef NTRLST_LANG_EN
		header(cr, "NATURALIST  /  POWER OFF", NULL);
		text(cr, 24, 88, 552, 25, true, false, 2, true,
			 "Really power off?");
		text(cr, 32, 159, 536, 18, false, false, 1, true,
			 "Finds are saved automatically");
		line(cr, 80, 216, 520, 216, 1);

		text(cr, 32, 236, 536, 21, true, false, 1, true,
			 "SHIFT + P  =  power off");
		text(cr, 32, 287, 536, 18, false, false, 1, true,
			 "ESC / BACK  =  cancel");

		footer(cr, "Remember the switch!");
#endif
}

static void draw_walk(cairo_t *cr, const struct ntrlst_deck *deck)
{
		const struct ntrlst_journal *j = deck->journal;

#ifdef NTRLST_LANG_DE
		header(cr, "NATURALIST  /  SPAZIERGANG", NULL);
#endif
#ifdef NTRLST_LANG_EN
		header(cr, "NATURALIST  /  WALK", NULL);
#endif

		if (!j) {
#ifdef NTRLST_LANG_DE
				text(cr, 28, 169, 544, 20, true, false, 2, true,
					 "Journal im Vorschaumodus nicht verfügbar.");
#endif
#ifdef NTRLST_LANG_EN
				text(cr, 28, 169, 544, 20, true, false, 2, true,
					 "Journal not available in preview mode.");
#endif
		} else if (!j->active) {
#ifdef NTRLST_LANG_DE
				text(cr, 26, 100, 548, 23, true, false, 2, true,
					 deck->walk_just_finished ? "Spaziergang gespeichert!" :
					 "Noch kein Spaziergang aktiv.");
				line(cr, 84, 173, 516, 173, 1);

				text(cr, 26, 204, 548, 22, true, false, 2, true,
					 "N Neuen Spaziergang starten");
				text(cr, 30, 288, 540, 16, false, false, 2, true,
					 "Frühere Spaziergänge liegen als JSON-Dateien im walks/-Verzeichnis.");
#endif
#ifdef NTRLST_LANG_EN
				text(cr, 26, 100, 548, 23, true, false, 2, true,
					 deck->walk_just_finished ? "Walk saved!" :
					 "No active walk yet.");
				line(cr, 84, 173, 516, 173, 1);

				text(cr, 26, 204, 548, 22, true, false, 2, true,
					 "N start a new walk");
				text(cr, 30, 288, 540, 16, false, false, 2, true,
					 "Previous walks are saved as JSON in walks/-directory.");
#endif
		} else {
				char label[256];
				char started[sizeof(j->started_at)];

				snprintf(started, sizeof(started), "%s", j->started_at);

				if (strlen(started) > 10 && started[10] == 'T')
						started[10] = ' ';

				snprintf(label, sizeof(label), "Start: %s", started);
				text(cr, 28, 74, 544, 17, true, false, 1, false, label);
#ifdef NTRLST_LANG_DE
				snprintf(label, sizeof(label), "Ort: %s",
						 *j->location ? j->location : "keiner");
				text(cr, 28, 122, 544, 20, true, false, 2, false, label);

				snprintf(label, sizeof(label), "Funde: %zu", j->finding_count);
				text(cr, 28, 183, 544, 20, true, false, 1, false, label);
				text(cr, 28, 231, 544, 16, false, false, 2, false,
					 *j->note ? j->note : "keine Notizen");
				line(cr, 28, 296, 572, 296, 1);

				text(cr, 28, 314, 544, 17, false, false, 1, false,
					 "L: Ort | N: Notiz | E: beenden | TAB: Korb");
#endif
#ifdef NTRLST_LANG_EN
				snprintf(label, sizeof(label), "Location: %s",
						 *j->location ? j->location : "none");
				text(cr, 28, 122, 544, 20, true, false, 2, false, label);

				snprintf(label, sizeof(label), "Finds: %zu", j->finding_count);
				text(cr, 28, 183, 544, 20, true, false, 1, false, label);
				text(cr, 28, 231, 544, 16, false, false, 2, false,
					 *j->note ? j->note : "no notes");
				line(cr, 28, 296, 572, 296, 1);

				text(cr, 28, 314, 544, 17, false, false, 1, false,
					 "L: Location | N: Note | E: cancel | TAB: Basket");
#endif

		}
#ifdef NTRLST_LANG_DE
		footer(cr, "Strg+Alt+W / ESC: zurück");
#endif
#ifdef NTRLST_LANG_EN
		footer(cr, "Strg+Alt+W / ESC: back");
#endif
}

static void draw_editor(cairo_t *cr, const struct ntrlst_deck *deck)
{
#ifdef NTRLST_LANG_DE
		header(cr, "NATURALIST  /  EINGABE", NULL);
		text(cr, 24, 79, 552, 20, true, false, 2, false,
			 deck->edit_target == NTRLST_EDIT_LOCATION
			 ? "Ort eingeben (kein Refresh beim Tippen)"
			 : "Notiz eingeben (kein Refresh beim Tippen)");
#endif
#ifdef NTRLST_LANG_EN
		header(cr, "NATURALIST  /  INPUT", NULL);
		text(cr, 24, 79, 552, 20, true, false, 2, false,
			 deck->edit_target == NTRLST_EDIT_LOCATION
			 ? "Enter location (no refresh while typing)"
			 : "Enter note (no refresh while typing)");
#endif

		cairo_set_source_rgb(cr, 1, 1, 1);
		cairo_set_line_width(cr, 2);
		cairo_rectangle(cr, 24, 128, 552, 208);
		cairo_stroke(cr);
#ifdef NTRLST_LANG_DE
		text(cr, 40, 145, 520, 19, true, false, 7, false,
			 *deck->edit_text ? deck->edit_text : "(leer)");

		footer(cr, "ENTER: speichern | TAB: Vorschau | ESC: abbrechen");
#endif
#ifdef NTRLST_LANG_EN
		text(cr, 40, 145, 520, 19, true, false, 7, false,
			 *deck->edit_text ? deck->edit_text : "(empty)");
		footer(cr, "ENTER: save | TAB: Preview | ESC: cancel");
#endif
}

static void draw_end_walk(cairo_t *cr, const struct ntrlst_deck *deck)
{
#ifdef NTRLST_LANG_DE
		header(cr, "NATURALIST  /  BEENDEN", NULL);
		text(cr, 27, 90, 546, 21, true, false, 1, true,
			 "Spaziergang beenden?");
#endif
#ifdef NTRLST_LANG_EN
		header(cr, "NATURALIST  /  FINISH", NULL);
		text(cr, 27, 90, 546, 21, true, false, 1, true,
			 "Finish walk?");
#endif
		char summary[128];

#ifdef NTRLST_LANG_DE
		snprintf(summary, sizeof(summary), "%zu Funde im Körbchen.",
				 deck->journal ? deck->journal->finding_count : 0);
		text(cr, 27, 149, 546, 25, true, false, 2, true, summary);
		line(cr, 82, 221, 518, 221, 1);

		text(cr, 27, 241, 546, 21, true, false, 1, true,
			 "SHIFT+E: speichern und beenden");
		text(cr, 27, 297, 546, 18, false, false, 1, true,
			 "ESC: weiter sammeln");

		footer(cr, "Spaziergänge werden als JSON gespeichert");
#endif
#ifdef NTRLST_LANG_EN
		snprintf(summary, sizeof(summary), "%zu Finds in basket.",
				 deck->journal ? deck->journal->finding_count : 0);
		text(cr, 27, 149, 546, 25, true, false, 2, true, summary);
		line(cr, 82, 221, 518, 221, 1);

		text(cr, 27, 241, 546, 21, true, false, 1, true,
			 "SHIFT+E: save and exit");
		text(cr, 27, 297, 546, 18, false, false, 1, true,
			 "ESC: continue collecting");

		footer(cr, "Walks are saved as JSON.");
#endif

}

static void draw_remove_finding(cairo_t *cr)
{
#ifdef NTRLST_LANG_DE
		header(cr, "NATURALIST  /  ENTFERNEN", NULL);
		text(cr, 28, 122, 544, 24, true, false, 2, true,
			 "Fund und gespeicherte Notiz löschen?");
		line(cr, 82, 217, 518, 217, 1);

		text(cr, 28, 239, 544, 21, true, false, 1, true,
			 "SHIFT+B: entfernen");
		text(cr, 28, 297, 544, 18, false, false, 1, true,
			 "ESC: abbrechen");

		footer(cr, "Löschen ist endgültig.");
#endif
#ifdef NTRLST_LANG_EN
		header(cr, "NATURALIST  /  REMOVE", NULL);
		text(cr, 28, 122, 544, 24, true, false, 2, true,
			 "Delete find and saved note?");
		line(cr, 82, 217, 518, 217, 1);

		text(cr, 28, 239, 544, 21, true, false, 1, true,
			 "SHIFT+B: remove");
		text(cr, 28, 297, 544, 18, false, false, 1, true,
			 "ESC: cancel");

		footer(cr, "Removing is final.");
#endif

}

static void draw_context(cairo_t *cr)
{
#ifdef NTRLST_LANG_DE
		static const char *const lines[] = {
				"SUCHE: 3–5 Merkmale, dann ENTER.",
				"BACK: letztes Merkmal entfernen. ESC: löschen.",
				"TREFFER: A–E oder Fn+1–5 wählen.",
				"ANSICHT: B legt Bild in den Korb.",
				"SPAZIERGANG: Strg+Alt+W öffnen.",
				"SPAZIERGANG: N starten / Notiz, L Ort, E Ende.",
				"NOTIZ: In Bildansicht N (bei aktivem Spaziergang).",
				"EINGABE: ENTER speichert, TAB zeigt Vorschau.",
				"CHEATSHEET: Merkmale eingeben, ENTER sucht.",
				"CHEATSHEET: ←/→ Seite; TAB/ESC zurück.",
				"Strg+Alt+P: Ausschalten (Bestätigung)."
		};

		header(cr, "NATURALIST  /  BEDIENUNG", NULL);
#endif
#ifdef NTRLST_LANG_EN
		static const char *const lines[] = {
				"SUCHE: 3–5 traits, then ENTER.",
				"BACK: remove last trait. ESC: delete.",
				"MATCH: A–E or select Fn+1–5.",
				"VIEW: B places pic in basket.",
				"WALK: Strg+Alt+W open.",
				"WALK: N start / Note, L location, E end.",
				"NOTE: In Preview N (for active walks).",
				"INPUT: ENTER saves, TAB previews.",
				"CHEAT SHEET: enter traits, search w/ ENTER.",
				"CHEAT SHEET: ←/→ page; TAB/ESC back.",
				"Strg+Alt+P: Power off (confirm)."
		};

		header(cr, "NATURALIST  /  MANUAL", NULL);
#endif

		for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++)
				text(cr, 24, 65 + (int)i * 26, 550, 15,
					 false, false, 1, false, lines[i]);
#ifdef NTRLST_LANG_DE
		footer(cr, "ESC / Fn+H: zurück");
#endif
#ifdef NTRLST_LANG_EN
		footer(cr, "ESC / Fn+H: back");
#endif
}

int ntrlst_render_png(const struct ntrlst_deck *deck,
					  const char *software_root, const char *output_path,
					  char *err, size_t err_size)
{
		if (!deck || !deck->json_hndlr || !software_root || !output_path) {
				if (err && err_size)
						snprintf(err, err_size, "Invalid renderer argument");
				return -1;
		}

		cairo_surface_t *surface = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
															  NTRLST_SCREEN_WIDTH, NTRLST_SCREEN_HEIGHT);
		if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
				if (err && err_size)
						snprintf(err, err_size, "Cannot allocate Cairo surface");
				cairo_surface_destroy(surface);
				return -1;
		}

		cairo_t *cr = cairo_create(surface);
		cairo_set_source_rgb(cr, 0, 0, 0);
		cairo_paint(cr);

		switch (deck->screen) {
		case NTRLST_SCREEN_SEARCH:
				draw_search(cr);
				break;
		case NTRLST_SCREEN_RESULTS:
				draw_results(cr, deck, software_root);
				break;
		case NTRLST_SCREEN_DETAIL:
				draw_detail(cr, deck, software_root);
				break;
		case NTRLST_SCREEN_BASKET:
				draw_basket(cr, deck, software_root);
				break;
		case NTRLST_SCREEN_CHEAT:
				draw_cheat(cr, deck);
				break;
		case NTRLST_SCREEN_CONTEXT:
				draw_context(cr);
				break;
		case NTRLST_SCREEN_POWEROFF:
				draw_poweroff(cr);
				break;
		case NTRLST_SCREEN_WALK:
				draw_walk(cr, deck);
				break;
		case NTRLST_SCREEN_EDITOR:
				draw_editor(cr, deck);
				break;
		case NTRLST_SCREEN_END_WALK:
				draw_end_walk(cr, deck);
				break;
		case NTRLST_SCREEN_REMOVE_FINDING:
				draw_remove_finding(cr);
				break;
		}

		cairo_status_t draw_status = cairo_status(cr);
		cairo_destroy(cr);

		cairo_status_t output_status = draw_status == CAIRO_STATUS_SUCCESS
				? cairo_surface_write_to_png(surface, output_path) : draw_status;
		cairo_surface_destroy(surface);

		if (output_status != CAIRO_STATUS_SUCCESS) {
				if (err && err_size)
						snprintf(err, err_size, "Cairo PNG render: %s",
								 cairo_status_to_string(output_status));
				return -1;
		}
		return 0;
}
