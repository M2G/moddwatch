#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <libfswatch/c/libfswatch.h>

#define PATH_MAX_LEN      4096
#define EVENT_QUEUE_SIZE  4096 // queue size
#define MAX_WATCH_PATHS   1024 // number of (map-)paths limited to 1024
#define MAX_KNOWN_FILES   8192 // NOTE: Reduced from 65536 (was 256MB, now 32MB)

// Event flag values
typedef enum {
    FLAG_CREATED = 1,
    FLAG_UPDATED = 2,
    FLAG_REMOVED = 3,
    FLAG_RENAMED = 4
} moddwatch_flag;

typedef struct {
    char     path[PATH_MAX_LEN];
    uint32_t flags;
} queued_event;

/* NOTE :
 known_files: open-addressing hash table keyed on path string.
 Used to distinguish a true file creation from O_CREATE on an
 existing file: if we have seen a path before, Created to Updated.
*/

typedef struct {
    char known_path[PATH_MAX_LEN];
    int  used;
} known_entry;

typedef struct {
    FSW_HANDLE monitor;

    queued_event queue[EVENT_QUEUE_SIZE];
    int          queue_head;
    int          queue_tail;

    pthread_mutex_t mutex;
    pthread_t       monitor_tid;
    int             monitor_started;

    // Paths we were asked to watch (resolved, no symlinks)
    char watch_paths[MAX_WATCH_PATHS][PATH_MAX_LEN];
    int  watch_path_count;

    known_entry known_files[MAX_KNOWN_FILES];

    int          ready;
    pthread_cond_t ready_cond;
} moddwatch_handle;

static pthread_once_t fsw_init_once = PTHREAD_ONCE_INIT;
static void do_fsw_init(void) { fsw_init_library(); }

// known_files hash table

// djb2 hash @see: https://gist.github.com/MohamedTaha98/ccdf734f13299efb73ff0b12f7ce429f
static unsigned int path_hash(const char *s) {
    unsigned int h = 5381;
    while (*s) h = ((h << 5) + h) ^ (unsigned char)*s++;
    return h;
}

static int known_contains(moddwatch_handle *h, const char *path) {
    unsigned int idx = path_hash(path) % MAX_KNOWN_FILES;
    for (int i = 0; i < MAX_KNOWN_FILES; i++) {
        unsigned int slot = (idx + i) % MAX_KNOWN_FILES;
        if (!h->known_files[slot].used) return 0;
        if (strcmp(h->known_files[slot].known_path, path) == 0) return 1;
    }
    return 0;
}

static void known_insert(moddwatch_handle *h, const char *path) {
    unsigned int idx = path_hash(path) % MAX_KNOWN_FILES;
    for (int i = 0; i < MAX_KNOWN_FILES; i++) {
        unsigned int slot = (idx + i) % MAX_KNOWN_FILES;
        if (!h->known_files[slot].used ||
            strcmp(h->known_files[slot].known_path, path) == 0) {
            strncpy(h->known_files[slot].known_path, path, PATH_MAX_LEN - 1);
            h->known_files[slot].known_path[PATH_MAX_LEN - 1] = '\0';
            h->known_files[slot].used = 1;
            return;
        }
    }
    // NOTE: warning Table full log and continue (event will be treated as Updated)
    fprintf(stderr, "moddwatch: known_files table full, cannot insert %s\n", path);
}

static void known_remove(moddwatch_handle *h, const char *path) {
    unsigned int idx = path_hash(path) % MAX_KNOWN_FILES;
    for (int i = 0; i < MAX_KNOWN_FILES; i++) {
        unsigned int slot = (idx + i) % MAX_KNOWN_FILES;
        if (!h->known_files[slot].used) break;
        if (strcmp(h->known_files[slot].known_path, path) == 0) {
            h->known_files[slot].used = 0;
            h->known_files[slot].known_path[0] = '\0';
            break;
        }
    }
}

