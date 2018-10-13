
#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>
#include <stdlib.h>
#include <time.h>

#ifdef USE_PAM
#include <security/pam_appl.h>
#endif

#ifndef _XOPEN_SOURCE
# define _XOPEN_SOURCE 1
#endif
#include <unistd.h>

#include <X11/Xos.h>
#include <X11/Xfuncs.h>
#include <X11/Xfuncproto.h>
#include <X11/Xmd.h>
#include <X11/Xauth.h>
#include <X11/Intrinsic.h>
#include <X11/Xdmcp.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>

#include "greet.h"

static ConfigData *cfg = NULL;

static GtkWidget *le, *pe, *se;
static gchar *uname = NULL;
static gchar *sess_cmd = NULL;

/*
 * Function pointers filled in by the initial call ito the library
 */

int (*__xdm_PingServer) (struct display * d, Display * alternateDpy) = NULL;
void (*__xdm_SessionPingFailed) (struct display * d) = NULL;
void (*__xdm_Debug) (const char *fmt, ...) = NULL;
void (*__xdm_RegisterCloseOnFork) (int fd) = NULL;
void (*__xdm_SecureDisplay) (struct display * d, Display * dpy) = NULL;
void (*__xdm_UnsecureDisplay) (struct display * d, Display * dpy) = NULL;
void (*__xdm_ClearCloseOnFork) (int fd) = NULL;
void (*__xdm_SetupDisplay) (struct display * d) = NULL;
void (*__xdm_LogError) (const char *fmt, ...) = NULL;
void (*__xdm_SessionExit) (struct display * d, int status, int removeAuth) = NULL;
void (*__xdm_DeleteXloginResources) (struct display * d, Display * dpy) = NULL;
int (*__xdm_source) (char **environ, char *file) = NULL;
char **(*__xdm_defaultEnv) (void) = NULL;
char **(*__xdm_setEnv) (char **e, char *name, char *value) = NULL;
char **(*__xdm_putEnv) (const char *string, char **env) = NULL;
char **(*__xdm_parseArgs) (char **argv, char *string) = NULL;
void (*__xdm_printEnv) (char **e) = NULL;
char **(*__xdm_systemEnv) (struct display * d, char *user, char *home) = NULL;
void (*__xdm_LogOutOfMem) (const char *fmt, ...) = NULL;
void (*__xdm_setgrent) (void) = NULL;
struct group *(*__xdm_getgrent) (void) = NULL;
void (*__xdm_endgrent) (void) = NULL;
struct spwd *(*__xdm_getspnam) (const char *) = NULL;
void (*__xdm_endspent) (void) = NULL;
struct passwd *(*__xdm_getpwnam) (const char *) = NULL;
void (*__xdm_endpwent) (void) = NULL;
char *(*__xdm_crypt) (const char *, const char *) = NULL;
#ifdef USE_PAM
pam_handle_t **(*__xdm_thepamhp) (void) = NULL;
#endif

#ifdef USE_PAM
/* pam conversation handler */
static int
pamconv (int num_msg, const struct pam_message **msg, struct pam_response **response, void *appdata_ptr)
{
  gint i, ret = PAM_SUCCESS;
  const gchar *pam_msg_styles[5] = {
    "<invalid pam msg style>",
    "PAM_PROMPT_ECHO_OFF", "PAM_PROMPT_ECHO_ON",
    "PAM_ERROR_MSG", "PAM_TEXT_INFO"
  };
  struct pam_message *m;
  struct pam_response *r;

  *response = calloc (num_msg, sizeof (struct pam_response));
  if (*response == NULL)
    return PAM_BUF_ERR;

  m = (struct pam_message *) *msg;
  r = *response;

  for (i = 0; i < num_msg; i++, m++, r++)
    {
      Debug ("pam_msg: %s (%d): '%s'\n",
             ((m->msg_style > 0) && (m->msg_style <= 4)) ?
             pam_msg_styles[m->msg_style] : pam_msg_styles[0], m->msg_style, m->msg);

      switch (m->msg_style)
        {
        case PAM_PROMPT_ECHO_ON:
          r->resp = g_strdup (gtk_entry_get_text (GTK_ENTRY (le)));
          break;
        case PAM_PROMPT_ECHO_OFF:
          r->resp = g_strdup (gtk_entry_get_text (GTK_ENTRY (pe)));
          if (!r->resp[0])
            ret = PAM_BUF_ERR;
          break;
        case PAM_TEXT_INFO:
        case PAM_ERROR_MSG:
          LogError ("pam_msg: %s\n", m->msg);
          break;
        default:
          LogError ("Unknown PAM msg_style: %d\n", m->msg_style);
        }
    }

  return ret;
}
#endif

