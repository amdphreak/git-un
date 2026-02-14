/*
 * gitu - Git Unfuckified CLI
 * A more intuitive interface to Git commands
 */

#include "git-compat-util.h"
#include "strbuf.h"
#include "run-command.h"
#include "parse-options.h"
#include "builtin.h"
#include "cache.h"
#include "shim_manager.h"

static const char * const gitu_usage[] = {
	N_("gitu <command> [options]"),
	NULL
};

/* Command mapping structure */
struct gitu_command {
	const char *name;
	const char *git_equiv;
	const char *help;
};

/* Mapping of gitu commands to git equivalents */
static const struct gitu_command gitu_commands[] = {
	{ "save", "commit", "Save changes with a message" },
	{ "stage", "add", "Stage files for commit" },
	{ "unstage", "reset HEAD --", "Unstage files" },
	{ "discard", "checkout --", "Discard changes to files" },
	{ "history", "log --oneline", "Show commit history" },
	{ "branch", "branch", "Manage branches" },
	{ "branch create", "checkout -b", "Create and switch to new branch" },
	{ "branch switch", "checkout", "Switch to existing branch" },
	{ "upload", "push", "Upload changes to remote" },
	{ "download", "pull", "Download changes from remote" },
	{ "sync", "pull && push", "Synchronize with remote" },
	{ "status", "status", "Show repository status" },
	{ "init", "init", "Initialize repository" },
	{ "clone", "clone", "Clone repository" },
	{ "merge", "merge", "Merge branches" },
	{ "undo", "reset --soft HEAD~1", "Undo last commit" },
	{ NULL, NULL, NULL }
};

/* Find git command equivalent */
static const struct gitu_command *find_gitu_command(const char *cmd)
{
	int i;
	for (i = 0; gitu_commands[i].name; i++) {
		if (!strcmp(cmd, gitu_commands[i].name))
			return &gitu_commands[i];
	}
	return NULL;
}

/* Show help for gitu commands */
static void show_gitu_help(void)
{
	int i;
	printf("gitu - Git Unfuckified CLI\n\n");
	printf("Available commands:\n");
	for (i = 0; gitu_commands[i].name; i++) {
		printf("  %-20s %s\n", gitu_commands[i].name, gitu_commands[i].help);
	}
	printf("\nFor more help, use: gitu <command> --help\n");
}

/* Shim management commands */
static int cmd_shim_register(int argc, const char **argv, const char *prefix)
{
	/* This function is now in shim_manager.c */
	return cmd_shim_register(argc, argv, prefix);
}

static int cmd_shim_unregister(int argc, const char **argv, const char *prefix)
{
	/* This function is now in shim_manager.c */
	return cmd_shim_unregister(argc, argv, prefix);
}

/* Main gitu command dispatcher */
int cmd_main(int argc, const char **argv, const char *prefix)
{
	const char *cmd;
	const struct gitu_command *gitu_cmd;
	struct child_process child = CHILD_PROCESS_INIT;
	int i;
	
	/* No arguments - show help */
	if (argc < 2) {
		show_gitu_help();
		return 0;
	}
	
	cmd = argv[1];
	
	/* Handle shim management */
	if (!strcmp(cmd, "shim")) {
		if (argc < 3) {
			printf("Usage: gitu shim <register|unregister|interactive>\n");
			return 1;
		}
		if (!strcmp(argv[2], "register"))
			return cmd_shim_register(argc, argv, prefix);
		else if (!strcmp(argv[2], "unregister"))
			return cmd_shim_unregister(argc, argv, prefix);
		else if (!strcmp(argv[2], "interactive"))
			return cmd_shim_interactive(argc, argv, prefix);
		else {
			printf("Unknown shim command: %s\n", argv[2]);
			return 1;
		}
	}
	
	/* Handle help command */
	if (!strcmp(cmd, "help") || !strcmp(cmd, "--help") || !strcmp(cmd, "-h")) {
		show_gitu_help();
		return 0;
	}
	
	/* Find the gitu command */
	gitu_cmd = find_gitu_command(cmd);
	if (!gitu_cmd) {
		printf("Unknown gitu command: %s\n", cmd);
		show_gitu_help();
		return 1;
	}
	
	/* Build and execute the git command */
	strvec_push(&child.args, "git");
	
	/* Handle commands with spaces (like "branch create") */
	if (strstr(cmd, " ")) {
		char *cmd_copy = xstrdup(cmd);
		char *space = strchr(cmd_copy, ' ');
		*space = '\0';
		strvec_push(&child.args, cmd_copy);
		strvec_push(&child.args, space + 1);
		
		/* Add remaining arguments */
		for (i = 2; i < argc; i++)
			strvec_push(&child.args, argv[i]);
		
		free(cmd_copy);
	} else {
		/* Simple command */
		strvec_push(&child.args, gitu_cmd->git_equiv);
		
		/* Add remaining arguments */
		for (i = 2; i < argc; i++)
			strvec_push(&child.args, argv[i]);
	}
	
	/* Execute the git command */
	child.use_shell = 0;
	child.git_cmd = 1;
	
	return run_command(&child);
}
