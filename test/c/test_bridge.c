#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "fswatch_bridge.h"

static int tests_run    = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define ASSERT(cond, msg) do { \
    tests_run++; \
    if (cond) { \
        tests_passed++; \
        printf("  OK %s\n", msg); \
    } else { \
        tests_failed++; \
        printf("  KO %s (line %d)\n", msg, __LINE__); \
    } \
} while(0)

#define TEST(name) printf("\n[TEST] %s\n", name)

static char tmpdir[256];

static void setup_tmpdir(void) {
    snprintf(tmpdir, sizeof(tmpdir), "/tmp/moddwatch_test_XXXXXX");
    mkdtemp(tmpdir);
}

static void teardown_tmpdir(void) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", tmpdir);
    system(cmd);
}

static void write_file(const char *name, const char *content) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", tmpdir, name);
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd >= 0) {
        write(fd, content, strlen(content));
        close(fd);
    }
}

static void delete_file(const char *name) {
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", tmpdir, name);
    unlink(path);
}

static int poll_events(void *handle, int timeout_ms) {
    char path[4096];
    uint32_t flags;
    int count = 0;
    int elapsed = 0;

    while (elapsed < timeout_ms) {
        if (moddwatch_next(handle, path, &flags)) {
            printf("    event: path=%s flags=%u\n", path, flags);
            count++;
        } else {
            usleep(10000); /* 10ms */
            elapsed += 10;
        }
    }
    return count;
}

void test_create_destroy(void) {
    TEST("create and destroy");

    void *h = moddwatch_create();
    ASSERT(h != NULL, "moddwatch_create returns non-null handle");

    moddwatch_destroy(h);
    ASSERT(1, "moddwatch_destroy completes without crash");
}

void test_add_path(void) {
    TEST("add path");

    void *h = moddwatch_create();
    ASSERT(h != NULL, "handle created");

    int ret = moddwatch_add(h, tmpdir);
    ASSERT(ret == 0, "moddwatch_add returns 0 on success");

    moddwatch_destroy(h);
}

void test_add_invalid_path(void) {
    TEST("add invalid path");

    void *h = moddwatch_create();
    ASSERT(h != NULL, "handle created");

    // Non-existent path : fswatch may still accept it
    int ret = moddwatch_add(h, "/nonexistent/path/xyz");
    printf("    moddwatch_add non-existent path returned: %d\n", ret);
    ASSERT(1, "moddwatch_add with non-existent path does not crash");

    moddwatch_destroy(h);
}

void test_create_event(void) {
    TEST("detect file creation");

    void *h = moddwatch_create();
    ASSERT(h != NULL, "handle created");

    moddwatch_add(h, tmpdir);
    moddwatch_start(h);

    // Give the monitor time to start
    usleep(600000); // 600ms

    write_file("created.txt", "hello");

    int count = poll_events(h, 2000);
    ASSERT(count > 0, "received at least one event after file creation");

    // Check we get a Create event (flags=1)
    char path[4096];
    uint32_t flags = 0;

    // Re-poll to check flags
    void *h2 = moddwatch_create();
    moddwatch_add(h2, tmpdir);
    moddwatch_start(h2);
    usleep(600000);
    write_file("created2.txt", "hello");

    int elapsed = 0;
    int got_create = 0;
    while (elapsed < 2000) {
        if (moddwatch_next(h2, path, &flags)) {
            if (flags == 1) got_create = 1;
        } else {
            usleep(10000);
            elapsed += 10;
        }
    }
    ASSERT(got_create, "received Create event (flags=1)");

    moddwatch_destroy(h2);
    moddwatch_destroy(h);
}

void test_modify_event(void) {
    TEST("detect file modification");

    // Create file before starting watcher
    write_file("modify.txt", "initial");

    void *h = moddwatch_create();
    ASSERT(h != NULL, "handle created");

    moddwatch_add(h, tmpdir);
    moddwatch_start(h);
    usleep(600000);

    // Modify existing file
    write_file("modify.txt", "modified");

    char path[4096];
    uint32_t flags = 0;
    int elapsed = 0;
    int got_write = 0;

    while (elapsed < 2000) {
        if (moddwatch_next(h, path, &flags)) {
            printf("    event: flags=%u path=%s\n", flags, path);
            if (flags == 1 || flags == 2) got_write = 1; // Fix: Write/Updated
        } else {
            usleep(10000);
            elapsed += 10;
        }
    }
    ASSERT(got_write, "received Write or Create event for modification");

    moddwatch_destroy(h);
}

void test_delete_event(void) {
    TEST("detect file deletion");

    write_file("delete.txt", "to be deleted");

    void *h = moddwatch_create();
    ASSERT(h != NULL, "handle created");

    moddwatch_add(h, tmpdir);
    moddwatch_start(h);
    usleep(600000);

    delete_file("delete.txt");

    char path[4096];
    uint32_t flags = 0;
    int elapsed = 0;
    int got_remove = 0;

    while (elapsed < 2000) {
        if (moddwatch_next(h, path, &flags)) {
            printf("    event: flags=%u path=%s\n", flags, path);
            if (flags == 3) got_remove = 1; // Removed
        } else {
            usleep(10000);
            elapsed += 10;
        }
    }
    ASSERT(got_remove, "received Remove event (flags=3) for deletion");

    moddwatch_destroy(h);
}

void test_multiple_destroy(void) {
    TEST("multiple create/destroy cycles");

    for (int i = 0; i < 5; i++) {
        void *h = moddwatch_create();
        ASSERT(h != NULL, "handle created");
        moddwatch_add(h, tmpdir);
        moddwatch_start(h);
        usleep(100000);
        moddwatch_destroy(h);
    }
    ASSERT(1, "5 create/destroy cycles completed without crash");
}

int main(void) {
    printf("moddwatch bridge C unit tests\n");
    printf("==============================\n");

    setup_tmpdir();
    printf("tmpdir: %s\n", tmpdir);

    test_create_destroy();
    test_add_path();
    test_add_invalid_path();
    test_create_event();
    test_modify_event();
    test_delete_event();
    test_multiple_destroy();

    teardown_tmpdir();

    printf("\n==============================\n");
    printf("Results: %d/%d passed", tests_passed, tests_run);
    if (tests_failed > 0) {
        printf(", %d FAILED", tests_failed);
    }
    printf("\n");

    return tests_failed > 0 ? 1 : 0;
}