static void
parse_config ()
{
  gchar *cfile;

  if (cfg)
    return;

  cfg = g_new0 (ConfigData, 1);

  cfg->min_uid = MIN_UID;

  cfg->welcome = WELCOME;
  cfg->user_lbl = USERLABEL;
  cfg->pwd_lbl = PASSLABEL;
  cfg->sess_lbl = SESSLABEL;

  cfg->icon = ICONNAME;

  cfg->borders = BORDERS;
  cfg->width = -1;

  cfg->allow_root = TRUE;

  cfg->path = DEFAULT_PATH;

  cfg->reboot_cmd = REBOOT_CMD;
  cfg->poweroff_cmd = POWEROFF_CMD;
  cfg->suspend_cmd = SUSPEND_CMD;
  cfg->hibernate_cmd = HIBERNATE_CMD;

  cfile = g_build_filename (SYSCONFDIR, "greeter.conf", NULL);
  if (g_file_test (cfile, G_FILE_TEST_EXISTS))
    {
      GKeyFile *kf;

      kf = g_key_file_new ();
      if (g_key_file_load_from_file (kf, cfile, G_KEY_FILE_NONE, NULL))
        {
          if (g_key_file_has_group (kf, "Interface"))
            {
              if (g_key_file_has_key (kf, "Interface", "MinUID", NULL))
                cfg->min_uid = g_key_file_get_integer (kf, "Interface", "MinUID", NULL);
              if (g_key_file_has_key (kf, "Interface", "Welcome", NULL))
                cfg->welcome = g_key_file_get_locale_string (kf, "Interface", "Welcome", NULL, NULL);
              if (g_key_file_has_key (kf, "Interface", "Username", NULL))
                cfg->user_lbl = g_key_file_get_locale_string (kf, "Interface", "Username", NULL, NULL);
              if (g_key_file_has_key (kf, "Interface", "Password", NULL))
                cfg->pwd_lbl = g_key_file_get_locale_string (kf, "Interface", "Password", NULL, NULL);
              if (g_key_file_has_key (kf, "Interface", "Session", NULL))
                cfg->sess_lbl = g_key_file_get_locale_string (kf, "Interface", "Session", NULL, NULL);
              if (g_key_file_has_key (kf, "Interface", "Borders", NULL))
                cfg->borders = g_key_file_get_integer (kf, "Interface", "Borders", NULL);
              if (g_key_file_has_key (kf, "Interface", "Width", NULL))
                cfg->width = g_key_file_get_integer (kf, "Interface", "Width", NULL);
              if (g_key_file_has_key (kf, "Interface", "Icon", NULL))
                cfg->icon = g_key_file_get_string (kf, "Interface", "Icon", NULL);
              cfg->rcfile = g_key_file_get_string (kf, "Interface", "RcFile", NULL);
            }

          if (g_key_file_has_group (kf, "Login"))
            {
              cfg->session = g_key_file_get_string (kf, "Login", "Session", NULL);
              cfg->auto_login = g_key_file_get_boolean (kf, "Login", "Autologin", NULL);
              cfg->auto_user = g_key_file_get_string (kf, "Login", "Autouser", NULL);
              if (g_key_file_has_key (kf, "Login", "AllowRoot", NULL))
                cfg->allow_root = g_key_file_get_boolean (kf, "Login", "AllowRoot", NULL);
              if (g_key_file_has_key (kf, "Login", "Path", NULL))
                cfg->path = g_key_file_get_string (kf, "Login", "Path", NULL);
            }

          if (g_key_file_has_group (kf, "Actions"))
            {
              if (g_key_file_has_key (kf, "Actions", "Reboot", NULL))
                cfg->reboot_cmd = g_key_file_get_string (kf, "Actions", "Reboot", NULL);
              if (g_key_file_has_key (kf, "Actions", "Poweroff", NULL))
                cfg->poweroff_cmd = g_key_file_get_string (kf, "Actions", "Poweroff", NULL);
              if (g_key_file_has_key (kf, "Actions", "Suspend", NULL))
                cfg->suspend_cmd = g_key_file_get_string (kf, "Actions", "Suspend", NULL);
              if (g_key_file_has_key (kf, "Actions", "Hibernate", NULL))
                cfg->hibernate_cmd = g_key_file_get_string (kf, "Actions", "Hibernate", NULL);
            }
        }
      g_key_file_free (kf);
    }
  g_free (cfile);
}

