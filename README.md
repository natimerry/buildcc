# Build System

A parallel build system written in C with automatic dependency tracking, file globbing, and self-rebuilding capabilities.

## Requirements

- GCC or Clang
- POSIX-compliant system (Linux, BSD, macOS)
- pthread library

## Building

Compile the build system:

```bash
gcc build.c build.impl.c -o build -lpthread
```

The build system automatically rebuilds itself when modified.

## Usage

Run the build system to compile all targets:

```bash
./build
```

## Quick Start

Create a simple `build.c`:

```c
#include "build.config.h"
#include <stdio.h>

int main(int argc, char **argv) {
    rebuild_self(argc, argv);

    printf("Building myproject...\n\n");

    ensure_dir("build");

    // Compile objects
    Target *main_o = target("build/main.o",
        CMD(CC, "-c", "src/main.c", "-o", "build/main.o", "-O2"),
        NULL);

    Target *utils_o = target("build/utils.o",
        CMD(CC, "-c", "src/utils.c", "-o", "build/utils.o", "-O2"),
        NULL);

    // Link binary
    Target *myapp = target("build/myapp",
        CMD(CC, "build/main.o", "build/utils.o", "-o", "build/myapp", "-lm"),
        main_o, utils_o, NULL);

    build_target(myapp);

    printf("\nBuild complete: build/myapp\n");
    return 0;
}
```

## Configuration

Edit `build.config.h` for global settings:

```c
#define CC "gcc"
#define LINKER "gcc"
#define SELF_CC CC
#define MAX_THREADS 0  // 0 = auto-detect CPU cores
```

## API Reference

### Creating Targets

**Source files** (no build command):

```c
Target *main_src = src("src/main.c");
```

**Compiled targets** with dependencies:

```c
Target *config_h = target("include/config.h",
    CMD("sh", "-c", "sed 's/@VERSION@/1.0/g' config.h.in > config.h"),
    src("config.h.in"), NULL);
```

**Build the target**:

```c
build_target(myapp);
```

### Commands

**Create command** using CMD macro:

```c
Cmd *compile = CMD(CC, "-c", "main.c", "-o", "main.o", "-Wall");
```

**Dynamic commands**:

```c
Cmd *compile = cmd_new();
cmd_append(compile, CC, "-c", "main.c", "-o", "main.o", NULL);
cmd_append(compile, "-Wall", "-Wextra", NULL);
```

**Clone commands**:

```c
Cmd *base = CMD(CC, "-c", "-O2", "-Wall");
Cmd *compile1 = cmd_clone(base);
cmd_append(compile1, "main.c", "-o", "main.o", NULL);
```

**Extend commands**:

```c
Cmd *base_flags = CMD("-Wall", "-Wextra", "-std=c11");
Cmd *compile = cmd_new();
cmd_append(compile, CC, "-c", "main.c", "-o", "main.o", NULL);
cmd_extend(compile, base_flags);
```

### File Globbing

**Glob files in directory**:

```c
char **c_sources = glob_files("src", ".c");
char **cpp_sources = glob_files("src", ".cpp");
char **headers = glob_files("include", ".h");

// Use the files...

// Free when done
free_glob(c_sources);
free_glob(cpp_sources);
free_glob(headers);
```

**Create directory if it doesn't exist**:

```c
ensure_dir("build");
ensure_dir("output/bin");
```

### Dependency Manipulation

**Add single dependency**:

```c
Target *obj = target("main.o", compile_cmd, NULL);
add_dep(obj, config_h);
add_dep(obj, version_h);
```

**Add multiple dependencies at once**:

```c
add_deps(obj, header1, header2, header3, NULL);
```

**Check if dependency exists**:

```c
if (has_dep(obj, config_h)) {
    printf("main.o depends on config.h\n");
}
```

**Count dependencies**:

```c
size_t count = dep_count(obj);
printf("main.o has %zu dependencies\n", count);
```

**Remove specific dependency**:

```c
if (remove_dep(obj, old_header)) {
    printf("Removed old_header dependency\n");
}
```

**Clear all dependencies**:

```c
clear_deps(obj);  // Removes all dependencies
```

## Complete Example with Globbing

