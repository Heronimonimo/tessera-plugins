# Translations

A plugin's words live in `translations/<language>.json`, in two parts, like Tessera's own texts:

```json
{
  "_meta": { "name": "Nederlands" },
  "screen": {
    "now": "nu",
    "minutes": "{n} min",
    "days": "1 dag | {n} dagen"
  },
  "app": {
    "name": "Openbaar vervoer (NL)",
    "summary": "Wanneer de volgende bus, tram, metro of veerboot bij jouw halte vertrekt.",
    "tile_name": "Eerstvolgende vertrek",
    "stop": "Haltecode"
  }
}
```

| Part | Read by | Language |
|---|---|---|
| `screen` | The plugin's C++, with `plugin->text("key")`. | The language the screen is built in. The build takes that file's texts and English for what it lacks. |
| `app` | The Tessera editor: the plugin's name and summary, the names, labels and hints the manifest names. | The editor's language, else English, and the page says the plugin is in English only. |

## The rules

- **English is complete.** `translations/en.json` has every key of `app` the manifest names, plus `name` and
  `summary`. `tools/check.py` fails without one.
- **Other languages may be partial.** A missing key falls back to English. A key in another language that English does
  not have in `screen` is a mistake, and the check says so.
- **The file name is the language**: `en`, `nl`, `de`, `fr`, `es`, `it`, `pt`, `pl`, `hu`, and `pt-BR` style for a
  regional variant (it falls back to `pt`, then English).
- **Placeholders are named**: `{n}`, `{list}`, `{name}`. A language puts them where its sentence needs them; never use
  a translation as a printf format.
- **Plurals** are forms separated by `|`, in the order of the language's plural rule (English and Dutch: one, other;
  Polish: one, few, many). `tessera::format(plugin->text("days"), n)` picks the form and fills in `{n}`.
- **Only letters the screen has.** The screen's fonts carry Latin letters with the accents of Tessera's languages,
  digits and common signs. Emoji and other scripts show as nothing.
- **Short on the screen.** A tile's second line on a 2.8-inch screen holds about 14 characters. Test the longest
  language on the smallest tile size.

## The README

`README.md` is English. `README.nl.md`, `README.de.md` and so on are optional; the editor shows the one in its language,
else the English one.

## Translating someone's plugin

A translation is a pull request on the plugin's repository: add `translations/<language>.json` (and a README if you
like). The app sees it with the next release of the plugin.