static gboolean
username_compl_cb (GtkEntryCompletion * w, GtkTreeModel * m, GtkTreeIter * it, gpointer d)
{
  gchar *name;

  gtk_tree_model_get (m, it, 0, &name, -1);
  gtk_entry_set_text (GTK_ENTRY (le), name);
  g_signal_emit_by_name (G_OBJECT (le), "activate");

  return TRUE;
}

static void
load_users ()
{
  GtkListStore *store;
  GtkTreeIter iter;
  GtkEntryCompletion *compl;
  struct passwd *pwd;
  GHashTable *uht;

  uht = g_hash_table_new (g_str_hash, g_str_equal);
  store = gtk_list_store_new (1, G_TYPE_STRING);  
  while (pwd = getpwent ())
    {
      if (pwd->pw_uid >= cfg->min_uid)
        {
          if (!g_hash_table_contains (uht, pwd->pw_name))
            {
              g_hash_table_add (uht, pwd->pw_name);        
              gtk_list_store_append (store, &iter);
              gtk_list_store_set (store, &iter, 0, pwd->pw_name, -1);
            }
        }
    }
  endpwent ();
  //g_hash_table_destroy (uht);

  /* add special users */
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "@poweroff", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "@reboot", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "@suspend", -1);
  gtk_list_store_append (store, &iter);
  gtk_list_store_set (store, &iter, 0, "@hibernate", -1);

  compl = gtk_entry_completion_new ();
  gtk_entry_completion_set_model (compl, GTK_TREE_MODEL (store));
  gtk_entry_completion_set_text_column (compl, 0);
  gtk_entry_set_completion (GTK_ENTRY (le), compl);

  g_signal_connect (G_OBJECT (compl), "match-selected", G_CALLBACK (username_compl_cb), NULL);

  g_object_unref (store);
  g_object_unref (compl);
}

