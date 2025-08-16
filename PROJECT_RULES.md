# Project Development Rules - Haiku OneDrive Client

## 🔒 MANDATORY RULES FOR ALL DEVELOPMENT SESSIONS

### Rule 1: Documentation After Every Step (CRITICAL)
**Every single code change, decision, or action MUST be documented immediately.**

1. **SESSION_LOG.md**: Update after EVERY action with:
   - Timestamp
   - Action taken
   - Files modified/created
   - Decisions made and rationale
   - Problems encountered
   - Solutions implemented
   - Next steps identified

2. **Project State Persistence**: After each step, update:
   - `.claude/project_state.json` - Machine-readable current state
   - `PROGRESS.md` - Human-readable progress tracking
   - Component-specific documentation in relevant directories

3. **Code Documentation**: 
   - EVERY file must have a header comment explaining its purpose
   - EVERY function must have documentation
   - EVERY significant code block must have explanatory comments
   - Use Doxygen format for all documentation

### Rule 2: State Management Between Sessions
**Project must be resumable by any developer (including AI) at any time.**

1. **Before Ending Any Work Session**:
   - Run `git add -A && git commit -m "WIP: [description]"`
   - Update SESSION_LOG.md with session summary
   - Update .claude/project_state.json with current state
   - Document any blocking issues or dependencies

2. **When Starting Any Work Session**:
   - Read SESSION_LOG.md to understand previous work
   - Check .claude/project_state.json for current state
   - Review any TODO items or blocking issues
   - Continue from exact stopping point

### Rule 3: Incremental Development Approach
**Build small, test often, document always.**

1. **Development Cycle**:
   - Plan step (document in SESSION_LOG.md)
   - Implement step (small, focused changes)
   - Test step (verify it works)
   - Document step (update all tracking files)
   - Commit step (git commit with descriptive message)

2. **Never Skip Documentation**:
   - Even for "trivial" changes
   - Even for "temporary" code
   - Even for "obvious" decisions

### Rule 4: File Organization Standards
**Consistent structure for predictable navigation.**

```
/Storage/Development/OneDrive/
├── PROJECT_RULES.md          # This file - development rules
├── SESSION_LOG.md           # Detailed session-by-session log
├── PROGRESS.md              # High-level progress tracking
├── ARCHITECTURE.md          # System architecture decisions
├── .claude/
│   ├── project_state.json  # Current state for AI continuation
│   ├── dependencies.json   # External dependencies tracking
│   └── blockers.json      # Current blocking issues
├── docs/                   # User-facing documentation
├── src/                    # Source code
└── tests/                  # Test files
```

### Rule 5: Git Commit Standards
**Every commit must be meaningful and traceable.**

1. **Commit Message Format**:
   ```
   [Component] Action: Brief description
   
   - Detailed change 1
   - Detailed change 2
   
   Session: [Session ID]
   State: [WIP|STABLE|TESTED]
   Next: [What needs to be done next]
   ```

2. **Commit Frequency**:
   - After each completed step
   - Before any major changes
   - At least every 30 minutes during active development
   - ALWAYS before ending a session

### Rule 6: Testing and Validation
**No code without verification.**

1. **Before Moving to Next Step**:
   - Verify current code compiles (if applicable)
   - Run any existing tests
   - Document test results in SESSION_LOG.md
   - Fix any issues before proceeding

2. **Test Documentation**:
   - Document what was tested
   - Document how it was tested
   - Document test results
   - Document any issues found

### Rule 7: Dependency Tracking
**Every external dependency must be documented.**

1. **When Adding Dependencies**:
   - Document in .claude/dependencies.json
   - Include version requirements
   - Include installation instructions
   - Include fallback options

### Rule 8: Error Handling and Recovery
**Plan for failure, document for success.**

1. **When Errors Occur**:
   - Document exact error message
   - Document steps that led to error
   - Document attempted solutions
   - Document final resolution
   - Update blockers.json if unresolved

### Rule 9: Architecture Decision Records (ADR)
**Every significant decision must be recorded.**

1. **Create ADR for**:
   - Technology choices
   - Design patterns
   - API designs
   - File structures
   - Algorithm selections

2. **ADR Format**:
   ```markdown
   # ADR-[number]: [Title]
   
   ## Status
   [Proposed|Accepted|Deprecated|Superseded]
   
   ## Context
   What is the issue we're addressing?
   
   ## Decision
   What have we decided to do?
   
   ## Consequences
   What are the implications?
   
   ## Alternatives Considered
   What other options were evaluated?
   ```

### Rule 10: Continuous Integration Mindset
**Always leave the project in a better state than you found it.**

1. **Before Starting Work**:
   - Ensure project builds
   - Review recent changes
   - Understand current state

2. **After Completing Work**:
   - Ensure project still builds
   - Update all documentation
   - Leave clear next steps

## 🚨 ENFORCEMENT

**These rules are NON-NEGOTIABLE. Any session that doesn't follow these rules should be considered incomplete and potentially harmful to project continuity.**

### Validation Checklist (Run after EVERY step):
- [ ] SESSION_LOG.md updated?
- [ ] Project state files updated?
- [ ] Code changes documented?
- [ ] Git commit created?
- [ ] Tests passing (if any)?
- [ ] Next steps documented?

### Session Handoff Checklist:
- [ ] All changes committed?
- [ ] SESSION_LOG.md has session summary?
- [ ] Blockers documented?
- [ ] Dependencies documented?
- [ ] Project builds successfully?
- [ ] Clear next steps provided?

---
*These rules ensure project continuity across sessions and developers. Failure to follow these rules will result in lost work and confusion.*

*Last Updated: [Session start timestamp]*
*Version: 1.0.0*