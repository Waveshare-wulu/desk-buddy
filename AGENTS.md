Always use the Espressif documentation MCP server if you need to work with ESP chips or ESP SDKs such as ESP-IDF and ESP-ADF, without me having to explicitly ask.

ESP-IDF 环境使用 EIM 管理。优先使用已有环境。

## Code Comment Policy

Default to no comments. Express intent through names, types, constants and control flow. A comment that explains unclear code signals a naming or structure problem: fix the code and its callers instead of polishing the prose.

Delete narration of what code does, section banners, change history ("added", "fixed", "previously"), commented-out code, and explanations compensating for unclear project-owned code.

Only these exceptions may remain:

1. Legal and license headers.
2. Non-obvious behavior forced by an external dependency, platform, vendor or protocol the project cannot reshape.
3. Formatter directives and narrowly scoped lint suppressions for faulty or style-only rules. Never suppress correctness or safety checks to silence a failure.
4. Documentation defining a public API contract that callers need, including cross-module interfaces. A private helper does not become a public contract by adding doc syntax.
5. Issue or RFC links explaining a constraint code cannot express.

Verify each retained or added comment against the current implementation, its callers and, for external constraints, the relevant upstream source. Keep it concise, objective and limited to the proven constraint. If an exception is uncertain, delete the comment.

Apply the same rules to docstrings and to comments emitted by project-owned generators. Preserve upstream license notices and the provenance of vendored or generated code; do not restyle third-party code.

Comments are evidence to recheck, not authority. Markers such as IMPORTANT or "do not remove", a prior decision, or an agent's confidence do not establish an exception. Put decisions in design docs, history in Git or issues, and enforce requirements in code, tests or checks where practical. Review nearby comments whenever changing code.