static void
load_sessions ()
{
  GDir *dir;
  const gchar *filename;
  GtkListStore *ss;
  gint len, s = 0, as = -1;

  dir = g_dir_open (SESSIONDIR, 0, NULL);
  if (!dir)
    return;

  len = strlen (cfg->session);
  ss = GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (se)));
  while ((filename = g_dir_read_name (dir)) != NULL)
    {
      GKeyFile *kf;
      gchar *fullname;

      if (!g_str_has_suffix (filename, ".desktop"))
        continue;

      kf = g_key_file_new ();
      fullname = g_build_filename (SESSIONDIR, filename, NULL);
      if (g_key_file_load_from_file (kf, fullname, 0, NULL))
        {
          gchar *type;

          type =  g_key_file_get_locale_string (kf, "Desktop Entry", "Type", NULL, NULL);

          if (type && strcasecmp (type, "xsession") == 0)
            {
              gchar *name, *icon, *cmd;
              GtkTreeIter it;
              GdkPixbuf *pb = NULL;

              name = g_key_file_get_locale_string (kf, "Desktop Entry", "Name", NULL, NULL);
              icon = g_key_file_get_string (kf, "Desktop Entry", "Icon", NULL);
              cmd = g_key_file_get_string (kf, "Desktop Entry", "Exec", NULL);

              if (icon)
                {
                  if (g_file_test (icon, G_FILE_TEST_EXISTS))
                    pb = gdk_pixbuf_new_from_file_at_size (icon, 16, 16, NULL);
                  else
                    pb = gtk_icon_theme_load_icon (gtk_icon_theme_get_default (), icon, 16, 0, NULL);
                }

              gtk_list_store_append (ss, &it);
              gtk_list_store_set (ss, &it, 0, pb, 1, name, 2, cmd, -1);

              if (g_ascii_strncasecmp (filename, cfg->session, len) == 0)
                as = s;
              s++;

              g_free (name);
              g_free (icon);
              g_free (cmd);
            }
          else
            Debug ("File %s is not a xsession\n", fullname);
        }

      g_free (fullname);
      g_key_file_free (kf);
    }
  g_dir_close (dir);

  gtk_combo_box_set_active (GTK_COMBO_BOX (se), as);
}

static void
username_cb (GtkWidget * w, gpointer d)
{
  const gchar *name = gtk_entry_get_text (GTK_ENTRY (le));

  if (!name || !name[0])
    {
      gtk_widget_grab_focus (le);
      return;
    }

  if (name[0] == '@')
    {
      /* proceed special names */
      if (strcmp (name + 1, "reboot") == 0)
        g_spawn_command_line_async (cfg->reboot_cmd, NULL);
      else if (strcmp (name + 1, "poweroff") == 0)
        g_spawn_command_line_async (cfg->poweroff_cmd, NULL);
      else if (strcmp (name + 1, "suspend") == 0)
        g_spawn_command_line_async (cfg->suspend_cmd, NULL);
      else if (strcmp (name + 1, "hibernate") == 0)
        g_spawn_command_line_async (cfg->hibernate_cmd, NULL);
    }

  gtk_widget_grab_focus (pe);
}

static void
verify_auth (GtkWidget * w, gpointer d)
{
  GtkTreeModel *model;
  GtkTreeIter it;
  const gchar *name;
#ifdef USE_PAM
  /* Run PAM conversation */
  pam_handle_t **pamh = thepamhp ();
  gint pam_error = PAM_SUCCESS;
  const gchar *pam_fname;
#else
  struct passwd *pw;
  struct spwd *sp;
  const gchar *pwd;
  gchar *cpwd;
#endif

  name = gtk_entry_get_text (GTK_ENTRY (le));

  if (!cfg->allow_root && strcmp (name, "root") == 0)
    goto clear;

  uname = g_strdup (name);

  /* get session command */
  model = gtk_combo_box_get_model (GTK_COMBO_BOX (se));
  gtk_combo_box_get_active_iter (GTK_COMBO_BOX (se), &it);
  gtk_tree_model_get (model, &it, 2, &sess_cmd, -1);

#ifdef USE_PAM
  /* authenticate user */
  pam_error = pam_authenticate (*pamh, 0);
  pam_fname = "pam_authenticate";
  if (pam_error != PAM_SUCCESS)
    {
      LogError ("%s failure: %s\n", pam_fname, pam_strerror (*pamh, pam_error));
      goto clear;
    }

  /* handle expired passwords */
  pam_error = pam_acct_mgmt (*pamh, 0);
  pam_fname = "pam_acct_mgmt";
  if (pam_error == PAM_NEW_AUTHTOK_REQD)
    {
      do
        pam_error = pam_chauthtok (*pamh, PAM_CHANGE_EXPIRED_AUTHTOK);
      while ((pam_error == PAM_AUTHTOK_ERR) || (pam_error == PAM_TRY_AGAIN));
      pam_fname = "pam_chauthtok";
    }
  if (pam_error != PAM_SUCCESS)
    {
      LogError ("%s failure: %s\n", pam_fname, pam_strerror (*pamh, pam_error));
      goto clear;
    }

  pam_setcred (*pamh, 0);

  gtk_main_quit ();
#else /* not PAM */
  /* get user */
  pw = getpwnam (name);
  endpwent ();
  if (!pw)
    goto clear;

  /* get password */
  pwd = gtk_entry_get_text (GTK_ENTRY (pe));
  if (!pwd || !pwd[0])
    goto clear;

  cpwd = pw->pw_passwd;
  if (!cpwd || !cpwd[0] || strcmp (cpwd, "x") == 0)
    {
      sp = getspnam (pw->pw_name);
      endspent ();
      if (!sp)
        goto clear;
      cpwd = sp->sp_pwdp;
    }

  if (g_ascii_strcasecmp ((const gchar *) crypt (pwd, cpwd), cpwd) == 0)
    gtk_main_quit ();
#endif /* USE_PAM */

clear:
#ifdef USE_PAM
  /* FIXME: memory leak? do wee need to free user before NULL-ed them?*/
  if (pam_error != PAM_SUCCESS)
    pam_set_item (*pamh, PAM_USER, NULL);
#endif
  gtk_entry_set_text (GTK_ENTRY (le), "");
  gtk_entry_set_text (GTK_ENTRY (pe), "");
  gtk_widget_grab_focus (le);
}

