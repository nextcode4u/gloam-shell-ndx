#include "configuration.h"
#include "paths.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void write_text(const char *base, const char *name, const char *text)
{
    char path[PATH_CAP];
    assert(path_join(path, base, name));
    FILE *file = fopen(path, "w");
    assert(file && fputs(text, file) >= 0 && !fclose(file));
}
static void copy(const char *from, const char *to)
{
    FILE *input = fopen(from, "rb"), *output = fopen(to, "wb");
    assert(input && output);
    unsigned char buffer[512];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof buffer, input)))
        assert(fwrite(buffer, 1, bytes, output) == bytes);
    assert(!ferror(input) && !fclose(input) && !fclose(output));
}
static void check_text(const char *base, const char *name, const char *expected)
{
    char path[PATH_CAP], text[128] = "";
    assert(path_join(path, base, name));
    FILE *file = fopen(path, "r");
    assert(file);
    assert(fread(text, 1, sizeof text - 1, file) == strlen(expected));
    assert(!ferror(file) && !fclose(file) && !strcmp(text, expected));
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    const char *root = argv[1];
    char base[PATH_CAP], exported[PATH_CAP], source[PATH_CAP], pending[PATH_CAP], path[PATH_CAP];
    char error[160] = "";
    unsigned count = 99;
    assert(path_prepare_preferences_directory(root));
    assert(path_preferences_file(base, root, ""));
    write_text(base, "state.ini", "version=1\ntheme=0\n");
    write_text(base, "appearance-1.ini", "version=1\n");
    write_text(base, "never-export.sav", "Save content stays private");
    assert(configuration_export(root, error));
    assert(
        configuration_export(root, error)); /* Existing exports retain their previous revision. */
    assert(path_preferences_file(exported, root, "configuration/export.dsconfig"));
    assert(configuration_inspect(exported, &count, error) && count == 2);
    assert(path_join(source, root, "Import.dsconfig"));
    copy(exported, source);
    assert(path_preferences_file(path, root, "appearance-1.ini") && !remove(path));
    write_text(base, "state.ini", "version=1\ntheme=2\n");
    write_text(base, "recent.ini", "version=1\n");
    assert(configuration_import(root, source, error));
    check_text(base, "state.ini", "version=1\ntheme=0\n");
    check_text(base, "appearance-1.ini", "version=1\n");
    check_text(base, "recent.ini", "version=1\n");
    check_text(base, "never-export.sav", "Save content stays private");
    /* Simulate power loss after part/all of a prepared import. The rollback
     * snapshot predates the import; the pending archive defines its scope. */
    assert(path_preferences_file(pending, root, "configuration/pending.dsconfig"));
    copy(source, pending);
    assert(configuration_recover(root, error));
    check_text(base, "state.ini", "version=1\ntheme=2\n");
    assert(path_preferences_file(path, root, "appearance-1.ini") && access(path, F_OK));
    assert(access(pending, F_OK));
    copy(source, pending);
    write_text(base, "state.ini", "An external edit must survive");
    assert(!configuration_recover(root, error));
    check_text(base, "state.ini", "An external edit must survive");
    write_text(base, "state.ini", "version=1\ntheme=0\n");
    assert(configuration_recover(root, error));
    /* A damaged archive never reaches the first destination write. */
    FILE *damaged = fopen(source, "r+b");
    assert(damaged && !fseek(damaged, -1, SEEK_END));
    int last = fgetc(damaged);
    assert(last != EOF && !fseek(damaged, -1, SEEK_END));
    assert(fputc(last ^ 1, damaged) != EOF && !fclose(damaged));
    assert(!configuration_inspect(source, &count, error));
    assert(!configuration_import(root, source, error));
    check_text(base, "state.ini", "version=1\ntheme=2\n");
    /* The native and computer codecs share an actual archive fixture. */
    assert(path_join(path, root, "python.dsconfig"));
    if (!access(path, F_OK)) {
        assert(configuration_inspect(path, &count, error) && count == 1);
        assert(configuration_import(root, path, error));
        check_text(base, "state.ini", "version=1\ntheme=1\n");
    }
    char rollback[PATH_CAP];
    assert(path_preferences_file(rollback, root, "configuration/rollback.dsconfig"));
    assert(configuration_import(root, rollback, error));
    check_text(base, "state.ini", "version=1\ntheme=2\n");
    char link[PATH_CAP], original[PATH_CAP];
    assert(path_preferences_file(link, root, "configuration/export.dsconfig.tmp"));
    assert(path_preferences_file(original, root, "state.ini"));
    assert(!symlink(original, link));
    assert(!configuration_export(root, error));
    check_text(base, "state.ini", "version=1\ntheme=2\n");
    assert(!unlink(link));
    puts("PASS: native configuration interchange, bounded validation, original-record backup, "
         "merge, crash rollback, external-edit preservation and untouched saves");
}
