/* main.c
 *
 * Copyright (C) 2020 Gooroom <gooroom@gooroom.kr>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <glib/gi18n.h>

#include "define.h"
#include "utils.h"
#include "viewer-installer-config.h"
#include "viewer-installer-application.h"

int
main (int   argc,
      char *argv[])
{
    g_autoptr(GtkApplication) app = NULL;

	gboolean checked;
	GSettings *settings;

    if (check_package (VIEWER_NAME))
	{
	    return 0;
	}

    settings = g_settings_new (VIEWER_SCHEMA);
    checked = g_settings_get_boolean (settings, VIEWER_SCHEMA_KEY);
	if (checked)
	{
	    g_object_unref (settings);
		return 0;
	}

    /* Set up gettext translations */
    bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
    textdomain (GETTEXT_PACKAGE);
    app = GTK_APPLICATION(viewer_installer_application_new());

    return g_application_run (G_APPLICATION (app), argc, argv);
}