static GtkWidget *
create_dialog ()
{
  GtkWidget *w, *f, *b, *wb, *t, *l;
  GtkListStore *ss;
  GtkCellRenderer *r;
  gchar *wlc;

  /* window */
  w = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position (GTK_WINDOW (w), GTK_WIN_POS_CENTER_ALWAYS);
  if (cfg->width > 0)
    gtk_window_set_default_size (GTK_WINDOW (w), cfg->width, -1);

  f = gtk_frame_new (NULL);
  gtk_frame_set_shadow_type (GTK_FRAME (f), GTK_SHADOW_ETCHED_IN);
  gtk_container_set_border_width (GTK_CONTAINER (f), 2);
  gtk_container_add (GTK_CONTAINER (w), f);

  b = gtk_vbox_new (FALSE, 2);
  gtk_container_set_border_width (GTK_CONTAINER (b), cfg->borders);
  gtk_container_add (GTK_CONTAINER (f), b);

  wb = gtk_hbox_new (FALSE, 5);
  gtk_box_pack_start (GTK_BOX (b), wb, TRUE, TRUE, 0);

  /* welcome */
  l = gtk_image_new_from_icon_name (cfg->icon, GTK_ICON_SIZE_DIALOG);
  gtk_box_pack_start (GTK_BOX (wb), l, TRUE, FALSE, 2);
  l = gtk_label_new (NULL);
  gtk_misc_set_alignment (GTK_MISC (l), 1.0, 0.5);
  wlc = g_strdup_printf (cfg->welcome, g_get_host_name ());
  gtk_label_set_markup (GTK_LABEL (l), wlc);
  g_free (wlc);
  gtk_box_pack_start (GTK_BOX (wb), l, TRUE, TRUE, 2);

  /* login */
  t = gtk_table_new (3, 2, FALSE);
  gtk_table_set_row_spacings (GTK_TABLE (t), 10);
  gtk_table_set_col_spacings (GTK_TABLE (t), 10);
  gtk_container_set_border_width (GTK_CONTAINER (t), 15);
  gtk_box_pack_start (GTK_BOX (b), t, TRUE, TRUE, 5);

  /* session */
  l = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (l), cfg->sess_lbl);
  gtk_misc_set_alignment (GTK_MISC (l), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (t), l, 0, 1, 0, 1, GTK_FILL, 0, 0, 0);

  ss = gtk_list_store_new (3, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING);
  se = gtk_combo_box_new_with_model (GTK_TREE_MODEL (ss));
  g_object_unref (ss);
  gtk_table_attach (GTK_TABLE (t), se, 1, 2, 0, 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  r = gtk_cell_renderer_pixbuf_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (se), r, FALSE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (se), r, "pixbuf", 0, NULL);
  r = gtk_cell_renderer_text_new ();
  gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (se), r, TRUE);
  gtk_cell_layout_set_attributes (GTK_CELL_LAYOUT (se), r, "text", 1, NULL);

  /* user */
  l = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (l), cfg->user_lbl);
  gtk_misc_set_alignment (GTK_MISC (l), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (t), l, 0, 1, 1, 2, GTK_FILL, 0, 0, 0);
  le = gtk_entry_new ();
  gtk_table_attach (GTK_TABLE (t), le, 1, 2, 1, 2, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  /* password */
  l = gtk_label_new (NULL);
  gtk_label_set_markup (GTK_LABEL (l), cfg->pwd_lbl);
  gtk_misc_set_alignment (GTK_MISC (l), 1.0, 0.5);
  gtk_table_attach (GTK_TABLE (t), l, 0, 1, 2, 3, GTK_FILL, 0, 0, 0);
  pe = gtk_entry_new ();
  gtk_entry_set_visibility (GTK_ENTRY (pe), FALSE);
  gtk_table_attach (GTK_TABLE (t), pe, 1, 2, 2, 3, GTK_FILL | GTK_EXPAND, 0, 0, 0);

  g_signal_connect (G_OBJECT (le), "activate", G_CALLBACK (username_cb), NULL);
  g_signal_connect (G_OBJECT (pe), "activate", G_CALLBACK (verify_auth), NULL);

  load_users ();
  load_sessions ();

  return w;
}

