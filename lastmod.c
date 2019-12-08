#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include <dirent.h>
#include <sys/types.h>

#include <limits.h>

struct FileEntry
{
	// Can either be the file path or just name, depending on configuration
	char name[FILENAME_MAX];
	time_t last_mod;
	short hidden;
};

struct Config
{
	short show_hidden;
	short show_path;
	short show_raw_UTC;
	short show_gmtime;
	size_t show_amount;
};

struct Config g_config;

struct FileEntry* g_files_buf;

time_t get_file_mod_time(char* path)
{
	struct stat attr;
	stat(path, &attr);
	return attr.st_mtime;
}

int is_regular_file(const char* path)
{
	struct stat path_stat;
	stat(path, &path_stat);
	return S_ISREG(path_stat.st_mode);
}

int is_dir(const char* path)
{
	struct stat path_stat;
	stat(path, &path_stat);
	return S_ISDIR(path_stat.st_mode);
}

void iterate_dirs(const char* dir)
{
	DIR* dp = opendir(dir);

	struct dirent* ep;
	if (dp != NULL)
	{
		while (ep = readdir(dp))
		{
			// puts(ep->d_name);

			char full_path[PATH_MAX];
			int hidden = 0;
			// Detects hidden dot files and directories
			if (ep->d_name[0] == '.')
				hidden = 1;

			if (!g_config.show_hidden && hidden)
				continue;

			if (dir[strlen(dir) - 1] != '/')
				snprintf(full_path, sizeof full_path, "%s/%s", dir, ep->d_name);
			else
				snprintf(full_path, sizeof full_path, "%s%s", dir, ep->d_name);

			if (is_dir(full_path) && strcmp(ep->d_name, "..") && strcmp(ep->d_name, "."))
			{
				iterate_dirs(full_path);
				continue;
			}

			if (!is_regular_file(full_path))
				continue;
			// Fill in a FileEntry
			struct FileEntry file;
			strcpy(file.name, g_config.show_path ? full_path : ep->d_name);
			file.last_mod = get_file_mod_time(full_path);
			file.hidden = hidden;

			short success = 0;

			for (size_t i = 0; i < g_config.show_amount; i++)
			{

				// Current file was modified earlier
				if (file.last_mod > g_files_buf[i].last_mod)
				{

					success = 1;
					// Shift down to insert
					memcpy(g_files_buf + i + 1, g_files_buf + i,
						   (g_config.show_amount - i - 1) * (size_t)sizeof(struct FileEntry));
					memcpy(g_files_buf + i, &file, sizeof(struct FileEntry));
					break;
				}
			}
		}
		(void)closedir(dp);
	}
	else
		perror("Couldn't open the directory");
}

int main(int argc, char** argv)
{

	char* dir = "./";

	// Set all config to their default values
	g_config.show_hidden = 0;
	g_config.show_path = 0;
	g_config.show_raw_UTC = 0;
	g_config.show_gmtime = 0;
	g_config.show_amount = 10;

	// If second argument is given, start program from there
	if (argc > 1)
	{
		for (size_t i = 1; i < argc; i++)
		{
			// Checks if argument is a flag/option or a path
			// If it's a path, it assigns it to dir and skips
			if (argv[i][0] != '-')
			{
				dir = argv[i];
				continue;
			}

			// Stores how long the option is. If above 2, several flags/options are combined, E.G; -an 10
			size_t cmb_options = strlen(argv[i]);

			for (size_t j = 1; j < cmb_options; j++)
			{
				if (argv[i][j] == 'a')
					g_config.show_hidden = 1;

				if (argv[i][j] == 'l')
					g_config.show_path = 1;

				if (argv[i][j] == 'u')
					g_config.show_raw_UTC = 1;
				if (argv[i][j] == 'g')
					g_config.show_gmtime = 1;

				if (argv[i][j] == 'n' && i < argc - 1)
				{
					g_config.show_amount = atoi(argv[i + 1]);
					i++;
				}
			}
		}
	}

	g_files_buf = calloc(g_config.show_amount, sizeof(struct FileEntry));

	iterate_dirs(dir);

	size_t longest_name = 0;

	for (size_t i = 0; i < g_config.show_amount; i++)
	{
		if (strlen(g_files_buf[i].name) > longest_name)
			longest_name = strlen(g_files_buf[i].name);
	}
	// Print result
	for (size_t i = 0; i < g_config.show_amount; i++)
	{
		// Quit when unfilled slot is met
		if (!strcmp(g_files_buf[i].name, ""))
			break;

		char color[15];
		strcpy(color, g_files_buf[i].hidden ? "\x001b[34;1m" : "\x001b[0m");

		// Use space padding so the dates all line up
		char padding[FILENAME_MAX];
		size_t pad_length = longest_name - strlen(g_files_buf[i].name);
		memset(padding, ' ', pad_length);
		// Null terminate padding string
		padding[pad_length] = '\0';

		// Generate data and time string according to the options
		char date[63];
		if (g_config.show_raw_UTC)
		{
			snprintf(date, 63, "%ld", g_files_buf[i].last_mod);
		}
		else
		{
			struct tm* timeinfo;

			struct tm* (*gentime)(time_t*) = g_config.show_gmtime ? gmtime : localtime;
			timeinfo = gentime(&g_files_buf[i].last_mod);

			strftime(date, sizeof date, "%d %b %H:%M", timeinfo);
		}
		printf("%s%s%s | %s\x001b[0m\n", color, g_files_buf[i].name, padding, date);
	}

	return 0;
}