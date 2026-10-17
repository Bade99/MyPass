# Coding style

## Names

| Kind | Style | Example |
| --- | --- | --- |
| Project-defined classes, structs, enums, and concepts | `PascalCase` | `TextLineLayout`, `SortOption` |
| Functions, methods, namespaces, variables, and fields | `snake_case` | `create_text_layout`, `line_index` |
| Enum values and constants | `snake_case` | `SortOption::alphabetic`, `max_delay_ms` |
| Macros and resource identifiers | `UPPER_SNAKE_CASE` | `WM_SETDEFAULTTEXT` |
| Template type parameters | `PascalCase` | `template<typename Value>` |

Use lowercase abbreviations within `snake_case` names, such as `ime_composition` and `dpi_scale`. Avoid leading underscores in project-defined identifiers.

Keep short foundational types and aliases such as `u32`, `i64`, `f32`, `str`, `utf16`, `text`, `v2`, and `rect_i32` as they are. Keep Windows SDK names and required callback or interface method names unchanged.

Renaming a C++ identifier must not change serialized field names, resource IDs, window messages, or other externally stored values. Preserve those values explicitly when changing the corresponding code.

Apply these rules to new code and normalize existing names in focused batches with a build after each batch.
