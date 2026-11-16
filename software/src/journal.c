#define _POSIX_C_SOURCE 200809L
#include "journal.h"

#include <jansson.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

static int report_errno(char *err, size_t size, const char *operation)
{
		snprintf(err, size, "%s: %s", operation, strerror(errno));

		return -1;
}

static int report_error(char *err, size_t size, const char *message)
{
		snprintf(err, size, "%s", message);

		return -1;
}

static int copy_string(char *dest, size_t n, const char *source)
{
		if (!source || strlen(source) >= n)
				return -1;

		memcpy(dest, source, strlen(source) + 1);

		return 0;
}

static int walk_path(const struct ntrlst_journal *j, const char *name,
					 char *out, size_t n)
{
		int len = snprintf(out, n, "%s/%s", j->directory, name);

		return len < 0 || (size_t)len >= n ? -1 : 0;
}

static int make_dirs(const char *path, char *err, size_t n)
{
		char buf[1024];
		char *p;

		if (copy_string(buf, sizeof(buf), path))
				return report_error(err, n, "path too long");

		for (p = buf + 1; ; ++p) {
				if (*p && *p != '/')
						continue;

				char saved = *p;

				*p = 0;
				if (mkdir(buf, 0700) < 0 && errno != EEXIST)
						return report_errno(err, n, "mkdir walks");

				struct stat st;

				if (stat(buf, &st) || !S_ISDIR(st.st_mode))
						return report_error(err, n,
											"not a directory");

				*p = saved;
				if (!saved)
						break;
		}

		return 0;
}

static void timestamp(char *out, size_t n, const char *fmt)
{
		time_t now = time(NULL);
		struct tm tm = {0};

		if (localtime_r(&now, &tm) && strftime(out, n, fmt, &tm))
				return;

		if (n)
				out[0] = '\0';
}

static json_t *json_for_walk(const struct ntrlst_journal *j,
							 const char *ended_at)
{
		json_t *root = json_object();

		if (!root)
				return NULL;

		json_t *entries = json_array();

		if (!entries) {
				json_decref(root);
				return NULL;
		}

		if (json_object_set_new(root, "schema_version", json_integer(1)) ||
			json_object_set_new(root, "id", json_string(j->id)) ||
			json_object_set_new(root, "started_at",
								json_string(j->started_at)) ||
			json_object_set_new(root, "ended_at", ended_at ?
								json_string(ended_at) : json_null()) ||
			json_object_set_new(root, "location", json_string(j->location)) ||
			json_object_set_new(root, "note", json_string(j->note)))
				goto failed;

		for (size_t i = 0; i < j->finding_count; ++i) {
				const struct ntrlst_finding *f = &j->findings[i];
				json_t *entry = json_object();

				if (!entry)
						goto failed;

				if (json_object_set_new(entry, "species_id",
										json_string(f->species_id)) ||
					json_object_set_new(entry, "recorded_at",
										json_string(f->recorded_at)) ||
					json_object_set_new(entry, "note", json_string(f->note)) ||
					json_array_append(entries, entry)) {
						json_decref(entry);
						goto failed;
				}

				json_decref(entry);
		}

		if (json_object_set(root, "findings", entries))
				goto failed;

		json_decref(entries);

		return root;

failed:
		json_decref(entries);
		json_decref(root);

		return NULL;
}

static int sync_dir(const struct ntrlst_journal *j)
{
		int fd = open(j->directory, O_RDONLY | O_DIRECTORY | O_CLOEXEC);

		if (fd < 0)
				return -1;

		int ret = fsync(fd);
		int saved = errno;

		close(fd);
		errno = saved;

		return ret;
}

static int write_atomic(const struct ntrlst_journal *j, const char *name,
						const char *ended, char *err, size_t n)
{
		json_t *root = json_for_walk(j, ended);

		if (!root)
				return report_error(err, n, "cant serialize walk");

		char *payload = json_dumps(root, JSON_INDENT(2) | JSON_ENSURE_ASCII);

		json_decref(root);
		if (!payload)
				return report_error(err, n, "cant encode walk JSON");

		char path[1200], tmp[1240];

		if (walk_path(j, name, path, sizeof(path))) {
				free(payload);
				return report_error(err, n, "path too long");
		}

		int len = snprintf(tmp, sizeof(tmp), "%s.tmp.%ld", path,
						   (long)getpid());

		if (len < 0 || (size_t)len >= sizeof(tmp)) {
				free(payload);
				return report_error(err, n, "temp path too long");
		}

		int fd = open(tmp, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);

		if (fd < 0) {
				free(payload);
				return report_errno(err, n, "open walk temp file");
		}

		size_t remaining = strlen(payload);
		const char *p = payload;
		int success = 0;

		while (remaining) {
				ssize_t written = write(fd, p, remaining);

				if (written < 0 && errno == EINTR)
						continue;
				if (written <= 0) {
						success = -1;
						break;
				}

				remaining -= (size_t)written;
				p += written;
		}

		if (!success && fsync(fd))
				success = -1;

		int saved = errno;

		if (close(fd) && !success) {
				success = -1;
				saved = errno;
		}

		free(payload);
		if (success) {
				unlink(tmp);
				errno = saved;
				return report_errno(err, n, "write walk");
		}

		if (rename(tmp, path)) {
				saved = errno;
				unlink(tmp);
				errno = saved;
				return report_errno(err, n, "rename walk");
		}

		/*
		 * Should probably fix:
		 * if fsync fails after rename the file could already have
		 * new state. atm callers undo their in memory state,
		 * that could cause inconsistencies.
		 * no idea how liekly this is
		 */
		if (sync_dir(j))
				return report_errno(err, n, "fsync walk dir");

		return 0;
}

