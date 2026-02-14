/*
 * Shim Manager for gitu
 * Handles PATH management and git replacement shim
 */

#include "git-compat-util.h"
#include "strbuf.h"
#include "run-command.h"

struct git_location {
	char *path;
	int priority;
	struct git_location *next;
};

static struct git_location *find_git_executables(void)
{
	struct git_location *list = NULL;
	struct strbuf path_env = STRBUF_INIT;
	struct strbuf current_path = STRBUF_INIT;
	char *path_copy, *path_token, *save_ptr = NULL;
	const char *path_entries[] = {
		"/usr/bin/git",
		"/usr/local/bin/git",
		"/opt/homebrew/bin/git",
		"C:\\Program Files\\Git\\bin\\git.exe",
		"C:\\Program Files (x86)\\Git\\bin\\git.exe",
		NULL
	};
	int i;
	
	/* Get PATH environment variable */
	const char *path_env_val = getenv("PATH");
	if (!path_env_val) {
		printf("Warning: PATH environment variable not found\n");
		return NULL;
	}
	
	strbuf_addstr(&path_env, path_env_val);
	path_copy = strbuf_detach(&path_env, NULL);
	
	/* Parse PATH entries */
	for (path_token = strtok_r(path_copy, PATH_SEP, &save_ptr);
	     path_token;
	     path_token = strtok_r(NULL, PATH_SEP, &save_ptr)) {
		
		struct strbuf git_path = STRBUF_INIT;
		
		/* Check for git in this PATH entry */
		strbuf_addf(&git_path, "%s%s", path_token, strchr(path_token, '/') ? "/git" : "\\git.exe");
		
		if (access(git_path.buf, X_OK) == 0) {
			struct git_location *loc = xcalloc(1, sizeof(struct git_location));
			loc->path = xstrdup(git_path.buf);
			loc->priority = 0; /* Will be set based on PATH order */
			loc->next = list;
			list = loc;
		}
		
		strbuf_release(&git_path);
	}
	
	/* Also check common installation paths */
	for (i = 0; path_entries[i]; i++) {
		if (access(path_entries[i], X_OK) == 0) {
			struct git_location *loc = xcalloc(1, sizeof(struct git_location));
			loc->path = xstrdup(path_entries[i]);
			loc->priority = -1; /* Special priority for system paths */
			loc->next = list;
			list = loc;
		}
	}
	
	free(path_copy);
	strbuf_release(&current_path);
	
	/* Set priorities based on PATH order */
	{
		int priority = 1000; /* Start high and decrement */
		struct git_location *current = list;
		while (current) {
			if (current->priority == 0) {
				current->priority = priority--;
			}
			current = current->next;
		}
	}
	
	return list;
}

static void free_git_locations(struct git_location *list)
{
	while (list) {
		struct git_location *next = list->next;
		free(list->path);
		free(list);
		list = next;
	}
}

static void display_git_locations(struct git_location *list)
{
	int index = 1;
	printf("\nFound Git installations:\n");
	printf("========================\n");
	
	while (list) {
		printf("%d. %s (priority: %d)\n", index++, list->path, list->priority);
		list = list->next;
	}
}

static int create_shim_script(const char *shim_path, const char *target_git)
{
	FILE *f;
	
#ifdef GIT_WINDOWS_NATIVE
	/* Windows batch file */
	strbuf_addstr(&shim_path, ".bat");
	f = fopen(shim_path, "w");
	if (!f) {
		perror("Failed to create shim script");
		return -1;
	}
	
	fprintf(f, "@echo off\n");
	fprintf(f, "REM gitu shim for git\n");
	fprintf(f, "\"%s\" %%*\n", target_git);
#else
	/* Unix shell script */
	f = fopen(shim_path, "w");
	if (!f) {
		perror("Failed to create shim script");
		return -1;
	}
	
	fprintf(f, "#!/bin/sh\n");
	fprintf(f, "# gitu shim for git\n");
	fprintf(f, "exec \"%s\" \"$@\"\n", target_git);
	fchmod(fileno(f), 0755);
#endif
	
	fclose(f);
	return 0;
}

int cmd_shim_register(int argc, const char **argv, const char *prefix)
{
	struct git_location *git_list;
	struct git_location *selected = NULL;
	int choice = 0;
	
	printf("Registering gitu shim for git replacement...\n");
	
	git_list = find_git_executables();
	if (!git_list) {
		printf("No Git installations found.\n");
		return 1;
	}
	
	display_git_locations(git_list);
	
	/* For now, just select the highest priority git */
	/* In a full implementation, this would have TUI selection */
	selected = git_list;
	while (selected && selected->next) {
		if (selected->next->priority > selected->priority)
			selected = selected->next;
	}
	
	if (selected) {
		printf("\nSelected: %s\n", selected->path);
		
		/* Create shim in a directory that should be early in PATH */
		struct strbuf shim_dir = STRBUF_INIT;
		struct strbuf shim_path = STRBUF_INIT;
		
		/* Try to create in user's local bin directory */
		const char *home = getenv("HOME");
		if (home) {
			strbuf_addf(&shim_dir, "%s/.local/bin", home);
		} else {
			/* Fallback to current directory */
			strbuf_addstr(&shim_dir, ".");
		}
		
		/* Create directory if it doesn't exist */
		mkdir(shim_dir.buf, 0755);
		
		strbuf_addf(&shim_path, "%s/git", shim_dir.buf);
		
		if (create_shim_script(shim_path.buf, selected->path) == 0) {
			printf("Shim created at: %s\n", shim_path.buf);
			printf("\nTo use the shim:\n");
			printf("1. Add '%s' to the beginning of your PATH\n", shim_dir.buf);
			printf("2. Or run: export PATH=\"%s:$PATH\"\n", shim_dir.buf);
		} else {
			printf("Failed to create shim.\n");
			free_git_locations(git_list);
			return 1;
		}
		
		strbuf_release(&shim_dir);
		strbuf_release(&shim_path);
	}
	
	free_git_locations(git_list);
	return 0;
}

int cmd_shim_unregister(int argc, const char **argv, const char *prefix)
{
	const char *home = getenv("HOME");
	struct strbuf shim_path = STRBUF_INIT;
	
	printf("Unregistering gitu shim...\n");
	
	if (home) {
		strbuf_addf(&shim_path, "%s/.local/bin/git", home);
	} else {
		strbuf_addstr(&shim_path, "./git");
	}
	
	if (unlink(shim_path.buf) == 0) {
		printf("Shim removed: %s\n", shim_path.buf);
		printf("Remember to remove the shim directory from your PATH if you added it.\n");
	} else {
		printf("No shim found at: %s\n", shim_path.buf);
	}
	
	strbuf_release(&shim_path);
	return 0;
}
