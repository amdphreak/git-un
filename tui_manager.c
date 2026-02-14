/*
 * TUI Manager for gitu
 * Interactive path management with visual reordering
 */

#include "git-compat-util.h"
#include "strbuf.h"
#include "run-command.h"
#include "shim_manager.h"

#ifdef HAVE_CURSES
#include <curses.h>
#include <menu.h>

struct tui_state {
	struct git_location *git_list;
	int selected_index;
	int count;
	WINDOW *main_win;
	MENU *menu;
	ITEM **items;
};

static void cleanup_tui(struct tui_state *state)
{
	if (state->menu) {
		unpost_menu(state->menu);
		free_menu(state->menu);
	}
	if (state->items) {
		int i;
		for (i = 0; i < state->count; i++) {
			free_item(state->items[i]);
		}
		free(state->items);
	}
	if (state->main_win) {
		delwin(state->main_win);
	}
	endwin();
}

static int init_tui(struct tui_state *state, struct git_location *git_list)
{
	int i, count = 0;
	struct git_location *current;
	
	/* Count git locations */
	for (current = git_list; current; current = current->next)
		count++;
	
	state->git_list = git_list;
	state->count = count;
	state->selected_index = 0;
	
	/* Initialize curses */
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);
	
	/* Create items for menu */
	state->items = xcalloc(count + 1, sizeof(ITEM *));
	current = git_list;
	for (i = 0; i < count; i++) {
		struct strbuf item_text = STRBUF_INIT;
		strbuf_addf(&item_text, "%s (priority: %d)", current->path, current->priority);
		state->items[i] = new_item(item_text.buf, NULL);
		current = current->next;
		strbuf_release(&item_text);
	}
	state->items[count] = NULL;
	
	/* Create menu */
	state->menu = new_menu(state->items);
	
	/* Create main window */
	int height, width;
	getmaxyx(stdscr, height, width);
	state->main_win = newwin(height - 4, width - 4, 2, 2);
	keypad(state->main_win, TRUE);
	
	/* Set menu window and subwindow */
	set_menu_win(state->menu, state->main_win);
	set_menu_sub(state->menu, derwin(state->main_win, height - 8, width - 8, 3, 2));
	set_menu_format(state->menu, height - 8, 1);
	
	/* Post menu */
	post_menu(state->menu);
	
	/* Draw borders and title */
	box(state->main_win, 0, 0);
	mvwprintw(state->main_win, 1, 2, "Git Installation Priority Manager");
	mvwprintw(state->main_win, height - 5, 2, "Arrow keys: Navigate | Enter: Select | q: Quit");
	
	wrefresh(state->main_win);
	
	return 0;
}

static void move_item_up(struct tui_state *state)
{
	if (state->selected_index > 0) {
		/* Swap items in the linked list */
		struct git_location *prev = NULL, *curr = state->git_list;
		struct git_location *target = NULL, *target_prev = NULL;
		int i = 0;
		
		/* Find the items to swap */
		while (curr && i < state->selected_index) {
			target_prev = prev;
			target = curr;
			prev = curr;
			curr = curr->next;
			i++;
		}
		
		if (target && target_prev && curr) {
			/* Perform the swap */
			if (target_prev)
				target_prev->next = curr;
			else
				state->git_list = curr;
			
			target->next = curr->next;
			curr->next = target;
			
			state->selected_index--;
		}
	}
}

static void move_item_down(struct tui_state *state)
{
	struct git_location *prev = NULL, *curr = state->git_list;
	int i = 0;
	
	/* Find current and next items */
	while (curr && i < state->selected_index) {
		prev = curr;
		curr = curr->next;
		i++;
	}
	
	if (curr && curr->next) {
		struct git_location *next = curr->next;
		
		/* Perform the swap */
		if (prev)
			prev->next = next;
		else
			state->git_list = next;
		
		curr->next = next->next;
		next->next = curr;
		
		state->selected_index++;
	}
}

static struct git_location *run_tui_selection(struct git_location *git_list)
{
	struct tui_state state;
	int c;
	struct git_location *selected = NULL;
	
