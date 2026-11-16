#define _POSIX_C_SOURCE 200809L
#include "input.h"

#include <dirent.h>
#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

struct key_map {
		unsigned code;
		const char *name;
};

/* Z, Y: I'm German... */
static const struct key_map key_map[] = {
		{ KEY_Q, "Q" },
		{ KEY_W, "W" },
		{ KEY_E, "E" },
		{ KEY_R, "R" },
		{ KEY_T, "T" },
		{ KEY_Y, "Z" },
		{ KEY_U, "U" },
		{ KEY_I, "I" },
		{ KEY_O, "O" },
		{ KEY_P, "P" },
		{ KEY_A, "A" },
		{ KEY_S, "S" },
		{ KEY_D, "D" },
		{ KEY_F, "F" },
		{ KEY_G, "G" },
		{ KEY_H, "H" },
		{ KEY_J, "J" },
		{ KEY_K, "K" },
		{ KEY_L, "L" },
		{ KEY_Z, "Y" },
		{ KEY_X, "X" },
		{ KEY_C, "C" },
		{ KEY_V, "V" },
		{ KEY_B, "B" },
		{ KEY_N, "N" },
		{ KEY_M, "M" },
		{ KEY_TAB, "Tab" },
		{ KEY_ENTER, "Enter" },
		{ KEY_BACKSPACE, "Backspace" },
		{ KEY_ESC, "Escape" },
		{ KEY_F1, "F1" },
		{ KEY_F2, "F2" },
		{ KEY_UP, "Up" },
		{ KEY_DOWN, "Down" },
		{ KEY_LEFT, "Left" },
		{ KEY_RIGHT, "Right" },
		{ KEY_PAGEUP, "PageUp" },
		{ KEY_PAGEDOWN, "PageDown" },
		{ KEY_1, "1" },
		{ KEY_2, "2" },
		{ KEY_3, "3" },
		{ KEY_4, "4" },
		{ KEY_5, "5" },
		{ KEY_6, "6" },
		{ KEY_7, "7" },
		{ KEY_8, "8" },
		{ KEY_9, "9" },
		{ KEY_0, "0" },
		{ KEY_SPACE, "Space" },
};

int ntrlst_input_decode(struct ntrlst_input_state *state,
						unsigned type, unsigned code, int value,
						char *out, size_t out_size)
{
		if (!state || !out || !out_size)
				return -1;

		out[0] = '\0';

		if (type != EV_KEY)
				return 0;

		if (code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT) {
				int *held = code == KEY_LEFTSHIFT ?
						&state->left_shift : &state->right_shift;

				if (value == 0)
						*held = 0;
				else if (value == 1)
						*held = 1;

				return 0;
		}

		if (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL ||
			code == KEY_LEFTALT || code == KEY_RIGHTALT) {
				int *held = NULL;

				switch (code) {
				case KEY_LEFTCTRL:
						held = &state->left_ctrl;
						break;
				case KEY_RIGHTCTRL:
						held = &state->right_ctrl;
						break;
				case KEY_LEFTALT:
						held = &state->left_alt;
						break;
				case KEY_RIGHTALT:
						held = &state->right_alt;
						break;
				default:
						break;
				}

				if (value == 0)
						*held = 0;
				else if (value == 1)
						*held = 1;

				return 0;
		}

		if (value != 1)
				return 0;

		bool ctrl = state->left_ctrl || state->right_ctrl;
		bool alt = state->left_alt || state->right_alt;

		if (ctrl || alt) {
				if (ctrl && alt && (code == KEY_P || code == KEY_W)) {
						int n = snprintf(out, out_size, "Ctrl+Alt+%c",
										 code == KEY_P ? 'P' : 'W');

						if (n < 0 || (size_t)n >= out_size) {
								out[0] = '\0';
								return -1;
						}

						return 1;
				}
				return 0;
		}

		for (size_t i = 0; i < sizeof(key_map) / sizeof(key_map[0]); ++i) {
				if (key_map[i].code != code)
						continue;

				const char *prefix = "";

				if (strlen(key_map[i].name) == 1 &&
					key_map[i].name[0] >= 'A' && key_map[i].name[0] <= 'Z' &&
					(state->left_shift || state->right_shift))
						prefix = "Shift+";

				int n = snprintf(out, out_size, "%s%s", prefix, key_map[i].name);

				if (n < 0 || (size_t)n >= out_size) {
						out[0] = '\0';
						return -1;
				}

				return 1;
		}

		return 0;
}

int ntrlst_input_open(const char *device_path, char *err, size_t err_size)
{
		if (device_path) {
				int fd = open(device_path, O_RDONLY | O_CLOEXEC);

				if (fd < 0 && err && err_size)
						snprintf(err, err_size, "open(%s): %s", device_path,
								 strerror(errno));

				return fd;
		}

		DIR *dir = opendir("/dev/input");

		if (!dir) {
				if (err && err_size)
						snprintf(err, err_size, "opendir(/dev/input): %s",
								 strerror(errno));
				return -1;
		}

		struct dirent *entry;
		int fd = -1;

		while ((entry = readdir(dir)) != NULL) {
				if (strncmp(entry->d_name, "event", 5) != 0)
						continue;

				char path[512];
				int n = snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);

				if (n < 0 || (size_t)n >= sizeof(path))
						continue;

				int candidate = open(path, O_RDONLY | O_CLOEXEC);

				if (candidate < 0)
						continue;

				char name[256] = {0};

				if (ioctl(candidate, EVIOCGNAME(sizeof(name)), name) >= 0 &&
					strcmp(name, NTRLST_KEYBOARD_NAME) == 0) {
						fd = candidate;
						break;
				}

				close(candidate);
		}

		closedir(dir);

		if (fd < 0 && err && err_size)
				snprintf(err, err_size,
						 "your evdev device is in another castle");

		return fd;
}
