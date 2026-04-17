# Snapshot: {{NAME}}

> **Type:** snapshot  
> **Created:** {{TIMESTAMP}}  
> **Project:** ToolScanIP  
> **Status:** {{STATUS}}  <!-- active | forked | superseded -->

---

## 1. Original Requirements

<!-- What the user asked for — paste verbatim or summarise. -->

{{REQUIREMENTS}}

---

## 2. Technical Decisions

<!-- Key choices made and the reasoning behind them. -->
<!-- Format: "Decision: X  →  Reason: Y  →  Alternatives rejected: Z" -->

{{DECISIONS}}

---

## 3. Files Modified / Created

<!-- List every file touched, with a one-line summary of the change. -->
<!-- Format: `path/to/file` — what changed -->

{{FILES}}

---

## 4. Code Snapshots

<!-- Paste the most important diffs or new code blocks here.
     Keep enough context that Claude can understand without reading
     the full file. -->

{{CODE}}

---

## 5. Pending Questions / Unknowns

<!-- Things that still need to be resolved before this feature ships. -->

{{QUESTIONS}}

---

## 6. Planned Next Steps

<!-- Ordered list of what comes next. -->

{{NEXT_STEPS}}

---

## 7. Constraints & Rules

<!-- Hard rules the implementation must follow
     (e.g., "must compile on armhf", "no root at runtime"). -->

{{CONSTRAINTS}}

---

## 8. How to Restore This Context

To resume this conversation from this snapshot, say:

```
Load snapshot "{{NAME}}" and continue from the planned next steps.
```

To fork with a change, say:

```
Fork from snapshot "{{NAME}}" but <describe the change>.
```
