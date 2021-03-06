/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GMAMEUI
 *
 * Copyright 2007-2008 Andrew Burton <adb@iinet.net.au>
 * based on GXMame code
 * 2002-2005 Stephane Pontier <shadow_walker@users.sourceforge.net>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gtk/gtkmain.h>
#include <gtk/gtkscrolledwindow.h>

#include "gmameui.h"
#include "io.h"
#include "gui.h"

#ifndef XMAME_ROOT
#define XMAME_ROOT "/usr/lib/games/xmame"
#endif

/*
converts a float to a string with precission 5.
this is locale independent.
buf must be of size G_ASCII_DTOSTR_BUF_SIZE
*/
const gchar *
my_dtostr (char * buf, gdouble d)
{
	char *p;
	g_ascii_dtostr (buf, FLOAT_BUF_SIZE, d);

	for (p = buf; *p && *p != '.'; p++); /* point p to the decimal point */

	if (*p)
	{
		p += 6;	/* precision + 1 */
		*p = '\0';
	}
	return buf;
}

#define LINE_BUF 512

/* Custom function to save the keyfile to disk. If the file does not exist,
   it will be created */
gboolean g_key_file_save_to_file(GKeyFile *kf, const gchar *file, GError **error)
{
	gsize length, nr;
	char *data = g_key_file_to_data(kf, &length, error);
	GIOChannel *gio;

	if (!data)
		return FALSE;

	gio = g_io_channel_new_file(file, "w+", error);
	if (!gio) {
		g_free(data);
		return FALSE;
	}
	
	g_io_channel_write_chars(gio, data, length, &nr, error);

	g_free(data);

	g_io_channel_unref(gio);

	return TRUE;
}

gboolean
load_games_ini (void)
{
	gchar *filename;
	GList *listpointer;
	RomEntry *tmprom = NULL;
	gboolean game_found = FALSE;
	
	GMAMEUI_DEBUG ("Loading games.ini");

	filename = g_build_filename (g_get_home_dir (), ".gmameui", "games.ini", NULL);
	if (!filename)
		return FALSE;

	GKeyFile *gameini_list = g_key_file_new ();
	GError *error = NULL;
	gboolean result = g_key_file_load_from_file (gameini_list, filename, G_KEY_FILE_KEEP_COMMENTS, &error);
	g_free (filename);
	if (!result) {
		GMAMEUI_DEBUG ("Error loading games.ini file: %s", error->message);
		g_error_free (error);
		g_key_file_free (gameini_list);

		game_list.not_checked_list = g_list_copy (game_list.roms);

		return FALSE;
	}

	/* ADB FIXME Run through the list of available roms, and look for
	the key values; if the first cannot be found, assume the rest cannot be either */
	gchar **gamelist = g_key_file_get_groups (gameini_list, NULL);
	int i;
	for (i = 0; gamelist[i] != NULL; i++) {
		tmprom = get_rom_from_gamelist_by_name (gamelist[i]);
		if (tmprom) {
			tmprom->timesplayed = g_key_file_get_integer (gameini_list, gamelist[i], "PlayCount", &error);
			if (!tmprom->timesplayed) tmprom->timesplayed = 0;	/* Set default */
			tmprom->has_roms = g_key_file_get_integer (gameini_list, gamelist[i], "HasRoms", &error);
			tmprom->has_samples = g_key_file_get_integer (gameini_list, gamelist[i], "HasSamples", &error);
			tmprom->favourite = g_key_file_get_integer (gameini_list, gamelist[i], "Favorite", &error);
			if (!tmprom->favourite) tmprom->favourite = 0;	/* Set default */
			/* TODO AutofireDelay */

			if ((tmprom->has_roms == UNKNOWN) ||
			    (tmprom->has_roms == NOT_AVAIL) ||
			    ((tmprom->has_roms == INCORRECT) && gui_prefs.GameCheck))
			{
				game_list.not_checked_list = g_list_append (game_list.not_checked_list, tmprom);
			}
		}
	}

	g_strfreev (gamelist);
	g_key_file_free (gameini_list);

	return TRUE;
}

/* FIXME Only update a game if it has changed */
gboolean
save_games_ini (void)
{
	FILE *game_ini_file;
	gchar *filename;
	RomEntry *rom;
	GList *list_pointer;
	
	
	GMAMEUI_DEBUG ("Saving games.ini");

	filename = g_build_filename (g_get_home_dir (), ".gmameui", "games.ini", NULL);
	game_ini_file = fopen (filename, "w");
	g_free (filename);
	if (!game_ini_file)
	{
		GMAMEUI_DEBUG ("unable to write games.ini");
		return FALSE;
	}
		
	for (list_pointer = g_list_first (game_list.roms);
	     (list_pointer != NULL);
	     list_pointer=g_list_next (list_pointer))
	{
		rom = (RomEntry *)list_pointer->data;
		fprintf (game_ini_file, "[%s]\n", rom->romname);
		fprintf (game_ini_file, "PlayCount=%i\n", rom->timesplayed);
		fprintf (game_ini_file, "HasRoms=%i\n", rom->has_roms);
		fprintf (game_ini_file, "HasSamples=%i\n", rom->has_samples);
		fprintf (game_ini_file, "Favorite=%s\n", rom->favourite ? "1" : "0");
		fputs ("\r\n", game_ini_file);
	}
	fclose (game_ini_file);
	
	return TRUE;
}


