#define _POSIX_C_SOURCE 200809L
#include "naturalist.h"
#include "input.h"
#include "renderer.h"

#include <errno.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

struct settings {
		const char *data_dir;
		const char *assets_root;
		const char *bridge;
		const char *python;
		const char *device;
		const char *frame;
		const char *walk_dir;
};

static void usage(const char *argv0)
{
		fprintf(stderr,
				"Usage: %s [OPTIONS] --live\n"
				"Options (put BEFORE --live):\n"
				"  --data DIR         default: ./data\n"
				"  --assets-root DIR  default: .\n"
				"  --device PATH      evdev input device\n"
				"  --python PATH      default: python3\n"
				"  --bridge PATH      default: ./display/inky_bridge.py\n"
				"  --frame PATH       default: /tmp/naturalist-frame.png\n"
				"  --walk-dir DIR     default: ~/.local/share/naturalist/walks\n",
				argv0);
}

static int parse_args(int argc, char **argv, struct settings *cfg)
{
		*cfg = (struct settings){
				.data_dir = "data", .assets_root = ".", .python = "python3",
				.bridge = "display/inky_bridge.py", .frame = "/tmp/naturalist-frame.png"
		};
		int i = 1;

		while (i < argc) {
				if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
						usage(argv[0]); return 1;
				}
				const char **field = NULL;
				if (!strcmp(argv[i], "--data")) field = &cfg->data_dir;
				else if (!strcmp(argv[i], "--assets-root")) field = &cfg->assets_root;
				else if (!strcmp(argv[i], "--python")) field = &cfg->python;
				else if (!strcmp(argv[i], "--bridge")) field = &cfg->bridge;
				else if (!strcmp(argv[i], "--frame")) field = &cfg->frame;
				else if (!strcmp(argv[i], "--device")) field = &cfg->device;
				else if (!strcmp(argv[i], "--walk-dir")) field = &cfg->walk_dir;
				else if (!strcmp(argv[i], "--live")) {
						if (i + 1 != argc) return -1;
						return 0;
				} else return -1;

				if (i + 1 >= argc) return -1;
				*field = argv[i + 1];
				i += 2;
		}
		return -1;
}

static int render(const struct ntrlst_deck *deck, const char *root,
                  const char *path)
{
		char err[512] = {0};

		if (ntrlst_render_png(deck, root, path, err, sizeof(err))) {
				fprintf(stderr, "RENDER ERROR: %s\n", err);
				return -1;
		}
		return 0;
}

static int apply(struct ntrlst_deck *deck, const char *key,
                 enum ntrlst_change *change)
{
		char err[512] = {0};
		*change = ntrlst_deck_press(deck, key, err, sizeof(err));

		if (*change == NTRLST_DECK_ERROR) {
				fprintf(stderr, "DECK ERROR on '%s': %s\n", key, err);
				return -1;
		}
		return 0;
}

static int call_bridge(const struct settings *cfg)
{
		pid_t pid = fork();

		if (pid == -1) {
				perror("fork");
				return -1;
		}
		if (pid == 0) {
				execlp(cfg->python, cfg->python, cfg->bridge, cfg->frame, (char *)NULL);
				perror("exec Python display helper");
				_exit(127);
		}

		int status;
		while (waitpid(pid, &status, 0) < 0) {
				if (errno == EINTR) continue;
				perror("waitpid"); return -1;
		}
		if (!WIFEXITED(status) || WEXITSTATUS(status)) {
				fprintf(stderr, "Display helper exited unsuccessfully (status %#x)\n", status);
				return -1;
		}
		return 0;
}

static int present(const struct ntrlst_deck *deck, const struct settings *cfg)
{
		if (render(deck, cfg->assets_root, cfg->frame)) return -1;
		printf("DISPLAY: %s (refresh may take 40–45 s)\n",
			   ntrlst_screen_name(deck->screen));
		fflush(stdout);
		return call_bridge(cfg);
}

static int request_poweroff(void)
{
		pid_t pid = fork();
		if (pid == -1) {
				perror("fork poweroff");
				return -1;
		}
		if (pid == 0) {
				execl("/usr/bin/sudo", "sudo", "-n", "/usr/bin/systemctl",
					  "poweroff", (char *)NULL);
				perror("exec poweroff");
				_exit(127);
		}
		int status;
		while (waitpid(pid, &status, 0) < 0) {
				if (errno == EINTR) continue;
				perror("waitpid poweroff"); return -1;
		}
		if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
				fprintf(stderr, "Poweroff request failed (status %#x); see journalctl.\n", status);
				return -1;
		}
		return 0;
}

