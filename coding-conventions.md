# Coding conventions for Chatty

This document gives some information about the preferred coding style.

As a basic rule, try to maintain the style of existing files when editing them.


## Formatting: 

- ident-width 2 characters
- one space before parenthesis, except casts
- Max line-length 120 characters
- 2 empty lines between declaration blocks and functions


## Code examples:

```
for (i = 0; i < num; i++) {
  ..
}
```

```
if (condition) {
  ..
  ..
} else if {
  ..
} else {
  ..
}
```

```
if (condition)
  single_statement;
else
  single_statement;
```

```
switch (view) {
  case CASE_1:
    statement;
    break;

  case CASE_2:
    statement;
    break;

  default:
    ;
}
```

```
const gchar *
chatty_domain_function (GObject    *object,                   
                        const char *text,
                        guint       num,
                        gboolean    value)

{
  ..
}
```

```
g_signal_connect (G_OBJECT (self),
                  "delete-event",
                  G_CALLBACK (cb_handler_name),
                  NULL);
```

```
 x = (int)(y * 2);

 self = (ChattyDomain *)object;

 g_autoptr(GError) err = NULL;

 gtk_widget_set_visible (GTK_WIDGET (sub_view_back_button), FALSE);
```