gboolean
load_catver_ini (void)
{
	gchar *filename;
	GList *listpointer;
	RomEntry *tmprom = NULL;
	gchar *category;
	gchar *version;
	GError *error = NULL;
	
	GMAMEUI_DEBUG ("Loading catver.ini");

	GTimer *timer = g_timer_new();
	g_timer_start (timer);
	
	/* Initialize categories */
	if (!game_list.categories) {
		g_list_foreach (game_list.categories, (GFunc) g_free, NULL);
		g_list_free (game_list.categories);
	}
	/* Initialize versions */
	if (!game_list.versions) {
		g_list_foreach (game_list.versions, (GFunc) g_free, NULL);
		g_list_free (game_list.versions);
	}

	category = g_strdup (_("Unknown"));
	game_list.categories = g_list_append (NULL, category);

	version = g_strdup (_("Unknown"));
	game_list.versions = g_list_append (NULL, version);

	/* Set all roms to unknown */
	for (listpointer = g_list_first (game_list.roms); (listpointer != NULL);
		listpointer = g_list_next (listpointer))
	{
		tmprom = (RomEntry *) listpointer->data;
		tmprom->category = category;
		tmprom->mame_ver_added = version;
	}

	filename = g_build_filename (gui_prefs.catverDirectory, "catver.ini", NULL);	
	if (!filename)
		return FALSE;

	GKeyFile *catver_file = g_key_file_new ();
	g_key_file_load_from_file (catver_file, filename, G_KEY_FILE_KEEP_COMMENTS, &error);
	if (error) {
		GMAMEUI_DEBUG ("catver.ini could not be loaded: %s", error->message);
		g_error_free (error);
		g_free (filename);
		return FALSE;
	}
	g_free (filename);
	
	/* For each game in the list of available roms, get the category and
	   version from the ini file, and add to the list of known categories/versions
	   for use in the custom filters */
	for (listpointer = g_list_first (game_list.roms); listpointer; listpointer = g_list_next (listpointer)) {
		tmprom = (RomEntry *)listpointer->data;
		/*GMAMEUI_DEBUG ("Parsing catver - %s", tmprom->romname);*/
		category = g_key_file_get_string (catver_file, "Category", tmprom->romname, &error);
		if (error) {
			/*GMAMEUI_DEBUG ("Error parsing catver - %s", error->message);*/
			error = NULL;
		}
		version = g_key_file_get_string (catver_file, "VerAdded", tmprom->romname, &error);
		if (error) {
			/*GMAMEUI_DEBUG ("Error parsing catver - %s", error->message);*/
			error = NULL;
		}
		tmprom->category = glist_insert_unique (&game_list.categories, category);
		tmprom->mame_ver_added = glist_insert_unique (&game_list.versions, version);

	}
	g_key_file_free (catver_file);
	g_free (category);
	g_free (version);
	
	GMAMEUI_DEBUG ("Catver loaded in %.2f seconds", g_timer_elapsed (timer, NULL));
	g_timer_stop (timer);
	g_timer_destroy (timer);

	return TRUE;
}

