# Notes: Ubuntu Codex and GenMC Deployment

## Local Source
- Path: `/Users/sujie/Documents/Codes/C++/GenMC/genmc`
- Branch: `genmc-caat`
- HEAD before deployment: `d963c49e45b82066ca9dcd036232c7384f6a95fe`
- The working tree has tracked modifications and untracked source files; a plain remote clone would not reproduce it.

## Safety Boundaries
- Exclude `.git`, local build directories matching `.codex-build-*`, and `.DS_Store` from transfer.
- Inspect `/home/lapulatos/Documents/Codes/C++/GenMC` before creating or synchronizing it.
- Codex account authentication is interactive and must not be fabricated or copied without explicit authorization.

## Remote Findings
- Ubuntu 24.04.4 LTS, x86_64.
- Target parent existed and contained historical projects, but `GenMC/genmc` did not exist.
- About 230 GB was free on `/home` before deployment.
- Direct access to `chatgpt.com` and GitHub timed out, while Ubuntu mirrors and the npm registry were reachable.

## Installed State
- npm: 9.2.0, installed with Ubuntu apt.
- Codex: `codex-cli 0.144.6` at `/home/lapulatos/.local/bin/codex`.
- Authentication check: `Logged in using ChatGPT`.

## Deployed State
- Source path: `/home/lapulatos/Documents/Codes/C++/GenMC/genmc`.
- Source files: 6,267.
- Git branch: `genmc-caat`.
- Git HEAD: `d963c49e45b82066ca9dcd036232c7384f6a95fe`.
- Git remote: `https://github.com/Lapulatos/genmc.git`.
- Working-tree status: 116 lines, matching the filtered local source status (47 tracked modifications and 69 untracked paths).
- Source archive SHA-256 matched before extraction: `d65f0ce6177007bf71f0413391105b0f7c4e8719e010e5f00c64139f0474b121`.
- `git fsck --connectivity-only` passed.
- A representative new test script retained executable permission.

## Preserved Incomplete Attempts
- Remote: `/home/lapulatos/Documents/Codes/C++/GenMC/genmc.incomplete-20260719`.
- Remote: `/home/lapulatos/Documents/Codes/C++/GenMC/genmc.incomplete-scp-20260719`.
- These contain only partial files created by this deployment and were preserved instead of deleted.

## Codex Runtime Network Check
- A read-only `codex exec` started with Codex 0.144.6 and the configured ChatGPT login.
- The request timed out because the server could not reach `https://chatgpt.com/backend-api/ps/mcp`.
- Installation and local configuration are valid; interactive model use still requires working outbound access or a proxy.

## Skills Migration Baseline
- Local `/Users/sujie/.codex/skills`: 105 MB, 12,674 files, 55 `SKILL.md` files.
- Existing remote `/home/lapulatos/.codex/skills`: 576 KB, 50 files, 5 `SKILL.md` files.

## Skills Migration Result
- Remote destination: `/home/lapulatos/.codex/skills`.
- Existing remote backup: `/home/lapulatos/.codex/skills.before-local-migration-20260719`.
- Migrated files: 12,674, exactly matching local.
- Migrated `SKILL.md` files: 55, exactly matching local.
- Archive SHA-256 matched: `86f4dfb694372672c357a19161c5c2528265ce63ef5b7390ce369d26f978a261`.
- Required `expression-skill` and `planning-with-files` files are present.
- AppleDouble metadata files after extraction: 0.
