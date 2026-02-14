#ifndef SHIM_MANAGER_H
#define SHIM_MANAGER_H

struct git_location;

/* Shim management functions */
int cmd_shim_register(int argc, const char **argv, const char *prefix);
int cmd_shim_unregister(int argc, const char **argv, const char *prefix);
int cmd_shim_interactive(int argc, const char **argv, const char *prefix);

/* Utility functions */
struct git_location *find_git_executables(void);
void free_git_locations(struct git_location *list);
void display_git_locations(struct git_location *list);
int create_shim_script(const char *shim_path, const char *target_git);

/* AI-friendly selection */
struct git_location *run_ai_selection(struct git_location *git_list);

#endif /* SHIM_MANAGER_H */
