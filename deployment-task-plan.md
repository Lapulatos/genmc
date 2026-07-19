# Task Plan: Ubuntu Codex and GenMC Deployment

## Goal
Install Codex CLI on `lapulatos@frp-bid.com:26684` and deploy the current local GenMC working tree to `/home/lapulatos/Documents/Codes/C++/GenMC` without overwriting unknown remote data.

## Phases
- [x] Phase 1: Inspect local and remote state
- [x] Phase 2: Verify the current official Codex installation method
- [x] Phase 3: Install and verify Codex CLI on Ubuntu
- [x] Phase 4: Transfer the current GenMC working tree
- [x] Phase 5: Verify the remote source snapshot and write the deployment report
- [x] Phase 6: Migrate all local Codex skills and verify the remote copy

## Key Questions
1. Does the remote target directory already contain data?
2. Which local files must be excluded from a development-source deployment?
3. Can Codex start successfully on the remote host, and what authentication remains user-driven?

## Decisions Made
- Preserve the existing local `task_plan.md` and `notes.md`; use deployment-specific planning files because both existing files contain user changes.
- Deploy the local working tree rather than only cloning Git, because the working tree contains modified and untracked source files.
- Do not delete or overwrite an existing remote target directory without inspecting it first.

## Errors Encountered
- The official standalone installer stalled because the server could not reach `chatgpt.com`; installed npm from Ubuntu and then installed `@openai/codex` from the reachable npm registry.
- The first source snapshot accidentally included `.codex-build-*`; stopped the incomplete transfer and rebuilt a 30 MB source-only snapshot.
- Recursive SCP was too slow over the FRP link; compressed the same snapshot to 1.9 MB, transferred it with SCP, and extracted it remotely.
- The macOS tar archive created 7,540 AppleDouble `._*` metadata files; removed only those generated metadata files and verified 6,267 source files remained.
- Git refused to fetch directly into the checked-out empty branch; fetched into temporary refs, then restored the `genmc-caat` and `master` branch refs.
- Default SFTP-mode SCP stalled at a 255 KB transfer window for the 48 MB skills archive; retried with legacy protocol via `scp -O`, which completed successfully.

## Status
**Complete** - Codex CLI, the GenMC working tree and Git history, and all 55 local Codex skills are deployed and verified.
