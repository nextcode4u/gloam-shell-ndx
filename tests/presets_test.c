/* Exercise every supplied extension through the runtime parser in both modes. */
#include "app.h"
#include "associations.h"
#include "paths.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static App app;
int main(int argc, char **argv)
{
    assert(argc == 3);
    unsigned checked = 0;
    for (int mode = 0; mode <= 1; mode++) {
        app_init(&app, argv[1], true, mode != 0);
        assert(app.association_count == 31);
        FILE *file = fopen(argv[2], "r");
        assert(file);
        char line[1024];
        while (fgets(line, sizeof line, file)) {
            int expected_mode;
            char content_relative[PATH_CAP], relative[PATH_CAP];
            assert(sscanf(line, "%d\t%767[^\t]\t%767[^\n]", &expected_mode, content_relative,
                          relative) == 3);
            if (expected_mode != mode)
                continue;
            char content[PATH_CAP], actual[PATH_CAP], expected[PATH_CAP];
            assert(path_join(content, argv[1], content_relative));
            assert(association_icon_application(&app, content, actual));
            assert(path_join(expected, argv[1], relative));
            assert(!strcmp(actual, expected));
            checked++;
        }
        assert(fclose(file) == 0);
    }
    assert(checked == 158);
    puts("PASS: all 79 configured handler entries resolve in their preferred folders in DS and DSi "
         "modes");
    return 0;
}
