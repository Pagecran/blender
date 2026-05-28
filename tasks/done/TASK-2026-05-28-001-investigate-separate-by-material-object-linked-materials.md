---
task_id: TASK-2026-05-28-001
title: Investigate Blender Separate by Material object-linked material behavior
complexity: standard
track: investigation
slice: logic
status: done
discussion: DISCUSSION-001
issue_url: https://github.com/Pagecran/blender/issues/42#issue-4524674835
---

# Task: Investigate Blender Separate by Material object-linked material behavior

## Request

Product Owner asks, in French: "est-ce que tu peux regarder dans le code source de blender ce qu'il y a derriere cette commande de l'UI et pourquoi ça ne fonctionne pas si les materiaux sont appliqué au niveau objet plutot que data ?"

Issue summary: In Blender, when using **Separate by Material**, separated objects no longer have materials assigned in their material slots if the source material slot is linked to the object rather than to mesh data.

## Classification

- Complexity: `standard`
- Track: `investigation`
- Slice: `logic`

## Acceptance Criteria

- AC-1: Identify the UI/operator command behind **Separate by Material**.
- AC-2: Identify the relevant source files/functions involved in material-based separation.
- AC-3: Explain why object-linked material slots are not preserved or assigned after separation.
- AC-4: Provide a concise recommendation for where a future fix should be made, without implementing code.
- AC-5: Respond in French with file/function references and any caveats.

## Discussion Record

- 2026-05-28: User asked for source-level investigation of GitHub issue #42 concerning **Separate by Material** and materials linked at object level rather than mesh data level.

## Instructions For Assigned Specialist

- Read this entire task file first.
- This is investigation only: do not modify source code.
- Follow CodeMap-first navigation if CodeMaps exist; if absent, use targeted source search.
- Inspect the issue URL context if needed.
- Return findings with the Specialist Output Contract sections.

# Closure

- 2026-05-28: Investigation completed and converted into implementation task `TASK-2026-05-28-002`.