```c
#include "build.config.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    rebuild_self(argc, argv);

    printf("Building project...\n\n");

    ensure_dir("build");

    // Glob all source files
    char **c_sources = glob_files("src", ".c");
    if (!c_sources || !c_sources[0]) {
        panic("No source files found in src/");
    }

    // Count sources
    int count = 0;
    while (c_sources[count]) count++;
    printf("Found %d source files\n\n", count);

    // Generate config header
    Target *config_h = target("build/config.h",
        CMD("sh", "scripts/generate_config.sh"),
        NULL);

    // Compile all sources
    Target **objs = malloc(count * sizeof(Target*));
    
    for (int i = 0; i < count; i++) {
        const char *src = c_sources[i];
        
        // Extract basename
        char *base = strrchr(src, '/');
        base = base ? base + 1 : (char*)src;
        
        // Create object path
        char *obj = malloc(256);
        snprintf(obj, 256, "build/%s", base);
        char *dot = strrchr(obj, '.');
        strcpy(dot, ".o");
        
        // Create compile command
        Cmd *compile = cmd_new();
        cmd_append(compile, CC, "-c", src, "-o", obj, NULL);
        cmd_append(compile, "-Wall", "-Wextra", "-O2", "-Iinclude", NULL);
        
        // Create target without dependencies first
        objs[i] = target(obj, compile, NULL);
        
        // Add config.h as dependency to all objects
        add_dep(objs[i], config_h);
        
        // Check for matching header and add as dependency
        char header_path[256];
        snprintf(header_path, sizeof(header_path), "include/%s", base);
        dot = strrchr(header_path, '.');
        if (dot) strcpy(dot, ".h");
        
        struct stat st;
        if (stat(header_path, &st) == 0) {
            add_dep(objs[i], src(header_path));
            printf("Added %s as dependency for %s\n", header_path, obj);
        }
    }

    // Link binary
    Cmd *link = cmd_new();
    cmd_append(link, CC, "-o", "build/myapp", NULL);
    
    for (int i = 0; i < count; i++) {
        cmd_append(link, objs[i]->output, NULL);
    }
    
    cmd_append(link, "-lm", "-lpthread", NULL);

    // Create final target
    Target *myapp = calloc(1, sizeof(Target));
    myapp->output = "build/myapp";
    myapp->cmd = link;
    myapp->deps = NULL;
    
    // Add all object files as dependencies
    for (int i = 0; i < count; i++) {
        add_dep(myapp, objs[i]);
    }
    
    printf("\nBuilding %zu targets...\n", dep_count(myapp));

    build_target(myapp);

    free_glob(c_sources);

    printf("\nBuild complete: build/myapp\n");
    return 0;
}
```

## Simple Globbing Example

For projects with straightforward structure:

```c
#include "build.config.h"
#include <stdio.h>

int main(int argc, char **argv) {
    rebuild_self(argc, argv);

    ensure_dir("build");

    // Glob all C files
    char **sources = glob_files("src", ".c");
    
    int count = 0;
    while (sources[count]) count++;

    // Compile each file
    Target **objs = malloc(count * sizeof(Target*));
    for (int i = 0; i < count; i++) {
        char obj[256];
        snprintf(obj, sizeof(obj), "build/%s.o", sources[i]);
        
        objs[i] = target(strdup(obj),
            CMD(CC, "-c", sources[i], "-o", obj, "-O2"),
            NULL);
    }

    // Link
    Cmd *link = cmd_new();
    cmd_append(link, CC, "-o", "build/app", NULL);
    for (int i = 0; i < count; i++) {
        cmd_append(link, objs[i]->output, NULL);
    }
    cmd_append(link, "-lm", NULL);

    Target *app = calloc(1, sizeof(Target));
    app->output = "build/app";
    app->cmd = link;
    app->deps = NULL;
    
    for (int i = 0; i < count; i++) {
        add_dep(app, objs[i]);
    }

    build_target(app);
    free_glob(sources);

    printf("\nDone!\n");
    return 0;
}
```

## Features

- **Parallel builds** using all available CPU cores
- **Incremental rebuilds** based on modification time
- **Automatic dependency resolution** with topological ordering
- **Self-rebuilding** when build files change
- **File globbing** for automatic source discovery
- **Dynamic dependency management** with add/remove functions
- **Diamond dependency detection** - shared dependencies built once
- **Thread-safe** build queue with atomic operations
- **Zero configuration** - works out of the box

## Advanced Usage

### Cross-Compilation

```c
#define CC "x86_64-w64-mingw32-gcc"
#define LINKER "x86_64-w64-mingw32-gcc"
#define SELF_CC "gcc"  // Use native compiler for build system
```

### Conditional Dependencies

```c
Target *main_o = target("main.o", compile_cmd, NULL);

#ifdef USE_SSL
    add_dep(main_o, ssl_lib);
#endif

#ifdef ENABLE_LOGGING
    add_deps(main_o, log_header, log_impl, NULL);
#endif
```

### Cleaning Build Artifacts

```c
// Remove dependency on old config
if (has_dep(main_o, old_config)) {
    remove_dep(main_o, old_config);
    add_dep(main_o, new_config);
}

// Start fresh
Target *test_o = target("test.o", cmd, NULL);
clear_deps(test_o);  // Remove all deps
add_deps(test_o, test_h, mock_h, NULL);  // Add only what's needed
```

### Building External Dependencies

```c
Target *lua_lib = target("deps/lua/src/liblua.a",
    CMD("sh", "-c", "cd deps/lua && make linux"),
    NULL);

// Add to all objects that need it
for (int i = 0; i < obj_count; i++) {
    add_dep(objs[i], lua_lib);
}
```

## License

See LICENSE file for details.