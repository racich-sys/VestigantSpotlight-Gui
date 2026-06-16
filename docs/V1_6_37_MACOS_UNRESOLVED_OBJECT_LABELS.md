# V1.6.41.1 macOS unresolved Store-V2 object labels

V1.6.41.1 confirmed external Store-V2 dictionary loading and native path-probe promotion, but the same macOS thin showed that most first-page GUI artifacts were still displayed as `------NONAME------` because the safe Store-V2 decoder cannot yet resolve structured names for many records.

V1.6.41.1 does not claim those unresolved records have real filenames. Instead, artifacts without a usable name/path now receive an explicit, non-evidentiary review label:

- `file_name = UNRESOLVED_SPOTLIGHT_OBJECT_INODE_<inode>`
- `display_name = Unresolved Spotlight object inode=<inode>`
- `path_source = RAW_RECORD_UNRESOLVED_IDENTIFIER_LABEL`
- `path_status = UNRESOLVED_NATIVE_STOREV2_OBJECT_IDENTIFIER_LABEL`
- `confidence = LOW_HEADER_ONLY_IDENTIFIER_LABEL`

This is a GUI/review usability improvement and forensic transparency marker, not a substitute for future full structured Store-V2 value decoding.
