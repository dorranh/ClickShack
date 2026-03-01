# Ported ClickHouse Manifest

This manifest tracks files imported into `ported_clickhouse/*` and the pruning done to keep only parser-core requirements.

Porting rules for future agents:
- Copy only files required by the current compile target.
- Rewrite includes to `ported_clickhouse/...` paths.
- Prefer aggressive pruning with minimal shims over importing large dependency trees.
- Update this table for every newly copied or shimmed file, including a short prune summary.

Note: Imported files under `ported_clickhouse/*` should be treated as upstream-derived code and kept easy to re-sync.

| Local Path | Upstream Source Path | Reason Copied | Pruned Content Summary | Prune Risk | Dependent Targets | Last Synced |
|---|---|---|---|---|---|---|
| `ported_clickhouse/parsers/Lexer.h` | `src/Parsers/Lexer.h` | Core token definitions and lexer interface | Includes rewritten to local ported paths only | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/Lexer.cpp` | `src/Parsers/Lexer.cpp` | Lexer implementation | Includes rewritten; added standard `<string>/<string_view>`; normalized heredoc `string_view` construction to C++17-compatible form | Medium | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/LexerStandalone.h` | `src/Parsers/LexerStandalone.h` | Local fallback helper definitions referenced by lexer | No functional pruning; retained as upstream copy | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/clickhouse_lexer.h` | `src/Parsers/clickhouse_lexer.h` | Public C lexer bridge header for future FFI use | No functional pruning | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/TokenIterator.h` | `src/Parsers/TokenIterator.h` | Token stream abstraction used by parser base | Includes rewritten to local ported paths only | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/TokenIterator.cpp` | `src/Parsers/TokenIterator.cpp` | Parenthesis matching helper + iterator glue | Includes rewritten to local ported paths only | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/LiteralTokenInfo.h` | `src/Parsers/LiteralTokenInfo.h` | Literal token metadata map | Abseil include changed to quote form for current Bazel include behavior | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/IParser.h` | `src/Parsers/IParser.h` | Core parser interface and expected-state tracking | Includes rewritten; C++20 spaceship operator replaced with C++17 `operator<`; absl include quote-form | Medium | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/IParser.cpp` | `src/Parsers/IParser.cpp` | Parser depth/backtrack behavior implementation | Includes rewritten to local ported paths only | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/IParserBase.h` | `src/Parsers/IParserBase.h` | Shared parser base wrapper logic | Includes rewritten to local ported paths only | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/IParserBase.cpp` | `src/Parsers/IParserBase.cpp` | Base parse wrapper implementation | Includes rewritten to local ported paths only | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/parsers/IAST_fwd.h` | `src/Parsers/IAST_fwd.h` | AST pointer/vector forward declarations needed by parser interfaces | Aggressive prune: removed Boost intrusive ptr/vector usage and replaced with `std::shared_ptr`/`std::vector` | High | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/base/types.h` | `base/types.h` | Minimal integer aliases used by parser headers | Aggressive prune to only required fixed-width aliases | Medium | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/base/defines.h` | `base/defines.h` | Assertion macro required by lexer | Aggressive prune to `chassert` only | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/base/find_symbols.h` | `base/find_symbols.h` | Symbol scan templates used by lexer | Aggressive prune to two required template helpers | Low | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/core/Defines.h` | `src/Core/Defines.h` | Inline/branch prediction macros used by parser core | Aggressive prune to `ALWAYS_INLINE`, `NO_INLINE`, `likely`, `unlikely` | Medium | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/common/StringUtils.h` | `src/Common/StringUtils.h` | ASCII classification helpers used by lexer | Aggressive prune to minimal classification + number-separator helpers only | Medium | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/common/UTF8Helpers.h` | `src/Common/UTF8Helpers.h` | UTF-8 whitespace/continuation helpers referenced by lexer | Aggressive prune to minimal `skipWhitespacesUTF8` and continuation check | Medium | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/common/checkStackSize.h` | `src/Common/checkStackSize.h` | Stack-depth hook required by parser depth guard | Aggressive prune to no-op implementation | Medium | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/common/Exception.h` | `src/Common/Exception.h` | Exception type required by parser error handling | Aggressive prune to minimal runtime_error-backed exception with code field | High | `//ported_clickhouse:parser_core` | 2026-03-01 |
| `ported_clickhouse/common/ErrorCodes.cpp` | `src/Common/ErrorCodes.cpp` (symbols only) | Defines parser-required error code symbols | Aggressive prune to only `TOO_DEEP_RECURSION`, `LOGICAL_ERROR`, `TOO_SLOW_PARSING` | Medium | `//ported_clickhouse:parser_core` | 2026-03-01 |