static int persist(struct ntrlst_journal *j, char *err, size_t n)
{
		return write_atomic(j, "active.json", NULL, err, n);
}

const struct ntrlst_finding *ntrlst_journal_find(
		const struct ntrlst_journal *j, const char *id)
{
		if (!j || !id)
				return NULL;

		for (size_t i = 0; i < j->finding_count; ++i)
				if (!strcmp(j->findings[i].species_id, id))
						return &j->findings[i];

		return NULL;
}

static int load_active(struct ntrlst_journal *j,
					   const struct ntrlst_json_hndlr *json_hndlr, char *err, size_t n)
{
		char path[1200];

		if (walk_path(j, "active.json", path, sizeof(path)))
				return report_error(err, n, "path too long");

		if (access(path, F_OK) < 0) {
				if (errno == ENOENT)
						return 0;
				return report_errno(err, n, "access active walk");
		}

		json_error_t parse_error;
		json_t *root = json_load_file(path, 0, &parse_error);

		if (!root) {
				if (err && n)
						snprintf(err, n, "can't load active walk: %s",
								 parse_error.text);
				return -1;
		}

		int ret = -1;
		const char *id = json_string_value(json_object_get(root, "id"));
		const char *started = json_string_value(
				json_object_get(root, "started_at"));
		const char *location = json_string_value(
				json_object_get(root, "location"));
		const char *note = json_string_value(json_object_get(root, "note"));
		json_t *findings = json_object_get(root, "findings");

		if (!json_is_integer(json_object_get(root, "schema_version")) ||
			json_integer_value(json_object_get(root, "schema_version")) != 1 ||
			!json_is_null(json_object_get(root, "ended_at")) ||
			!id ||
			strspn(id, "abcdefghijklmnopqrstuvwxyz"
				   "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_") != strlen(id) ||
			!started || !location || !note || !json_is_array(findings) ||
			json_array_size(findings) > NTRLST_WALK_MAX_FINDINGS ||
			copy_string(j->id, sizeof(j->id), id) ||
			copy_string(j->started_at, sizeof(j->started_at), started) ||
			copy_string(j->location, sizeof(j->location), location) ||
			copy_string(j->note, sizeof(j->note), note)) {
				report_error(err, n, "Invalid active walk schema");
				goto done;
		}

		for (size_t i = 0; i < json_array_size(findings); ++i) {
				json_t *entry = json_array_get(findings, i);
				const char *species_id = json_string_value(
						json_object_get(entry, "species_id"));
				const char *recorded = json_string_value(
						json_object_get(entry, "recorded_at"));
				const char *entry_note = json_string_value(
						json_object_get(entry, "note"));
				struct ntrlst_finding *f = &j->findings[i];

				if (!species_id || !recorded || !entry_note ||
					!ntrlst_find_species(json_hndlr, species_id) ||
					copy_string(f->species_id, sizeof(f->species_id),
								species_id) ||
					copy_string(f->recorded_at, sizeof(f->recorded_at),
								recorded) ||
					copy_string(f->note, sizeof(f->note), entry_note)) {
						report_error(err, n, "Invalid finding in active walk "
									 "(file preserved)");
						goto done;
				}

				j->finding_count++;
		}

		j->active = true;
		ret = 0;

done:
		json_decref(root);

		return ret;
}

int ntrlst_journal_open(struct ntrlst_journal *j, const char *directory,
						const struct ntrlst_json_hndlr *json_hndlr, char *err, size_t n)
{
		if (!j || !directory || !*directory || !json_hndlr)
				return report_error(err, n, "Invalid journal arguments");

		memset(j, 0, sizeof(*j));
		if (copy_string(j->directory, sizeof(j->directory), directory))
				return report_error(err, n, "path too long");
		if (make_dirs(directory, err, n))
				return -1;

		return load_active(j, json_hndlr, err, n);
}

