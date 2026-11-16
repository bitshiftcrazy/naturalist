#ifndef NTRLST_INPUT_H
#define NTRLST_INPUT_H

#include <stddef.h>

/* there's a HID called "kati naturalist Keyboard", this is not
 * the droid we're looking for so "kati naturalist" it is */
#define NTRLST_KEYBOARD_NAME "kati naturalist"

struct ntrlst_input_state {
		int left_shift;
		int right_shift;
		int left_ctrl;
		int right_ctrl;
		int left_alt;
		int right_alt;
};

int ntrlst_input_decode(struct ntrlst_input_state *state,
						unsigned type, unsigned code, int value,
						char *out, size_t out_size);

int ntrlst_input_open(const char *device_path, char *err, size_t err_size);

#endif