greet_user_rtn
GreetUser (struct display * d,
           Display ** dpy, struct verify_info * verify, struct greet_info * greet, struct dlfuncs * dlfuncs)
{
  GtkWidget *w;
  GdkDisplay *gd;
  struct passwd *p;
  gchar **env;
#ifdef USE_PAM
  pam_handle_t **pamhp;
  struct pam_conv pc = { pamconv, NULL };
#endif

  /*
   * These must be set before they are used.
   */
  __xdm_PingServer = dlfuncs->_PingServer;
  __xdm_SessionPingFailed = dlfuncs->_SessionPingFailed;
  __xdm_Debug = dlfuncs->_Debug;
  __xdm_RegisterCloseOnFork = dlfuncs->_RegisterCloseOnFork;
  __xdm_SecureDisplay = dlfuncs->_SecureDisplay;
  __xdm_UnsecureDisplay = dlfuncs->_UnsecureDisplay;
  __xdm_ClearCloseOnFork = dlfuncs->_ClearCloseOnFork;
  __xdm_SetupDisplay = dlfuncs->_SetupDisplay;
  __xdm_LogError = dlfuncs->_LogError;
  __xdm_SessionExit = dlfuncs->_SessionExit;
  __xdm_DeleteXloginResources = dlfuncs->_DeleteXloginResources;
  __xdm_source = dlfuncs->_source;
  __xdm_defaultEnv = dlfuncs->_defaultEnv;
  __xdm_setEnv = dlfuncs->_setEnv;
  __xdm_putEnv = dlfuncs->_putEnv;
  __xdm_parseArgs = dlfuncs->_parseArgs;
  __xdm_printEnv = dlfuncs->_printEnv;
  __xdm_systemEnv = dlfuncs->_systemEnv;
  __xdm_LogOutOfMem = dlfuncs->_LogOutOfMem;
  __xdm_setgrent = dlfuncs->_setgrent;
  __xdm_getgrent = dlfuncs->_getgrent;
  __xdm_endgrent = dlfuncs->_endgrent;
  __xdm_getspnam = dlfuncs->_getspnam;
  __xdm_endspent = dlfuncs->_endspent;
  __xdm_getpwnam = dlfuncs->_getpwnam;
  __xdm_endpwent = dlfuncs->_endpwent;
  __xdm_crypt = dlfuncs->_crypt;
# ifdef USE_PAM
  __xdm_thepamhp = dlfuncs->_thepamhp;
# endif

  parse_config ();

  gtk_parse_args (0, NULL);
  gd = gdk_display_open (d->name);
  gdk_display_manager_set_default_display (gdk_display_manager_get (), gd);
  *dpy = GDK_DISPLAY_XDISPLAY (gd);

  if (!*dpy)
    {
      LogError ("Cannot reopen display %s for greet window\n", d->name);
      exit (3);
    }

  gdk_x11_grab_server ();

#ifdef USE_PAM
  pamhp = thepamhp ();

  pam_start ("xdm", NULL, &pc, pamhp);
  pam_set_item (*pamhp, PAM_USER_PROMPT, ">");

  if (d->name[0] != ':')
    {
      /* Displaying to remote host */
      gchar *hostname = g_strdup (d->name);
      gchar *colon = strrchr (hostname, ':');

      if (colon != NULL)
        *colon = '\0';

      pam_set_item (*pamhp, PAM_RHOST, hostname);
      g_free (hostname);
    }
  else
    pam_set_item (*pamhp, PAM_TTY, d->name);
#endif

  if (!cfg->auto_login)
    {
      GdkWindow *root = gdk_get_default_root_window ();

      /* load specific settings */
      if (cfg->rcfile)
        {
          if (g_file_test (cfg->rcfile, G_FILE_TEST_EXISTS))
            {
              gchar *rc_buf;
              if (g_file_get_contents (cfg->rcfile, &rc_buf, NULL, NULL))
                {
                  gtk_rc_parse_string (rc_buf);
                  g_free (rc_buf);
                }
            }
        }

      /* set cursor */
      gdk_window_set_cursor (root, gdk_cursor_new_for_display (gd, GDK_LEFT_PTR));

      /* create login window */
      w = create_dialog ();
      gtk_widget_show_all (w);

      gtk_widget_grab_default (w);
      gtk_widget_grab_focus (le);

      gdk_keyboard_grab (gtk_widget_get_window (w), FALSE, GDK_CURRENT_TIME);

      gtk_main ();

      gtk_widget_destroy (w);

      gdk_keyboard_ungrab (GDK_CURRENT_TIME);
      gdk_x11_ungrab_server ();

      Debug ("Greet loop finished\n");
    }
  else
    {
      uname = g_strdup (cfg->auto_user);
#ifdef USE_PAM
      pam_set_item (*pamhp, PAM_USER, uname);
#endif
    }

  if (!uname)
    return Greet_Failure;

  greet->name = uname;
  p = getpwnam (greet->name);
  endpwent ();

  verify->uid = p->pw_uid;
  verify->gid = p->pw_gid;

  if (d->session)
    verify->argv = parseArgs (NULL, d->session);
  else
    verify->argv = parseArgs (NULL, "Xsession");

  env = defaultEnv ();
  env = setEnv (env, "DISPLAY", d->name);
  env = setEnv (env, "HOME", p->pw_dir);
  env = setEnv (env, "LOGNAME", p->pw_name);    /* POSIX, System V */
  env = setEnv (env, "USER", p->pw_name);       /* BSD */
  env = setEnv (env, "PATH", cfg->path);
  env = setEnv (env, "SHELL", p->pw_shell);
  if (sess_cmd)
    env = setEnv (env, "SESSION_COMMAND", sess_cmd);
  verify->userEnviron = env;
  Debug ("user environment:\n");
  printEnv (verify->userEnviron);

  verify->systemEnviron = systemEnv (d, p->pw_name, p->pw_dir);
  Debug ("system environment:\n");
  printEnv (verify->systemEnviron);
  Debug ("end of environments\n");

  g_free (cfg);
  cfg = NULL;

  if (source (verify->systemEnviron, d->startup))
    {
      Debug ("Startup program %s exited with non-zero status\n", d->startup);
      SessionExit (d, 0, FALSE);
    }

  return Greet_Success;
}
