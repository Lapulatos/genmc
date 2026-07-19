# GenMC Ubuntu Server Deployment

## Result
Codex CLI 0.144.6 and the current GenMC development working tree are available on `lapulatos@frp-bid.com:26684`.

## Start Work

```bash
ssh lapulatos@frp-bid.com -p 26684
cd '/home/lapulatos/Documents/Codes/C++/GenMC/genmc'
codex
```

## Verified State
- Codex path: `/home/lapulatos/.local/bin/codex`
- Codex authentication: logged in with ChatGPT
- Branch: `genmc-caat`
- HEAD: `d963c49e45b82066ca9dcd036232c7384f6a95fe`
- Origin: `https://github.com/Lapulatos/genmc.git`
- Working-tree status: 47 tracked modifications and 69 untracked paths
- Source files: 6,267

## Intentionally Excluded
- `.codex-build-*` local build directories
- `experiment-analysis/`
- `optimization-analysis/`
- `.DS_Store`
- Local deployment-planning files

These excluded paths remain unchanged on the local machine.

## Network Limitation
At deployment time, the server could reach Ubuntu mirrors and npm, but direct requests to `chatgpt.com` and GitHub timed out. Codex was installed from npm and reports an active ChatGPT login; future `git fetch` or Codex model requests may still depend on the server's proxy/network configuration.

## Migrated Codex Skills
- Destination: `/home/lapulatos/.codex/skills`
- Files: 12,674
- Skills with `SKILL.md`: 55
- Previous remote skills backup: `/home/lapulatos/.codex/skills.before-local-migration-20260719`
- Transfer archive SHA-256: `86f4dfb694372672c357a19161c5c2528265ce63ef5b7390ce369d26f978a261`
