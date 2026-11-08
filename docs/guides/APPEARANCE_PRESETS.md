# Appearance presets

START → Appearance provides three named slots: Everyday, Evening and Travel.
Use Left/Right on Preset slot, then Save appearance or Load appearance. These
are user snapshots, not predefined visual styles; an empty slot reports that it
is unavailable. Saving retains the previous revision as `.bak`. Loading checks
the complete record before changing the current appearance, and can recover a
valid backup when the current record is malformed.

A preset includes the theme ID, accent override, reduced motion, icon size,
browser view, cover visibility, clock format and independent wallpaper paths
and strength. It leaves navigation, associations, game options, saves, music
selection and volumes alone. Missing optional theme assets follow the normal
built-in fallback. Loading marks ordinary preferences unsaved until the normal
save succeeds. Recovery sessions cannot save, load or reset appearance.

Files are `_nds/dsi-frontend/appearance-1.ini` through `appearance-3.ini`.
Paths use `sd:/` references. Names identify the three slots; renaming a `name=`
field does not rename the on-console slot. Computer configuration exports
include these records. Reset appearance restores the built-in theme and view
choices without deleting presets, packages or source images.
