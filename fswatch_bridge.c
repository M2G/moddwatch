#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <stdint.h>
#include <time.h>
#include <sys/stat.h>
#include <libfswatch/c/libfswatch.h>

#define EVENT_QUEUE_SIZE  4096 // queue size
#define MAX_WATCH_PATHS   1024 // number of (map-)paths limited to 1024
#define MAX_KNOWN_FILES   65536 // max locked memory

static pthread_once_t fsw_init_once = PTHREAD_ONCE_INIT;
static void do_fsw_init(void) { fsw_init_library(); }

typedef struct {
    char     path[4096];
    uint32_t flags;
} queued_event;

typedef struct {
    FSW_HANDLE monitor;

    queued_event queue[EVENT_QUEUE_SIZE];
    int          queue_head;
    int          queue_tail;

    pthread_mutex_t mutex;
    pthread_t monitor_tid;

    // Paths we were asked to watch (resolved, no symlinks)
    char watch_paths[MAX_WATCH_PATHS][4096];
    int  watch_path_count;

    /* NOTE :
     * Set of file paths we have already emitted at least one event for.
     * Used to distinguish a true creation from O_CREATE on an existing
     * file: if we've seen a path before, Created to Updated.
     *
     * Simple open-addressing hash table keyed on path string.
     */
    char known_files[MAX_KNOWN_FILES][4096];
    int  known_used[MAX_KNOWN_FILES];
} moddwatch_handle;

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
        if (!h->known_used[slot]) return 0;
        if (strcmp(h->known_files[slot], path) == 0) return 1;
    }
    return 0;
}

static void known_insert(moddwatch_handle *h, const char *path) {
    unsigned int idx = path_hash(path) % MAX_KNOWN_FILES;
    for (int i = 0; i < MAX_KNOWN_FILES; i++) {
        unsigned int slot = (idx + i) % MAX_KNOWN_FILES;
        if (!h->known_used[slot] || strcmp(h->known_files[slot], path) == 0) {
            strncpy(h->known_files[slot], path, 4095);
            h->known_files[slot][4095] = '\0';
            h->known_used[slot] = 1;
            return;
        }
    }
}

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

static void fswatch_callback(
    const fsw_cevent *events,
    unsigned int      event_num,
    void             *data
) {
    moddwatch_handle *h = (moddwatch_handle *)data;

    pthread_mutex_lock(&h->mutex);

    for (unsigned int i = 0; i < event_num; i++) {
        if (is_directory(events[i].path)) continue;
        if (!is_allowed(h, events[i].path))  continue;

        int next_tail = (h->queue_tail + 1) % EVENT_QUEUE_SIZE;
        if (next_tail == h->queue_head) continue;

        int has_created = 0, has_updated = 0, has_removed = 0, has_renamed = 0;
        for (unsigned int k = 0; k < events[i].flags_num; k++) {
            enum fsw_event_flag f = events[i].flags[k];
            if (f == Created) has_created = 1;
            if (f == Updated) has_updated = 1;
            if (f == Removed) has_removed = 1;
            if (f == Renamed) has_renamed = 1;
        }

        uint32_t flags = 0;
        if (has_removed) {
            flags = 3;
            // remove from known set on deletion
        } else if (has_created) {
            if (known_contains(h, events[i].path)) {
                // File was already known - O_CREATE on existing file
                flags = 2; // Updated / Write
            } else {
                flags = 1; // genuinely new
                known_insert(h, events[i].path);
            }
        } else if (has_updated) {
            flags = 2;
            known_insert(h, events[i].path);
        } else if (has_renamed) {
            flags = 4;
        }

        if (flags == 0) continue;

        strncpy(
            h->queue[h->queue_tail].path,
            events[i].path,
            sizeof(h->queue[h->queue_tail].path) - 1
        );
        h->queue[h->queue_tail].path[sizeof(h->queue[h->queue_tail].path) - 1] = '\0';
        h->queue[h->queue_tail].flags = flags;
        h->queue_tail = next_tail;
    }

    pthread_mutex_unlock(&h->mutex);
}

void *moddwatch_create(void) {
    pthread_once(&fsw_init_once, do_fsw_init);

    moddwatch_handle *h = calloc(1, sizeof(moddwatch_handle));
    if (!h) return NULL;
    // switch
    #ifdef __linux__
        h->monitor = fsw_init_session(inotify_monitor_type);
    #else
        h->monitor = fsw_init_session(system_default_monitor_type);
    #endif
    if (!h->monitor) { free(h); return NULL; }

    h->queue_head       = 0;
    h->queue_tail       = 0;
    h->watch_path_count = 0;

    pthread_mutex_init(&h->mutex, NULL);
    fsw_set_callback(h->monitor, fswatch_callback, h);
    fsw_set_recursive(h->monitor, true);
    fsw_set_latency(h->monitor, 0.5);

    return h;
}

int moddwatch_add(void *handle, const char *path) {
    moddwatch_handle *h = (moddwatch_handle *)handle;
    if (h->watch_path_count >= MAX_WATCH_PATHS) return -1;

    char resolved[4096];
    if (realpath(path, resolved) == NULL) {
        strncpy(resolved, path, sizeof(resolved) - 1);
        resolved[sizeof(resolved) - 1] = '\0';
    }

    strncpy(
        h->watch_paths[h->watch_path_count],
        resolved,
        sizeof(h->watch_paths[h->watch_path_count]) - 1
    );
    h->watch_paths[h->watch_path_count][sizeof(h->watch_paths[h->watch_path_count]) - 1] = '\0';
    h->watch_path_count++;

    /* NOTE :
     * Pre-populate known_files with files that already exist under this
     * path, so that a subsequent O_CREATE on them is seen as Updated.
     */
    // We only pre-populate the exact path if it's a file
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

    struct timespec ts = {0, 500000000}; // 500ms
    nanosleep(&ts, NULL);

    return FSW_OK;
}

int moddwatch_next(void *handle, char *path, uint32_t *flags) {
    moddwatch_handle *h = (moddwatch_handle *)handle;

    pthread_mutex_lock(&h->mutex);
    if (h->queue_head == h->queue_tail) {
        pthread_mutex_unlock(&h->mutex);
        return 0;
    }

    strncpy(path, h->queue[h->queue_head].path, 4095);
    path[4095] = '\0';
    *flags = h->queue[h->queue_head].flags;
    h->queue_head = (h->queue_head + 1) % EVENT_QUEUE_SIZE;

    pthread_mutex_unlock(&h->mutex);
    return 1;
}

void moddwatch_destroy(void *handle) {
    moddwatch_handle *h = (moddwatch_handle *)handle;
    if (!h) return;
    fsw_stop_monitor(h->monitor);
    pthread_join(h->monitor_tid, NULL);
    fsw_destroy_session(h->monitor);
    pthread_mutex_destroy(&h->mutex);
    free(h);
}