/* preferences for the gui */
gboolean
load_gmameui_ini (void)
{
	gchar *filename;
	gchar *default_game = NULL;
	gint i;
//	gsize sizes[2], columnsize[NUMBER_COLUMN];
	gsize sizes, columnsize;
	
	GMAMEUI_DEBUG ("Loading gmameui.ini");

	gui_prefs.FolderID = 1;
	gui_prefs.ShowScreenShot = TRUE;
	gui_prefs.ShowScreenShotTab = TRUE;
	gui_prefs.ShowFlyer = FALSE;
	gui_prefs.ShowToolBar = TRUE;
	gui_prefs.ShowStatusBar = TRUE;
	gui_prefs.ShowFolderList = TRUE;
	gui_prefs.SortColumn = GAMENAME;
	gui_prefs.SortReverse = FALSE;
	gui_prefs.GUIPosX = 0;
	gui_prefs.GUIPosY = 0;
	gui_prefs.GUIWidth = 640;
	gui_prefs.GUIHeight = 400;
	gui_prefs.previous_mode = 3;
	gui_prefs.current_mode = 3;
	gui_prefs.Splitters = g_new0 (gint, 2);
	gui_prefs.Splitters[0] = 150;
	gui_prefs.Splitters[1] = 800;
	gui_prefs.ModifyThe = TRUE;
	gui_prefs.GameCheck = FALSE;
	gui_prefs.VersionCheck = TRUE;
	gui_prefs.gui_joy = TRUE;
	gui_prefs.use_xmame_options = FALSE;
	gui_prefs.ColumnWidth = g_new0 (gint, NUMBER_COLUMN);
	gui_prefs.ColumnOrder = g_new0 (gint, NUMBER_COLUMN);
	gui_prefs.ColumnShown = g_new0 (gint, NUMBER_COLUMN);
	/* FIXME Have default value set only if they cannot be loaded below */
	for (i = 0; i < NUMBER_COLUMN; i++)
	{
		gui_prefs.ColumnWidth[i] = 0;  /* 0 --> Auto Size */
		gui_prefs.ColumnOrder[i] = i;
		gui_prefs.ColumnShown[i] = 0;
	}
	gui_prefs.ColumnShown[GAMENAME] = 1;
	gui_prefs.ColumnShown[HAS_SAMPLES] = 1;
	gui_prefs.ColumnShown[ROMNAME] = 1;
	gui_prefs.ColumnShown[VECTOR] = 1;
	gui_prefs.ColumnShown[CONTROL] = 1;
	gui_prefs.ColumnShown[TIMESPLAYED] = 1;
	gui_prefs.ColumnShown[MANU] = 1;
	gui_prefs.ColumnShown[YEAR] = 1;
	gui_prefs.ColumnShown[CLONE] = 1;
	gui_prefs.ColumnShown[DRIVER] = 1;
	gui_prefs.ColumnShown[MAMEVER] = 1;
	gui_prefs.ColumnShown[CATEGORY] = 1;

	filename = g_build_filename (g_get_home_dir (), ".gmameui", "gmameui.ini", NULL);

	GKeyFile *gmameui_ini_file = g_key_file_new ();
	GError *error = NULL;
	gboolean result = g_key_file_load_from_file (gmameui_ini_file, filename, G_KEY_FILE_KEEP_COMMENTS, &error);

	if (!result) {
		GMAMEUI_DEBUG ("Error loading gmameui.ini: %s\n", error->message);
		g_error_free (error);
		g_free (filename);
		return FALSE;
	}

	default_game = g_key_file_get_string (gmameui_ini_file, "Default", "DefaultGame", &error);
	gui_prefs.FolderID = g_key_file_get_integer (gmameui_ini_file, "Default", "FolderID", &error);
	gui_prefs.ShowScreenShot = g_key_file_get_integer (gmameui_ini_file, "Default", "ShowScreenShot", &error);
	gui_prefs.ShowScreenShotTab = g_key_file_get_integer (gmameui_ini_file, "Default", "ShowScreenShotTab", &error);
	gui_prefs.ShowFlyer = g_key_file_get_integer (gmameui_ini_file, "Default", "ShowFlyer", &error);
	gui_prefs.ShowToolBar = g_key_file_get_integer (gmameui_ini_file, "Default", "ShowToolBar", &error);
	gui_prefs.ShowStatusBar = g_key_file_get_integer (gmameui_ini_file, "Default", "ShowStatusBar", &error);
	gui_prefs.ModifyThe = g_key_file_get_integer (gmameui_ini_file, "Default", "ModifyThe", &error);
	gui_prefs.ShowFolderList = g_key_file_get_integer (gmameui_ini_file, "Default", "ShowFolderList", &error);
	gui_prefs.GameCheck = g_key_file_get_integer (gmameui_ini_file, "Default", "GameCheck", &error);
	gui_prefs.VersionCheck = g_key_file_get_integer (gmameui_ini_file, "Default", "VersionCheck", &error);
	gui_prefs.use_xmame_options = g_key_file_get_integer (gmameui_ini_file, "Default", "UseXmameOptions", &error);
	gui_prefs.gui_joy = g_key_file_get_integer (gmameui_ini_file, "Default", "gui_joy", &error);
	gui_prefs.SortColumn = g_key_file_get_integer (gmameui_ini_file, "Default", "SortColumn", &error);
	gui_prefs.SortReverse = g_key_file_get_integer (gmameui_ini_file, "Default", "SortReverse", &error);
	gui_prefs.GUIPosX = g_key_file_get_integer (gmameui_ini_file, "Default", "GUIPosX", &error);
	gui_prefs.GUIPosY = g_key_file_get_integer (gmameui_ini_file, "Default", "GUIPosY", &error);
	gui_prefs.GUIWidth = g_key_file_get_integer (gmameui_ini_file, "Default", "GUIWidth", &error);
	gui_prefs.GUIHeight = g_key_file_get_integer (gmameui_ini_file, "Default", "GUIHeight", &error);
	gui_prefs.current_mode = g_key_file_get_integer (gmameui_ini_file, "Default", "ListMode", &error);

	gui_prefs.Splitters = g_key_file_get_integer_list (gmameui_ini_file, "Default", "Splitters", &sizes, &error);
	gui_prefs.ColumnWidth = g_key_file_get_integer_list (gmameui_ini_file, "Default", "ColumnWidth", &columnsize, &error);
	gui_prefs.ColumnOrder = g_key_file_get_integer_list (gmameui_ini_file, "Default", "ColumnOrder", &columnsize, &error);
	gui_prefs.ColumnShown = g_key_file_get_integer_list (gmameui_ini_file, "Default", "ColumnShown", &columnsize, &error);

	g_key_file_free (gmameui_ini_file);
	g_free (filename);

	if (game_list.roms && default_game) {
		GList *listpointer;
		/* Try and find the default game in the ROM list */
		listpointer = g_list_find_custom (game_list.roms, default_game,
						  (GCompareFunc) g_ascii_strcasecmp);		

		if (listpointer) {
			gui_prefs.current_game = (RomEntry *) listpointer->data;
			GMAMEUI_DEBUG ("Setting current game to %s", gui_prefs.current_game->romname);
		}
	}

	g_error_free(error);
	g_free (default_game);

	return TRUE;
}