	if (init_tui(&state, git_list) != 0) {
		return NULL;
	}
	
	while ((c = wgetch(state.main_win)) != 'q') {
		switch (c) {
		case KEY_UP:
			menu_driver(state.menu, REQ_UP_ITEM);
			state.selected_index--;
			if (state.selected_index < 0) state.selected_index = 0;
			break;
		case KEY_DOWN:
			menu_driver(state.menu, REQ_DOWN_ITEM);
			state.selected_index++;
			if (state.selected_index >= state.count) 
				state.selected_index = state.count - 1;
			break;
		case KEY_PPAGE:
			menu_driver(state.menu, REQ_SCR_UPAGE);
			state.selected_index -= 5;
			if (state.selected_index < 0) state.selected_index = 0;
			break;
		case KEY_NPAGE:
			menu_driver(state.menu, REQ_SCR_DPAGE);
			state.selected_index += 5;
			if (state.selected_index >= state.count)
				state.selected_index = state.count - 1;
			break;
		case KEY_UP | 0x1000: /* Ctrl+Up */
			move_item_up(&state);
			break;
		case KEY_DOWN | 0x1000: /* Ctrl+Down */
			move_item_down(&state);
			break;
		case 10: /* Enter */
		case KEY_ENTER:
			{
				int i;
				struct git_location *current = state.git_list;
				for (i = 0; i < state.selected_index && current; i++) {
					current = current->next;
				}
				selected = current;
				goto done;
			}
		}
		wrefresh(state.main_win);
	}
	
done:
	cleanup_tui(&state);
	return selected;
}

#endif /* HAVE_CURSES */

/* Non-TUI AI-friendly interface */
struct git_location *run_ai_selection(struct git_location *git_list)
{
	struct git_location *current;
	int index = 1;
	char input[32];
	int choice;
	
	printf("\nAI-Friendly Git Selection Interface:\n");
	printf("===================================\n");
	
	for (current = git_list; current; current = current->next) {
		printf("%d. %s (priority: %d)\n", index++, current->path, current->priority);
	}
	
	printf("\nEnter selection (1-%d): ", index - 1);
	if (fgets(input, sizeof(input), stdin)) {
		choice = atoi(input);
		if (choice >= 1 && choice < index) {
			current = git_list;
			for (index = 1; index < choice && current; index++) {
				current = current->next;
			}
			return current;
		}
	}
	
	printf("Invalid selection. Using highest priority Git.\n");
	return git_list;
}

int cmd_shim_interactive(int argc, const char **argv, const char *prefix)
{
	struct git_location *git_list;
	struct git_location *selected = NULL;
	
	printf("Interactive shim registration...\n");
	
	git_list = find_git_executables();
	if (!git_list) {
		printf("No Git installations found.\n");
		return 1;
	}
	
#ifdef HAVE_CURSES
	printf("Starting TUI interface...\n");
	selected = run_tui_selection(git_list);
#else
	printf("TUI not available. Using AI-friendly interface...\n");
	selected = run_ai_selection(git_list);
#endif
	
	if (selected) {
		printf("\nSelected: %s\n", selected->path);
		
		/* Create shim */
		struct strbuf shim_dir = STRBUF_INIT;
		struct strbuf shim_path = STRBUF_INIT;
		
		const char *home = getenv("HOME");
		if (home) {
			strbuf_addf(&shim_dir, "%s/.local/bin", home);
		} else {
			strbuf_addstr(&shim_dir, ".");
		}
		
		mkdir(shim_dir.buf, 0755);
		strbuf_addf(&shim_path, "%s/git", shim_dir.buf);
		
		if (create_shim_script(shim_path.buf, selected->path) == 0) {
			printf("Shim created at: %s\n", shim_path.buf);
			printf("Add '%s' to your PATH to use it.\n", shim_dir.buf);
		}
		
		strbuf_release(&shim_dir);
		strbuf_release(&shim_path);
	}
	
	free_git_locations(git_list);
	return 0;
}
