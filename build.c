#include <errno.h>
#include <linux/limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define CC "gcc"
#define CFLAGS "-Wall -Werror -ggdb"
#define LIBS ""

// Functions
time_t get_file_mtime(const char *path);
void go_build_urself(int argc, char **argv);

typedef struct Cmd Cmd;
struct Cmd {
  const char **args;
  size_t count;
  size_t capacity;
};

void cmd_internal_append(Cmd *cmd, ...);
#define cmd_append(CMD, ...) cmd_internal_append(CMD, __VA_ARGS__, NULL);
void cmd_remove(Cmd *cmd, int32_t idx);
void cmd_print(const Cmd *cmd);
int cmd_run(Cmd *cmd);

typedef struct Target Target;
struct Target {
  const char *output;
  Target **deps;
  Cmd *cmd;
};
void build_target(Target *t);

#define DEPS(...)                                                              \
  (Target *[]) { __VA_ARGS__, NULL }

#define CMD(...)                                                               \
  &(Cmd){.args = (const char *[]){__VA_ARGS__, NULL},                          \
         .count =                                                              \
             (sizeof((const char *[]){__VA_ARGS__}) / sizeof(const char *)),   \
         .capacity = 0}

#define TARGET(name, out_file, deps_arr, cmd_ptr)                              \
  Target name = {.output = (out_file), .deps = (deps_arr), .cmd = (cmd_ptr)}

#define SOURCE(name, out_file)                                                 \
  Target name = {.output = (out_file), .deps = NULL, .cmd = NULL}

int main(int argc, char **argv) {
  go_build_urself(argc, argv);

  SOURCE(src_main, "main.c");

  TARGET(main_obj, "main.o", DEPS(&src_main),
         CMD(CC, "main.c", "-c", "main.o"));
  TARGET(main, "main2", DEPS(&src_main, &main_obj),
         CMD(CC, "main.o", "-o", "main2"));

  build_target(&main);
  return 0;
}

#define panic(msg, ...)                                                        \
  do {                                                                         \
    fprintf(stderr, "Panic at %s:%d: \n>>> " msg "\n", __FILE__, __LINE__,     \
            ##__VA_ARGS__);                                                    \
    exit(1);                                                                   \
  } while (0)

#define todo() panic("not yet implemented")

void cmd_internal_append(Cmd *cmd, ...) {
  if (cmd == NULL)
    panic("cmd is a null pointer");
  const char *str;
  va_list args;

  va_start(args, cmd);

  const char *cur_arg = va_arg(args, const char *);

  while (cur_arg != NULL) {
    // first check if we need to realloc the dynamic array

    if (cmd->count == cmd->capacity) {
      size_t newsize = (cmd->capacity == 0) ? 10 : cmd->capacity * 2;
      const char **new_items =
          (const char **)realloc(cmd->args, newsize * sizeof(char *));

      if (!new_items) {
        panic("Your garbage ran out of ram holy shit");
      }

      cmd->args = new_items;
      cmd->capacity++;
    }

    cmd->args[cmd->count++] = cur_arg;

    // go over the next arg

    cur_arg = va_arg(args, const char *);
  }
}

#include <stdio.h>
#include <string.h>
void cmd_print(const Cmd *cmd) {
  if (cmd->count == 0)
    return;

  for (size_t i = 0; i < cmd->count; ++i) {
    if (strchr(cmd->args[i], ' ')) {
      printf("'%s'", cmd->args[i]);
    } else {
      printf("%s", cmd->args[i]);
    }

    // Print space between args, newline at the end
    if (i + 1 < cmd->count)
      printf(" ");
  }
  printf("\n");
}

int cmd_run(Cmd *cmd) {
  if (cmd->count == 0) {
    return 1;
  }

  printf("[CMD] ");
  cmd_print(cmd);

  pid_t pid = fork();
  if (pid < 0) {
    panic("fork failed");
  }

  if (pid == 0) {
    // child process
    execvp(cmd->args[0], (char *const *)cmd->args);

    // fail if somehow this shit itself
    panic("Failed to execve");
  }

  int status;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status)) {
    int exit_code = WEXITSTATUS(status);
    if (exit_code != 0) {
      fprintf(stderr, "Command failed with exit code %d\n", exit_code);
      return 0;
    }
    return 1; // Success
  }

  if (WIFSIGNALED(status)) {
    panic("Build terminated with signal %d\n", WTERMSIG(status));
  }

  return 0;
}
void go_build_urself(int argc, char **argv) {
  const char *source_path = __FILE__;
  const char *binary_path = argv[0];

  if (get_file_mtime(source_path) <= get_file_mtime(binary_path))
    return;

  printf("[INFO] rebuilding");

  char old_binary_path[1024];
  snprintf(old_binary_path, sizeof(old_binary_path), "%s.old", binary_path);

#ifdef _WIN32
  // Windows 'rename' fails if target exists
  // We don't use 'unlink' because it might be locked if it was a previous run
  if (get_file_mtime(old_binary_path) != 0) {
    remove(old_binary_path);
  }
#endif

  if (rename(binary_path, old_binary_path) != 0) {
    panic("Couldnt rename old binary");
  }

  // We rebuild
  Cmd cmd = {0};
  cmd_append(&cmd, CC, source_path, "-o", binary_path);

  if (!cmd_run(&cmd)) {
    rename(old_binary_path, binary_path);
  }
  unlink(old_binary_path);

  printf("[INFO] Restarting %s...\n", binary_path);

  execvp(binary_path, argv);
  panic("How did we get here?");
}

time_t get_file_mtime(const char *path) {
  struct stat attr;
  if (stat(path, &attr) < 0)
    return 0;
  return attr.st_mtime;
}

void build_target(Target *t) {
  int needs_rebuild = 0;
  time_t mtime = get_file_mtime(t->output);

  if (mtime == 0) {
    needs_rebuild = 1;
  }

  // check if build.c got changed
  //
  if (t->cmd && get_file_mtime(__FILE__) > mtime) {
    needs_rebuild = 1;
  }
  if (t->deps) {
    for (int i = 0; t->deps[i] != NULL; i++) {
      Target *dep = t->deps[i];
      build_target(dep);

      if (get_file_mtime(dep->output) > mtime) {
        needs_rebuild = 1;
      }
    }
  }

  if (needs_rebuild) {
    if (!t->cmd) {
      // if mtime is 0 this is not a leaf node

      if (mtime == 0)
        panic("No command provided to build %s", t->output);

      return;
    }

    if (!cmd_run(t->cmd))
      panic("Failed to build %s\n", t->output);
  } else {
    if (t->cmd)
      printf("%s Up to date!!!\n", t->output);
  }
}
