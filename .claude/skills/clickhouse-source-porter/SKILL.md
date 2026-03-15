---
name: clickhouse-source-porter
description: Use when porting new modules / features from the official ClickHouse sources into ported_clickhouse.
---

- Cloning the ClickHouse repo is super slow (several minutes) due to all of its git submodules
- ClickHouse SQL parsers live in the src/Parsers library in the ClickHouse repo
- Parsers include a lot of heavy transitive dependencies (e.g. on ClickHouse's common lib). Avoid
  bringing these into our ported lib, trimming and stubbing references as needed.
- Non-SQL parts of the ClickHouse parse (e.g. MySQL) are out of scope.
- Make sure all ported modules are included in Bazel targets and can be compiled.
- Add test queries to testdata for new parser features
- Ensure any ClickHouse Apache licensing headers, etc. are updated according to the existing patterns in this project.