gboolean
save_gmameui_ini (void)
{
	gchar *filename;
	
	GMAMEUI_DEBUG ("Saving gmameui.ini");

	filename = g_build_filename (g_get_home_dir (), ".gmameui", "gmameui.ini", NULL);

	GKeyFile *gmameui_ini_file = g_key_file_new ();
	
	if (gui_prefs.current_game)
		g_key_file_set_string (gmameui_ini_file, "Default", "DefaultGame", gui_prefs.current_game->romname);
	g_key_file_set_integer (gmameui_ini_file, "Default", "FolderID", gui_prefs.FolderID);
	g_key_file_set_integer (gmameui_ini_file, "Default", "ShowScreenShot", gui_prefs.ShowScreenShot);
	g_key_file_set_integer (gmameui_ini_file, "Default", "ShowScreenShotTab", gui_prefs.ShowScreenShotTab);
	g_key_file_set_integer (gmameui_ini_file, "Default", "ShowFlyer", gui_prefs.ShowFlyer);
	g_key_file_set_integer (gmameui_ini_file, "Default", "ShowToolBar", gui_prefs.ShowToolBar);
	g_key_file_set_integer (gmameui_ini_file, "Default", "ModifyThe", gui_prefs.ModifyThe);
	g_key_file_set_integer (gmameui_ini_file, "Default", "ShowStatusBar", gui_prefs.ShowStatusBar);
	g_key_file_set_integer (gmameui_ini_file, "Default", "ShowFolderList", gui_prefs.ShowFolderList);
	g_key_file_set_integer (gmameui_ini_file, "Default", "GameCheck", gui_prefs.GameCheck);
	g_key_file_set_integer (gmameui_ini_file, "Default", "VersionCheck", gui_prefs.VersionCheck);
	g_key_file_set_integer (gmameui_ini_file, "Default", "UseXmameOptions", gui_prefs.use_xmame_options);
	g_key_file_set_integer (gmameui_ini_file, "Default", "gui_joy", gui_prefs.gui_joy);
	g_key_file_set_integer (gmameui_ini_file, "Default", "SortColumn", gui_prefs.SortColumn);
	g_key_file_set_integer (gmameui_ini_file, "Default", "SortReverse", gui_prefs.SortReverse);

	gtk_window_get_position (GTK_WINDOW (MainWindow), &gui_prefs.GUIPosX, &gui_prefs.GUIPosY);
	g_key_file_set_integer (gmameui_ini_file, "Default", "GUIPosX", gui_prefs.GUIPosX);
	g_key_file_set_integer (gmameui_ini_file, "Default", "GUIPosY", gui_prefs.GUIPosY);

	
	gdk_drawable_get_size (MainWindow->window, &gui_prefs.GUIWidth, &gui_prefs.GUIHeight);
	g_key_file_set_integer (gmameui_ini_file, "Default", "GUIWidth", gui_prefs.GUIWidth);
	g_key_file_set_integer (gmameui_ini_file, "Default", "GUIHeight", gui_prefs.GUIHeight);

	g_key_file_set_integer (gmameui_ini_file, "Default", "ListMode", gui_prefs.current_mode);
	g_key_file_set_integer_list (gmameui_ini_file, "Default", "Splitters", gui_prefs.Splitters, 2); /* TODO */

/*	fprintf (gmameui_ini_file, "Splitters=%i,%i\r\n",
			gui_prefs.ShowFolderList ? main_gui.scrolled_window_filters->allocation.width : gui_prefs.Splitters[0],
			gui_prefs.ShowScreenShot ? main_gui.scrolled_window_games->allocation.width : gui_prefs.Splitters[1]);*/

	g_key_file_set_integer_list (gmameui_ini_file, "Default", "ColumnWidth", gui_prefs.ColumnWidth, NUMBER_COLUMN);
	g_key_file_set_integer_list (gmameui_ini_file, "Default", "ColumnOrder", gui_prefs.ColumnOrder, NUMBER_COLUMN);
	g_key_file_set_integer_list (gmameui_ini_file, "Default", "ColumnShown", gui_prefs.ColumnShown, NUMBER_COLUMN);

	g_key_file_save_to_file (gmameui_ini_file, filename, NULL);

	g_key_file_free (gmameui_ini_file);
	g_free (filename);
	
	GMAMEUI_DEBUG ("Done saving gmameui.ini");
	
	return TRUE;
}

