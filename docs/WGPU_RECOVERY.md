# wgpu baseline and recovery

## Preserved source

Repository: Trilec/upp_render; branch main.
Clean baseline after fetching origin/main:
0999308ad1b7e2f60789356b7923fb93955c5480.

Verified Git bundle on the Curt machine:
E:/apps/github/upp_render/build/render-before-wgpu-2026-10-08.bundle
Size: 938132 bytes.
SHA-256: 97e1b69a2aec889377fcb2e96fc5686e2eb329b5ee1b1a23e4f177ba283553b0.

Git bundle create job: exec-67AB74C484C9F3F9161114EF72798D08.
Git bundle verify job: exec-EA1FBEC15FD531262D2A363DCBEA69CE.
Verification reported complete history; all available refs were included.
Filename uses the remote execution machine's date.

The bundle contains tracked Git history/refs. It does not contain ignored build
outputs, installed SDKs, ignored local settings, or new uncommitted design files.
It resides on the same disk: a verified recovery copy, not an off-machine backup.
Keep existing release manifests, earlier branch bundles and Patch journals.
No legacy renderer source has been deleted or replaced at this checkpoint.

## Recover without overwriting current work

First inspect Git status and the Patch recovery state. Save new work before any
restoration. Do not blindly replay a lost Patch apply or delete a journal lock.

Verify the bundle and its recorded SHA-256. Restore into an absent sibling
directory, rather than resetting the active checkout:

```text
git bundle verify E:/apps/github/upp_render/build/render-before-wgpu-2026-10-08.bundle
git clone E:/apps/github/upp_render/build/render-before-wgpu-2026-10-08.bundle E:/apps/github/upp_render-recovered
git -C E:/apps/github/upp_render-recovered switch main
git -C E:/apps/github/upp_render-recovered rev-parse HEAD
```

The restored main should report the baseline commit above. Restore build settings
and dependencies from the relevant manifests; do not assume ignored outputs came
from that source. A new sibling workspace needs its own local Klick grant before
remote inspection or execution. Never run a destructive reset to make a dirty
checkout match this document.

## Resume new work

Read ACTIVE_WORK.md, WGPU_ARCHITECTURE.md and WGPU_IMPLEMENTATION_PLAN.md.
Resolve the current KlickCurt identity and Render project context.
Read Git status/diff and retained Execute/Patch records before continuing.
Architecture documents are a design baseline, not evidence of a running wgpu
renderer. Preserve each successful dependency/build identity and update the
checkpoint with measured results. Commit/publication remain separate actions.
