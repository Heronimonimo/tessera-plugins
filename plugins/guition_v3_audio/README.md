# Days until

A tile that counts the days until a date you choose: holidays, a birthday, the end of a project.

This is the template for a Tessera plugin. It is complete and it works: put it on a screen to see how a plugin behaves,
then make it your own with `python3 tools/new_plugin.py <your_id>` from the root of this repository. The docs in
`docs/` explain every file; start with `docs/MAKING_A_PLUGIN.md`.

## Set up

1. Add the plugin to a screen in Tessera. The screen builds once.
2. In Layout, place the **Days until** tile from the library's Plugins group.
3. In the inspector, fill in the date (year-month-day, such as `2026-12-25`) and a few words for under the number.

## Good to know

- The tile counts on the screen's own clock and changes at midnight.
- It uses nothing outside the screen: no Home Assistant entity and no internet service.