/* this the master config file, it will contain the path to every other config file
   when we will share these config files with another mame32 application */
gboolean
load_gmameuirc (void)
{
	FILE *gamerc_file;
	gchar *filename;
	gchar line[2000];
	gchar **option_value = NULL;
	gchar *mame_executable = NULL;
	gchar **xmame_executables;

	GMAMEUI_DEBUG ("Loading gmameuirc");

	gui_prefs.catverDirectory = g_build_filename (g_get_home_dir (), ".gmameui", NULL);
	gui_prefs.CheatFile = g_build_filename (XMAME_ROOT, "cheat.dat", NULL);
	gui_prefs.HiscoreFile = g_build_filename (XMAME_ROOT, "hiscore.dat", NULL);
	gui_prefs.HistoryFile = g_build_filename (XMAME_ROOT, "history.dat", NULL);
	gui_prefs.MameInfoFile = g_build_filename (XMAME_ROOT, "mameinfo.dat", NULL);
	
	gui_prefs.clone_color.red = 47031;
	gui_prefs.clone_color.green = 47031;
	gui_prefs.clone_color.blue = 47031;
	gui_prefs.Joystick_in_GUI = g_strdup (get_joy_dev ());

	filename = g_build_filename (g_get_home_dir (), ".gmameui", "gmameuirc", NULL);	
	gamerc_file = fopen (filename, "r");
	g_free (filename);
	if (!gamerc_file)
	{
		xmame_table_add ("xmame");
		GMAMEUI_DEBUG ("gmameuirc not found");
		return FALSE;
	}

	while (fgets (line, 500, gamerc_file))
	{
		/* skip comments */
		if (*line != '#')
		{	
			if (*line == '\n')
				break;
			/* FIXME: leaks here */
			option_value = g_strsplit (g_strstrip (g_strdelimit (line, "[]\n", ' ')), "=", 0);
			if (option_value[1] != NULL)
			{
				if (!strcmp (option_value[0], "mame_executable"))
				{
					if (mame_executable)
						g_free (mame_executable);
					mame_executable = g_strdup (option_value[1]);
				} else if (!strcmp (option_value[0], "xmame_executables_array"))
				{
					gint i;
					xmame_executables = g_strsplit (option_value[1], ";", 0);
					if (xmame_executables)
					{
						for (i = 0; xmame_executables[i]; i++)
							xmame_table_add (xmame_executables[i]);
						g_strfreev (xmame_executables);
					}			
				} else if (!strcmp (option_value[0], "catverDirectory"))
				{
					g_free (gui_prefs.catverDirectory);
					gui_prefs.catverDirectory = g_strdup (option_value[1]);
				} else if (!strcmp (option_value[0], "CheatFile"))
				{	
					g_free (gui_prefs.CheatFile);
					gui_prefs.CheatFile = g_strdup (option_value[1]);
				} else if (!strcmp (option_value[0], "HiscoreFile"))
				{	
					g_free (gui_prefs.HiscoreFile);
					gui_prefs.HiscoreFile = g_strdup (option_value[1]);
				} else if (!strcmp (option_value[0], "HistoryFile"))
				{	
					g_free (gui_prefs.HistoryFile);
					gui_prefs.HistoryFile = g_strdup (option_value[1]);
				} else if (!strcmp (option_value[0], "MameInfoFile"))
				{	
					g_free (gui_prefs.MameInfoFile);
					gui_prefs.MameInfoFile = g_strdup (option_value[1]);
				} else if (!strcmp (option_value[0], "Joystick_in_GUI"))
				{	
					g_free (gui_prefs.Joystick_in_GUI);
					gui_prefs.Joystick_in_GUI = g_strdup (option_value[1]);
				} else if (!strcmp (option_value[0], "clone_color.red"))
				{	
					gui_prefs.clone_color.red = atoi (option_value[1]);
				} else if (!strcmp (option_value[0], "clone_color.green"))
				{	
					gui_prefs.clone_color.green = atoi (option_value[1]);
				} else if (!strcmp (option_value[0], "clone_color.blue"))
				{	
					gui_prefs.clone_color.blue = atoi (option_value[1]);
				}
			}
			g_strfreev (option_value);
		}
	}
	fclose (gamerc_file);

	if (mame_executable)
	{
		current_exec = xmame_table_get (mame_executable);
		g_free (mame_executable);
	}
	
	if (!current_exec)
		current_exec = xmame_table_get_by_index (0);

	return TRUE;
}


