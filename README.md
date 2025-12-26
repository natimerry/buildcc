# Build System

A parallel build system written in C with automatic dependency tracking and self-rebuilding capabilities.

## Requirements

- GCC or compatible C compiler
- POSIX-compliant system (Linux, BSD, macOS)
- pthread library

## Building

Compile the build system:

    gcc build.c -o build -lpthread

The build system automatically rebuilds itself when modified.

## Usage

Run the build system to compile all targets:

    ./build

### Configuration

Edit build configuration at the top of `build.c`:

    #define CC      "x86_64-w64-mingw32-gcc"
    #define SELF_CC "gcc"
    #define LINKER  "x86_64-w64-mingw32-gcc"
    #define MAX_THREADS 8

`SELF_CC` specifies the compiler used to rebuild the build system itself, useful when cross-compiling.

Optional build.config.h file for additional configuration.

### Defining Targets

Source files (no build command):

    SOURCE(src_main, "src/main.c");

Generated files with custom commands:

    TARGET(config_h, "include/config.h", DEPS(&config_h_in),
           CMD("sh", "-c", "sed 's/@VERSION@/1.0/g' config.h.in > config.h"));

Binary targets from multiple sources:

    const char* sources[] = {"main.c", "util.c", "parser.c", NULL};
    BINARY(program,              // Target name
           "program",            // Output file
           sources,              // Source files
           DEPS(&config_h),      // Dependencies
           "-Iinclude",          // Include flags
           LINKER,               // Linker (NULL for CC)
           "-lpthread");         // Linker flags

Build specific target:

    build_target(program);

### Manual Target Definition

For fine-grained control, define targets manually without BINARY:

    // Source files
    SOURCE(src_main, "src/main.c");
    SOURCE(src_util, "src/util.c");
    SOURCE(src_parser, "src/parser.c");

    // Object file targets
    TARGET(main_obj, "main.o", DEPS(&src_main, &config_h),
           CMD(CC, "-Iinclude", "-c", "src/main.c", "-o", "main.o"));
    
    TARGET(util_obj, "util.o", DEPS(&src_util, &config_h),
           CMD(CC, "-Iinclude", "-c", "src/util.c", "-o", "util.o"));
    
    TARGET(parser_obj, "parser.o", DEPS(&src_parser, &config_h),
           CMD(CC, "-Iinclude", "-c", "src/parser.c", "-o", "parser.o"));

    // Link executable
    TARGET(program, "program", DEPS(&main_obj, &util_obj, &parser_obj),
           CMD(LINKER, "main.o", "util.o", "parser.o", "-o", "program", "-lpthread"));

    build_target(program);

This approach provides explicit control over compilation flags per file.

## Features

- Parallel builds using available CPU cores
- Incremental rebuilds based on modification time
- Automatic dependency resolution
- Self-rebuilding when build.c or build.config.h changes
- No external dependencies beyond POSIX and pthread

## License

See LICENSE file for details.

