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

#include "mame-exec-list.h"

static void mame_exec_list_class_init (MameExecListClass *klass);
static void mame_exec_list_init (MameExecList *list);
static void mame_exec_list_finalize (GObject *obj);

G_DEFINE_TYPE (MameExecList, mame_exec_list, G_TYPE_OBJECT)

struct _MameExecListPrivate {
	GList *list;
	/*TODOMameExec *current_exec;*/
	XmameExecutable *current_exec;
};

/* TODO Replace XmameExecutable with new MameExec class */

/*TODO
void
mame_exec_list_add (MameExecList *list, MameExec *exec)
{
	GList *list_ptr;
	gchar *exec_path;
	gboolean exist;

	g_return_if_fail (list != NULL);
	g_return_if_fail (exec != NULL);

	exist = FALSE;

	g_object_get (exec, "exec-path", &exec_path, NULL);
	GMAMEUI_DEBUG ("Adding executable %s to exec list", exec_path);

	* Check if the executable is already in the list *
	for (list_ptr = g_list_first (list->priv->list); list_ptr != NULL; list_ptr = g_list_next (list_ptr)) {
		MameExec *exist_exec;
		gchar *exist_path;

		exist_exec = (MameExec *) list_ptr->data;
		g_object_get (exist_exec, "exec-path", &exist_path, NULL);

		if (exist_path != NULL) {
			GMAMEUI_DEBUG ("Comparing against existing exec %s", exist_path);
			if (g_ascii_strcasecmp (exec_path, exist_path) == 0)
				exist = TRUE;
			else
				exist = FALSE;
		}

		g_free (exist_path);
	}

	if (!exist)
		list->priv->list = g_list_append (list->priv->list, exec);
	else
		GMAMEUI_DEBUG ("%s already in the executable list - skipping", exec_path);

}*/
void
mame_exec_list_add (MameExecList *list, XmameExecutable *exec)
{
	GList *list_ptr;
	gboolean exist;

	g_return_if_fail (list != NULL);
	g_return_if_fail (exec != NULL);

	exist = FALSE;

	GMAMEUI_DEBUG ("Adding executable %s to exec list", exec->path);

	/* Check if the executable is already in the list */
	for (list_ptr = g_list_first (list->priv->list); list_ptr != NULL; list_ptr = g_list_next (list_ptr)) {
		XmameExecutable *exist_exec;
		gchar *exist_path;

		exist_exec = (XmameExecutable *) list_ptr->data;

		if (g_ascii_strcasecmp (exec->path, exist_exec->path) == 0)
			exist = TRUE;
		else
			exist = FALSE;

	}

	if (!exist)
		list->priv->list = g_list_append (list->priv->list, exec);
	else
		GMAMEUI_DEBUG ("%s already in the executable list - skipping", exec->path);

}

GList *
mame_exec_list_get_list (MameExecList *list)
{
	return list->priv->list;
}

/*TODOMameExec *
mame_exec_list_nth (MameExecList *list, guint index)
{
	MameExec *exec;
	GList *list_ptr;

	list_ptr = g_list_nth (list->priv->list, index);
	exec = (MameExec *) list_ptr->data;

	return exec;
}*/
XmameExecutable *
mame_exec_list_nth (MameExecList *list, guint index)
{
	XmameExecutable *exec;
	GList *list_ptr;

	list_ptr = g_list_nth (list->priv->list, index);
	
	g_return_val_if_fail (list_ptr != NULL, NULL);
	
	exec = (XmameExecutable *) list_ptr->data;
	
	return exec;
}

/*TODOMameExec *
mame_exec_list_get_exec_by_path (MameExecList *list, gchar *path)
{
	GList *list_ptr;

	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);

	for (list_ptr = g_list_first (list->priv->list); list_ptr != NULL; list_ptr = g_list_next (list_ptr)) {
		MameExec *curr_exec;

		curr_exec = (MameExec *) list_ptr->data;

		if (g_ascii_strcasecmp (mame_exec_get_path (curr_exec), path) == 0)
			return curr_exec;
	}

	return NULL;
}*/
XmameExecutable *
mame_exec_list_get_exec_by_path (MameExecList *list, gchar *path)
{
	GList *list_ptr;

	g_return_val_if_fail (list != NULL, NULL);
	g_return_val_if_fail (path != NULL, NULL);

	for (list_ptr = g_list_first (list->priv->list); list_ptr != NULL; list_ptr = g_list_next (list_ptr)) {
		XmameExecutable *curr_exec;

		curr_exec = (XmameExecutable *) list_ptr->data;

		if (g_ascii_strcasecmp (curr_exec->path, path) == 0)
			return curr_exec;
	}

	return NULL;
}

/*TODOMameExec *
mame_exec_list_get_current_executable (MameExecList *list)
{
	g_return_val_if_fail (list != NULL, NULL);

	return list->priv->current_exec;
}*/
XmameExecutable *
mame_exec_list_get_current_executable (MameExecList *list)
{
	g_return_val_if_fail (list != NULL, NULL);

	return list->priv->current_exec;
}

/*TODOvoid
mame_exec_list_set_current_executable (MameExecList *list, MameExec *exec)
{
	g_return_if_fail (list != NULL);
	g_return_if_fail (exec != NULL);

	list->priv->current_exec = exec;
	GMAMEUI_DEBUG ("Setting current executable to %s", mame_exec_get_path (list->priv->current_exec));
}*/
void
mame_exec_list_set_current_executable (MameExecList *list, XmameExecutable *exec)
{
	g_return_if_fail (list != NULL);
	g_return_if_fail (exec != NULL);

	list->priv->current_exec = exec;
	GMAMEUI_DEBUG ("Setting current executable to %s", exec->path);
	/*GMAMEUI_DEBUG ("Setting current executable to %s", mame_exec_get_path (list->priv->current_exec));*/
}

gboolean
mame_exec_list_has_current_executable (MameExecList *list)
{
	return (list->priv->current_exec != NULL);
}

int
mame_exec_list_size (MameExecList *list)
{
	g_return_val_if_fail (list != NULL, 0);

	return g_list_length (list->priv->list);
}

static void
mame_exec_list_finalize (GObject *obj)
{
	GMAMEUI_DEBUG ("Finalising mame_exec_list object");
	
	MameExecList *li = MAME_EXEC_LIST (obj);
	
	/* FIXME TODO
	g_list_foreach (li->priv->list, g_object_unref, NULL);
	g_list_free (li->priv->list);*/
	
	/* Reset the current executable */
	li->priv->current_exec = NULL;
	
// FIXME TODO	g_free (li->priv);
	
	GMAMEUI_DEBUG ("Finalising mame_exec_list object... done");
	
	/* FIXME TODO Unref all the strings and destroy the object */
}

static void
mame_exec_list_class_init (MameExecListClass *klass)
{
	
	
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	
//	object_class->set_property = mame_exec_list_set_property;
//	object_class->get_property = mame_exec_list_get_property;
	object_class->finalize = mame_exec_list_finalize;

}

static void
mame_exec_list_init (MameExecList *list)
{
	
	GMAMEUI_DEBUG ("Creating mame_exec_list object");
	list->priv = g_new0 (MameExecListPrivate, 1);

	list->priv->list = NULL;

}

MameExecList* mame_exec_list_new (void)
{
	return g_object_new (MAME_TYPE_EXEC_LIST, NULL);
}