gboolean
save_gmameuirc (void)
{
	FILE *gamerc_file;
	gchar *filename;
	guint i;
	
	filename = g_build_filename (g_get_home_dir (), ".gmameui", "gmameuirc", NULL);	
	gamerc_file = fopen (filename, "w");
	g_free (filename);
	if (!gamerc_file)
	{
		GMAMEUI_DEBUG ("unable to write gmameuirc");
		return (FALSE);
	}
	fputs ("# master config file of GMAMEUI\n", gamerc_file);
	if (current_exec)
		fprintf (gamerc_file, "mame_executable=%s\n", current_exec->path);

	if (xmame_table_size () > 0)
	{
		gchar **exec_paths;
		exec_paths = xmame_table_get_all ();
		fputs ("xmame_executables_array=", gamerc_file);
		for (i = 0; exec_paths[i] != NULL; i++)
		{	
			fputs (exec_paths[i], gamerc_file);
			if (exec_paths[i + 1] != NULL)
				fputs (";", gamerc_file);
		}
		g_free (exec_paths);
	}

	fprintf (gamerc_file, "\ncatverDirectory=%s", gui_prefs.catverDirectory);
	fprintf (gamerc_file, "\nCheatFile=%s", gui_prefs.CheatFile);
	fprintf (gamerc_file, "\nHiscoreFile=%s", gui_prefs.HiscoreFile);
	fprintf (gamerc_file, "\nHistoryFile=%s", gui_prefs.HistoryFile);
	fprintf (gamerc_file, "\nMameInfoFile=%s", gui_prefs.MameInfoFile);
	fprintf (gamerc_file, "\nJoystick_in_GUI=%s", gui_prefs.Joystick_in_GUI);
	fprintf (gamerc_file, "\nclone_color.red=%i", gui_prefs.clone_color.red);
	fprintf (gamerc_file, "\nclone_color.green=%i", gui_prefs.clone_color.green);
	fprintf (gamerc_file, "\nclone_color.blue=%i\n", gui_prefs.clone_color.blue);
	
	fclose (gamerc_file);
	
	return TRUE;
}