int ntrlst_journal_start(struct ntrlst_journal *j, char *err, size_t n)
{
		if (!j || j->active)
				return report_error(err, n,
									"Walk already active or journal missing");

		struct ntrlst_journal previous = *j;

		j->finding_count = 0;
		j->location[0] = j->note[0] = 0;
		timestamp(j->started_at, sizeof(j->started_at), "%Y-%m-%dT%H:%M:%S%z");

		char prefix[48];

		timestamp(prefix, sizeof(prefix), "%Y%m%d-%H%M%S");

		struct timespec ts = {0};

		(void)clock_gettime(CLOCK_REALTIME, &ts);

		int m = snprintf(j->id, sizeof(j->id), "%s-%ld-%09ld", prefix,
						 (long)getpid(), ts.tv_nsec);

		if (m < 0 || (size_t)m >= sizeof(j->id) || persist(j, err, n)) {
				*j = previous;
				return -1;
		}

		j->active = true;

		return 0;
}

static int persist_string(struct ntrlst_journal *j, char *dest,
						  size_t size, const char *text, char *err, size_t n)
{
		char old[NTRLST_WALK_NOTE_SIZE];

		if (!j || !j->active)
				return report_error(err, n, "No active walk");

		if (!text)
				return report_error(err, n, "Missing text");

		if (strlen(text) >= size)
				return report_error(err, n, "Text too long");

		memcpy(old, dest, size);
		memcpy(dest, text, strlen(text) + 1);

		if (persist(j, err, n)) {
				memcpy(dest, old, size);
				return -1;
		}

		return 0;
}

int ntrlst_journal_set_location(struct ntrlst_journal *j,
								const char *text, char *err, size_t n)
{
		if (!j)
				return report_error(err, n, "Missing journal");

		return persist_string(j, j->location, sizeof(j->location),
							  text, err, n);
}

int ntrlst_journal_set_note(struct ntrlst_journal *j,
							const char *text, char *err, size_t n)
{
		if (!j)
				return report_error(err, n, "Missing journal");

		return persist_string(j, j->note, sizeof(j->note),
							  text, err, n);
}

int ntrlst_journal_add(struct ntrlst_journal *j, const char *id,
					   char *err, size_t n)
{
		if (!j || !j->active || !id ||
			strlen(id) >= sizeof(j->findings[0].species_id))
				return report_error(err, n, "can't add finding to inactive walk");

		if (ntrlst_journal_find(j, id))
				return 0;

		if (j->finding_count >= NTRLST_WALK_MAX_FINDINGS)
				return report_error(err, n, "finding limit exceeded");

		struct ntrlst_finding *f = &j->findings[j->finding_count++];

		memset(f, 0, sizeof(*f));
		strcpy(f->species_id, id);
		timestamp(f->recorded_at, sizeof(f->recorded_at),
				  "%Y-%m-%dT%H:%M:%S%z");

		if (persist(j, err, n)) {
				--j->finding_count;
				return -1;
		}

		return 0;
}

int ntrlst_journal_remove(struct ntrlst_journal *j, const char *id,
						  char *err, size_t n)
{
		if (!j || !j->active || !id)
				return report_error(err, n, "No active walk");

		for (size_t i = 0; i < j->finding_count; ++i) {
				if (strcmp(j->findings[i].species_id, id))
						continue;

				struct ntrlst_finding backup = j->findings[i];

				memmove(&j->findings[i], &j->findings[i+1],
						(j->finding_count - i - 1) * sizeof(j->findings[0]));
				--j->finding_count;

				if (persist(j, err, n)) {
						memmove(&j->findings[i+1], &j->findings[i],
								(j->finding_count - i) *
								sizeof(j->findings[0]));
						j->findings[i] = backup;
						++j->finding_count;
						return -1;
				}

				return 0;
		}

		return report_error(err, n, "Finding not present");
}

int ntrlst_journal_set_finding_note(struct ntrlst_journal *j,
									const char *id, const char *text,
									char *err, size_t n)
{
		if (!j || !j->active || !id)
				return report_error(err, n, "No active walk");

		for (size_t i = 0; i < j->finding_count; ++i)
				if (!strcmp(j->findings[i].species_id, id))
						return persist_string(j, j->findings[i].note,
											  sizeof(j->findings[i].note),
											  text, err, n);

		return report_error(err, n, "Finding not present");
}

int ntrlst_journal_finish(struct ntrlst_journal *j, char *err, size_t n)
{
		if (!j || !j->active)
				return report_error(err, n, "No active walk to finish");

		char archived[100];
		int len = snprintf(archived, sizeof(archived), "%s.json", j->id);

		if (len < 0 || (size_t)len >= sizeof(archived))
				return report_error(err, n, "ID too long");

		char ended[48];

		timestamp(ended, sizeof(ended), "%Y-%m-%dT%H:%M:%S%z");
		if (write_atomic(j, archived, ended, err, n))
				return -1;

		char path[1200];

		if (walk_path(j, "active.json", path, sizeof(path)))
				return report_error(err, n, "path too long");
		if (unlink(path) && errno != ENOENT)
				return report_errno(err, n, "remove active walk");
		if (sync_dir(j))
				return report_errno(err, n, "fsync walk directory");

		j->active = false;

		return 0;
}