// Path helpers
static int is_directory(const char *path) {
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

static int is_allowed(moddwatch_handle *h, const char *path) {
    for (int j = 0; j < h->watch_path_count; j++) {
        size_t len = strlen(h->watch_paths[j]);
        if (strcmp(path, h->watch_paths[j]) == 0 ||
            (strncmp(path, h->watch_paths[j], len) == 0 &&
             (path[len] == '/' || path[len] == '\0'))) {
            return 1;
        }
    }
    return 0;
}

static uint32_t resolve_flags(
    moddwatch_handle *h,
    const char *path,
    int has_created,
    int has_updated,
    int has_removed,
    int has_renamed
) {
    if (has_removed) {
        known_remove(h, path);
        return FLAG_REMOVED;
    }
    if (has_created) {
        if (known_contains(h, path)) {
            return FLAG_UPDATED;
        }
        known_insert(h, path);
        return FLAG_CREATED;
    }
    if (has_updated) {
        known_insert(h, path);
        return FLAG_UPDATED;
    }
    if (has_renamed) {
        return FLAG_RENAMED;
    }
    return 0;
}

// fswatch callback
static void fswatch_callback(
    const fsw_cevent *events,
    unsigned int      event_num,
    void             *data
) {
    moddwatch_handle *h = (moddwatch_handle *)data;

    for (unsigned int i = 0; i < event_num; i++) {
        // stat() hors mutex : thread-safe
        if (is_directory(events[i].path)) continue;

        pthread_mutex_lock(&h->mutex);

        if (!h->ready) {
            h->ready = 1;
            pthread_cond_signal(&h->ready_cond);
        }

        if (!is_allowed(h, events[i].path)) {
            pthread_mutex_unlock(&h->mutex);
            continue;
        }

        int next_tail = (h->queue_tail + 1) % EVENT_QUEUE_SIZE;
        if (next_tail == h->queue_head) {
            // queue full : drop event
            pthread_mutex_unlock(&h->mutex);
            continue;
        }

        int has_created = 0, has_updated = 0, has_removed = 0, has_renamed = 0;
        for (unsigned int k = 0; k < events[i].flags_num; k++) {
            enum fsw_event_flag f = events[i].flags[k];
            if (f == Created) has_created = 1;
            if (f == Updated) has_updated = 1;
            if (f == Removed) has_removed = 1;
            if (f == Renamed) has_renamed = 1;
        }

        uint32_t flags = resolve_flags(
            h,
            events[i].path,
            has_created,
            has_updated,
            has_removed,
            has_renamed
        );

        if (flags == 0) {
            pthread_mutex_unlock(&h->mutex);
            continue;
        }

        strncpy(
            h->queue[h->queue_tail].path,
            events[i].path,
            PATH_MAX_LEN - 1
        );
        h->queue[h->queue_tail].path[PATH_MAX_LEN - 1] = '\0';
        h->queue[h->queue_tail].flags = flags;
        h->queue_tail = next_tail;

        pthread_mutex_unlock(&h->mutex);
    }
}

// API
void *moddwatch_create(void) {
     pthread_once(&fsw_init_once, do_fsw_init);
        moddwatch_handle *h = calloc(1, sizeof(moddwatch_handle));
        if (!h) return NULL;
    // switch
    #ifdef __linux__
        h->monitor = fsw_init_session(inotify_monitor_type);
    #else
        h->monitor = fsw_init_session(system_default_monitor_type); // NOTE: trying poll_monitor_type performance loss
    #endif

        if (!h->monitor) {
            free(h);
            return NULL;
        }

        h->queue_head       = 0;
        h->queue_tail       = 0;
        h->watch_path_count = 0;
        h->monitor_started  = 0;
        h->ready            = 0;

        if (pthread_mutex_init(&h->mutex, NULL) != 0) {
            fsw_destroy_session(h->monitor);
            free(h);
            return NULL;
        }

        if (pthread_cond_init(&h->ready_cond, NULL) != 0) {
            pthread_mutex_destroy(&h->mutex);
            fsw_destroy_session(h->monitor);
            free(h);
            return NULL;
        }

    fsw_set_callback(h->monitor, fswatch_callback, h);
    fsw_set_recursive(h->monitor, true);
    fsw_set_latency(h->monitor, 0.5);
    fsw_set_allow_overflow(h->monitor, true);
    fsw_set_follow_symlinks(h->monitor, true);

    return h;
}

int moddwatch_add(void *handle, const char *path) {
    moddwatch_handle *h = (moddwatch_handle *)handle;
    if (h->watch_path_count >= MAX_WATCH_PATHS) return -1;

    if (h->monitor_started) {
        fprintf(stderr, "moddwatch: cannot add path after start\n");
        return -1;
    }

    // Resolve symlinks so the filter matches what fswatch emits. macOS /var/folders to /private/var/folders
    char resolved[PATH_MAX_LEN];
    if (realpath(path, resolved) == NULL) {
        strncpy(resolved, path, PATH_MAX_LEN - 1);
        resolved[PATH_MAX_LEN - 1] = '\0';
    }

    strncpy(
        h->watch_paths[h->watch_path_count],
        resolved,
        PATH_MAX_LEN - 1
    );
    h->watch_paths[h->watch_path_count][PATH_MAX_LEN - 1] = '\0';
    h->watch_path_count++;

    // NOTE: Pre-populate known_files with files that already exist under this path, so a subsequent O_CREATE on them is seen as Updated
    struct stat st;
    if (stat(resolved, &st) == 0 && !S_ISDIR(st.st_mode)) {
        known_insert(h, resolved);
    }

    return fsw_add_path(h->monitor, resolved);
}

static void *monitor_thread(void *arg) {
    moddwatch_handle *h = (moddwatch_handle *)arg;
    fsw_start_monitor(h->monitor);
    return NULL;
}

int moddwatch_start(void *handle) {
    moddwatch_handle *h = (moddwatch_handle *)handle;

    int ret = pthread_create(&h->monitor_tid, NULL, monitor_thread, h);
    if (ret != 0) return ret;

    h->monitor_started = 1;

    // Give the monitor time to register watches before returning
    // struct timespec ts = {0, 500000000}; // 500ms
    // nanosleep(&ts, NULL);
    pthread_mutex_lock(&h->mutex);
    while (!h->ready) {
        struct timespec timeout;
        clock_gettime(CLOCK_REALTIME, &timeout);
        timeout.tv_sec += 2; // max 2s timeout
        if (pthread_cond_timedwait(&h->ready_cond, &h->mutex, &timeout) != 0) {
            break; // timeout : proceed anyway
        }
    }
    pthread_mutex_unlock(&h->mutex);

    return FSW_OK;
}

int moddwatch_next(void *handle, char *path, uint32_t *flags) {
    moddwatch_handle *h = (moddwatch_handle *)handle;

    pthread_mutex_lock(&h->mutex);
    if (h->queue_head == h->queue_tail) {
        pthread_mutex_unlock(&h->mutex);
        return 0;
    }

    strncpy(path, h->queue[h->queue_head].path, PATH_MAX_LEN - 1);
    path[PATH_MAX_LEN - 1] = '\0';
    *flags = h->queue[h->queue_head].flags;
    h->queue_head = (h->queue_head + 1) % EVENT_QUEUE_SIZE;

    pthread_mutex_unlock(&h->mutex);
    return 1;
}

void moddwatch_destroy(void *handle) {
    moddwatch_handle *h = (moddwatch_handle *)handle;
    if (!h) return;

    // NOTE: Only join if the monitor thread was started
    if (h->monitor_started) {
        fsw_stop_monitor(h->monitor);
        pthread_join(h->monitor_tid, NULL);
    }
    pthread_cond_destroy(&h->ready_cond);
    fsw_destroy_session(h->monitor);
    pthread_mutex_destroy(&h->mutex);
    free(h);
}
