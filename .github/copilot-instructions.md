# GitHub Copilot Instructions

## PR Review Checklist (CRITICAL)
<!-- KEEP THIS SECTION UNDER 4000 CHARS - Copilot only reads first ~4000 -->

### DIS API Patterns (Do Not Flag)

- **dis_version() modifying state**: This is intentional. The original DOS DIS API was designed so dis_version() both initializes state AND returns version. From original docs: "This routine must be called at start of each demo part, it will clear the exitkey flag and do other stuff as well." Do not suggest renaming or separating initialization.

- **Original API compatibility**: All DIS functions match the original DOS DIS/DIS.H signatures and semantics exactly. Do not suggest API changes that would break compatibility with original demo parts.

## Code Style

- C99 standard
- Function comments use doxygen-style `/** */`
- Error messages format: Each subsystem uses its own prefix (e.g., "DIS: ERROR ...", "PART: ERROR ...", "VIDEO: ERROR ..."). Do not suggest changing PART: to DIS: - the part loader is a separate subsystem.