static int live(struct ntrlst_deck *deck, const struct settings *cfg)
{
		char err[512] = {0};
		int fd = ntrlst_input_open(cfg->device, err, sizeof(err));
		if (fd < 0) {
				fprintf(stderr, "INPUT ERROR: %s\n", err);
				return 1;
		}
		fprintf(stdout, "INPUT: %s; Fn+? help; Ctrl+Alt+W walks; Ctrl+Alt+P shutdown; Ctrl+C exits.\n",
				cfg->device ? cfg->device : NTRLST_KEYBOARD_NAME);
		struct ntrlst_input_state state = {0};
		int result = present(deck, cfg) ? 1 : 0;
		while (!result) {
				struct input_event event;
				ssize_t n = read(fd, &event, sizeof(event));

				if (n < 0 && errno == EINTR) continue;

				if (n != (ssize_t)sizeof(event)) {
						if (n < 0) perror("read evdev");
						else fprintf(stderr, "Short read or end of evdev input\n");
						result = 1; break;
				}
				char key[32];
				int has_key = ntrlst_input_decode(&state, event.type, event.code,
												  event.value, key, sizeof(key));
				if (has_key < 0) {
						result = 1;
						break;
				}
				if (has_key != 1) continue;
				enum ntrlst_change change;
				if (apply(deck, key, &change)) {
						result = 1;
						break;
				}
				if (change == NTRLST_DECK_SHUTDOWN) {
						result = request_poweroff() ? 1 : 0;
						break;
				}
				if (change == NTRLST_DECK_REDRAW && present(deck, cfg)) {
						result = 1;
						break;
				}
		}
		close(fd);
		return result;
}

int main(int argc, char **argv)
{
		struct settings cfg;
		int parsed = parse_args(argc, argv, &cfg);
		if (parsed == 1) return 0;
		if (parsed != 0) { usage(argv[0]); return 2; }
		struct ntrlst_json_hndlr json_hndlr = {0};
		char err[512] = {0};
		if (ntrlst_json_hndlr_open(&json_hndlr, cfg.data_dir, err, sizeof(err))) {
				fprintf(stderr, "DATABASE ERROR: %s\n", err);
				ntrlst_json_hndlr_close(&json_hndlr);
				return 1;
		}
		struct ntrlst_deck deck;
		if (ntrlst_deck_init(&deck, &json_hndlr)) {
				fprintf(stderr, "Application allocation failed\n");
				ntrlst_json_hndlr_close(&json_hndlr);
				return 1;
		}
		struct ntrlst_journal journal;
		char default_dir[1024];
		const char *directory = cfg.walk_dir;

		if (!directory) {
				const char *base = getenv("XDG_DATA_HOME");
				char home_base[800];

				if (!base || !*base) {
						const char *home = getenv("HOME");
						if (!home || !*home) {
								fprintf(stderr, "HOME or XDG_DATA_HOME is required for saving walks\n");
								ntrlst_deck_destroy(&deck);
								ntrlst_json_hndlr_close(&json_hndlr);
								return 1;
						}
						int m = snprintf(home_base, sizeof(home_base),
										 "%s/.local/share", home);
						if (m < 0 || (size_t)m >= sizeof(home_base)) {
								fprintf(stderr, "HOME path too long\n");
								ntrlst_deck_destroy(&deck);
								ntrlst_json_hndlr_close(&json_hndlr);
								return 1;
						}
						base = home_base;
				}

				int m = snprintf(default_dir, sizeof(default_dir),
								 "%s/naturalist/walks", base);
				if (m < 0 || (size_t)m >= sizeof(default_dir)) {
						fprintf(stderr, "directory too long\n");
						ntrlst_deck_destroy(&deck);
						ntrlst_json_hndlr_close(&json_hndlr);
						return 1;
				}
				directory = default_dir;
		}

		if (ntrlst_journal_open(&journal, directory, &json_hndlr, err, sizeof(err)) ||
			ntrlst_deck_attach_journal(&deck, &journal)) {
				fprintf(stderr, "JOURNAL ERROR: %s\n", err[0] ? err : "attach failed");
				ntrlst_deck_destroy(&deck);
				ntrlst_json_hndlr_close(&json_hndlr);
				return 1;
		}
		printf("JOURNAL: %s (%s)\n", directory,
			   journal.active ? "continued active walk" : "no active walk");

		int result = live(&deck, &cfg);

		ntrlst_deck_destroy(&deck);
		ntrlst_json_hndlr_close(&json_hndlr);
		return result;
}