/* this is where directory paths are set (common with mame32k) */
gboolean
load_dirs_ini (void)
{
	gchar *filename;
//	gsize paths[10];	/* FIXME Define max number of rom/sample dirs */
	gsize paths;	/* FIXME Define max number of rom/sample dirs */
	
		/* Set defaults */
	gui_prefs.RomPath = g_strsplit (g_build_filename (XMAME_ROOT, "roms", NULL), ";", 0);
	gui_prefs.SamplePath = g_strsplit (g_build_filename (XMAME_ROOT, "samples", NULL), ";", 0);
	gui_prefs.SnapshotDirectory = g_build_filename (XMAME_ROOT, "snap", NULL);
	gui_prefs.ArtworkDirectory = g_build_filename (XMAME_ROOT, "artwork", NULL);
	gui_prefs.HiscoreDirectory = g_build_filename (g_get_home_dir () , ".xmame", "hi", NULL);
	gui_prefs.FlyerDirectory = g_build_filename (XMAME_ROOT, "flyers", NULL);
	gui_prefs.CabinetDirectory = g_build_filename (XMAME_ROOT, "cabinets", NULL);
	gui_prefs.MarqueeDirectory = g_build_filename (XMAME_ROOT, "marquees", NULL);
	gui_prefs.TitleDirectory = g_build_filename (XMAME_ROOT, "titles", NULL);
	gui_prefs.IconDirectory = g_build_filename (XMAME_ROOT, "icons", NULL);
	
	gui_prefs.DiffDirectory = g_build_filename (g_get_home_dir (), ".xmame", "diff", NULL);
	gui_prefs.CtrlrDirectory = g_build_filename (XMAME_ROOT, "ctrlr", NULL);
	
	gui_prefs.InputDirectory = g_build_filename (g_get_home_dir (), ".xmame", "inp", NULL);
	gui_prefs.NVRamDirectory = g_build_filename (g_get_home_dir (), ".xmame", "nvram", NULL);
	gui_prefs.MemCardDirectory = g_build_filename (g_get_home_dir (), ".xmame", "memcard", NULL);
	gui_prefs.ConfigDirectory = g_build_filename (g_get_home_dir (), ".xmame" , "cfg", NULL);
	gui_prefs.StateDirectory = g_build_filename (g_get_home_dir (), ".xmame", "sta", NULL);
	gui_prefs.inipath = g_build_filename (g_get_home_dir (), ".xmame" , "ini", NULL);
	
	filename = g_build_filename (g_get_home_dir (), ".gmameui", "dirs.ini", NULL);	
GMAMEUI_DEBUG ("Loading directories ini file");
	GKeyFile *dirsini_list = g_key_file_new ();
	GError *error = NULL;
	gboolean result = g_key_file_load_from_file (dirsini_list, filename, G_KEY_FILE_KEEP_COMMENTS, &error);
	g_free (filename);
	if (!result) {
		GMAMEUI_DEBUG ("Error loading dirs.ini file: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}

	gui_prefs.RomPath = g_key_file_get_string_list (dirsini_list, "Directories", "RomPath", &paths, &error);
	gui_prefs.SamplePath = g_key_file_get_string_list (dirsini_list, "Directories", "SamplePath", &paths, &error);

	gui_prefs.ConfigDirectory = g_key_file_get_string (dirsini_list, "Directories", "ConfigDirectory", &error);
	gui_prefs.NVRamDirectory = g_key_file_get_string (dirsini_list, "Directories", "NVRamDirectory", &error);
	gui_prefs.CtrlrDirectory = g_key_file_get_string (dirsini_list, "Directories", "CtrlrDirectory", &error);
	gui_prefs.MemCardDirectory = g_key_file_get_string (dirsini_list, "Directories", "MemCardDirectory", &error);
	gui_prefs.InputDirectory = g_key_file_get_string (dirsini_list, "Directories", "InputDirectory", &error);
	gui_prefs.HiscoreDirectory = g_key_file_get_string (dirsini_list, "Directories", "HiscoreDirectory", &error);
	gui_prefs.StateDirectory = g_key_file_get_string (dirsini_list, "Directories", "StateDirectory", &error);
	gui_prefs.ArtworkDirectory = g_key_file_get_string (dirsini_list, "Directories", "ArtworkDirectory", &error);
	gui_prefs.SnapshotDirectory = g_key_file_get_string (dirsini_list, "Directories", "SnapshotDirectory", &error);
	gui_prefs.FlyerDirectory = g_key_file_get_string (dirsini_list, "Directories", "FlyerDirectory", &error);
	gui_prefs.CabinetDirectory = g_key_file_get_string (dirsini_list, "Directories", "CabinetDirectory", &error);
	gui_prefs.MarqueeDirectory = g_key_file_get_string (dirsini_list, "Directories", "MarqueeDirectory", &error);
	gui_prefs.TitleDirectory = g_key_file_get_string (dirsini_list, "Directories", "TitleDirectory", &error);
	gui_prefs.DiffDirectory = g_key_file_get_string (dirsini_list, "Directories", "DiffDirectory", &error);
	gui_prefs.IconDirectory = g_key_file_get_string (dirsini_list, "Directories", "IconDirectory", &error);
	gui_prefs.inipath = g_key_file_get_string (dirsini_list, "Directories", "inipath", &error);

	g_key_file_free (dirsini_list);
	
	return TRUE;
}

gboolean
save_dirs_ini (void)
{
	gchar *filename;

	GMAMEUI_DEBUG ("Saving dirs.ini");

	filename = g_build_filename (g_get_home_dir (), ".gmameui", "dirs.ini", NULL);	

	GKeyFile *dirsini_list = g_key_file_new ();

	g_key_file_set_string_list (dirsini_list, "Directories", "RomPath", gui_prefs.RomPath, g_strv_length (gui_prefs.RomPath));
	g_key_file_set_string_list (dirsini_list, "Directories", "SamplePath", gui_prefs.SamplePath, g_strv_length (gui_prefs.RomPath));

	g_key_file_set_string (dirsini_list, "Directories", "ConfigDirectory", gui_prefs.ConfigDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "NVRamDirectory", gui_prefs.NVRamDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "CtrlrDirectory", gui_prefs.CtrlrDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "MemCardDirectory", gui_prefs.MemCardDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "InputDirectory", gui_prefs.InputDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "HiscoreDirectory", gui_prefs.HiscoreDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "StateDirectory", gui_prefs.StateDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "ArtworkDirectory", gui_prefs.ArtworkDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "SnapshotDirectory", gui_prefs.SnapshotDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "FlyerDirectory", gui_prefs.FlyerDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "CabinetDirectory", gui_prefs.CabinetDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "MarqueeDirectory", gui_prefs.MarqueeDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "TitleDirectory", gui_prefs.TitleDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "DiffDirectory", gui_prefs.DiffDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "IconDirectory", gui_prefs.IconDirectory);
	g_key_file_set_string (dirsini_list, "Directories", "inipath", gui_prefs.inipath);
	
	g_key_file_save_to_file (dirsini_list, filename, NULL);
	
	g_key_file_free (dirsini_list);
	g_free (filename);
	
	return TRUE;
}

void
quick_check (void)
{
	static gboolean quick_check_running = FALSE;
	GList *list_pointer;
	gchar *filename;
	gint nb_rom_not_checked;
	gfloat done;
	int i;
	RomEntry *rom;


	GMAMEUI_DEBUG ("Running quick check.");
	if (game_list.num_games == 0)
		return;

	/*prevent user to launch several quick check at the same time */
	if (quick_check_running)
	{
		GMAMEUI_DEBUG ("Quick check already running");
		return;
	}
	quick_check_running = TRUE;
		
	show_progress_bar ();
	filename = g_malloc (200);
	nb_rom_not_checked = g_list_length (game_list.not_checked_list);
	g_message (_("Performing quick check, please wait:"));
	/* Disable the callback */
	g_signal_handlers_block_by_func (G_OBJECT (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (main_gui.scrolled_window_games))),
					 (gpointer)adjustment_scrolled, NULL);

	/* need to use the last or I can find the list anymore*/
	for (list_pointer = game_list.not_checked_list;list_pointer ;list_pointer = g_list_next (list_pointer))
	{
		rom = (RomEntry *)list_pointer->data;

		/* Looking for roms */
		for (i=0;gui_prefs.RomPath[i]!=NULL;i++)
		{
			g_snprintf (filename, 200, "%s" G_DIR_SEPARATOR_S "%s.zip", gui_prefs.RomPath[i], rom->romname);
			if (g_file_test (filename, g_file_test (filename, G_FILE_TEST_EXISTS)))
			{
				rom->has_roms = CORRECT;
			}
			else
			{	/*test of directories*/
				g_snprintf (filename, 200, "%s" G_DIR_SEPARATOR_S "%s", gui_prefs.RomPath[i], rom->romname);
				if (g_file_test (filename, G_FILE_TEST_IS_DIR))
				{
					rom->has_roms = CORRECT;
				}
				else if (strcmp (rom->cloneof, "-"))
				{
					g_snprintf (filename, 200, "%s" G_DIR_SEPARATOR_S "%s.zip", gui_prefs.RomPath[i], rom->cloneof);
					if (g_file_test (filename, G_FILE_TEST_EXISTS))
					{
						rom->has_roms = CORRECT;
					}
					else
					{
						g_snprintf (filename, 200, "%s" G_DIR_SEPARATOR_S "%s", gui_prefs.RomPath[i], rom->cloneof);
						if (g_file_test (filename, G_FILE_TEST_IS_DIR))
						{
							rom->has_roms = CORRECT;
						}
					}
				}
			}
			/* prevent to look into other folders if we already find the rom*/
			if (rom->has_roms) break;
		}

		/* Looking for samples */
		rom->has_samples = 0;

		if (rom->nb_samples > 0)
		{
			for (i = 0; gui_prefs.SamplePath[i] != NULL; i++)
			{
				g_snprintf (filename, 200, "%s" G_DIR_SEPARATOR_S "%s.zip", gui_prefs.SamplePath[i], rom->romname);
				if (g_file_test (filename, G_FILE_TEST_EXISTS))
		        {
		        	rom->has_samples = 1;
		        }
		        else
		        {	/*test of directories*/
					g_snprintf (filename, 200, "%s" G_DIR_SEPARATOR_S "%s", gui_prefs.SamplePath[i], rom->romname);
					if (g_file_test (filename, G_FILE_TEST_IS_DIR))
					{
						rom->has_samples = 1;
					}
					else if (strcmp (rom->sampleof, "-"))
			       	{
			       		g_snprintf (filename, 200, "%s" G_DIR_SEPARATOR_S "%s.zip", gui_prefs.SamplePath[i], rom->sampleof);
			       		if (g_file_test (filename, G_FILE_TEST_EXISTS))
			       		{
			      			rom->has_samples = 1;
			       		}
						else
						{
							g_snprintf (filename, 200, "%s" G_DIR_SEPARATOR_S "%s", gui_prefs.SamplePath[i], rom->sampleof);
							if (g_file_test (filename, G_FILE_TEST_IS_DIR))
							{
								rom->has_samples = 1;
							}
						}
					}
				}
				/* prevent to look into other folders if we already find the sample*/
				if (rom->has_samples) break;
			}
		}

		list_pointer->data = NULL;
		nb_rom_not_checked--;

		done = (gfloat) (game_list.num_games - nb_rom_not_checked) / (gfloat)game_list.num_games;

#if 0
		if (done >= 0.0 && done <= 1.0)
			update_progress_bar (done);
#endif

		/* if the game is in the list, update it */
		if (rom->is_in_list)
			update_game_in_list (rom);
	}
	g_list_free(game_list.not_checked_list);
	game_list.not_checked_list = NULL;

	g_free (filename);
	hide_progress_bar ();
	quick_check_running= FALSE;
	
	/* Re-Enable the callback */
	g_signal_handlers_unblock_by_func (G_OBJECT (gtk_scrolled_window_get_vadjustment (GTK_SCROLLED_WINDOW (main_gui.scrolled_window_games))),
					   (gpointer)adjustment_scrolled, NULL);		
}

GList *
get_ctrlr_list (void)
{
	GList *ctrlr_list = NULL;
	GDir *di;
	const gchar *dent;
	gchar *filename;
	gsize filename_len;

	GMAMEUI_DEBUG ("Getting the ctrlr list. %s", gui_prefs.CtrlrDirectory);

	di = g_dir_open (gui_prefs.CtrlrDirectory, 0, NULL);
	if (!di)
	{
		GMAMEUI_DEBUG ("ERROR - unable to open folder %s", gui_prefs.CtrlrDirectory);
	}
	else
	{
		while ( (dent = g_dir_read_name (di)))
		{
			filename = g_build_filename (gui_prefs.CtrlrDirectory, dent, NULL);
			if (g_file_test (filename, G_FILE_TEST_IS_DIR))
				ctrlr_list = g_list_append (ctrlr_list, g_strdup (dent));

			if (g_str_has_suffix (filename, ".cfg")) {
				filename_len = strlen (dent) - 4;
				ctrlr_list = g_list_append (ctrlr_list, g_strndup (dent, filename_len));
			}

			g_free (filename);
		}
		g_dir_close (di);
	}

	return ctrlr_list;
}
