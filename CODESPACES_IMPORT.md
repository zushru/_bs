# Import v0.2.48 into GitHub Codespaces

The current GitHub `main` branch is an older snapshot. Do not overwrite it directly. Import this release into a dedicated branch first.

## One-time GitHub browser steps

1. Open `https://github.com/zushru/_bs` and sign in.
2. Create a new branch from `main` named `codespaces/v0.2.48`.
3. Switch to that new branch.
4. Use **Add file → Upload files** and upload `att_v0.2.48_codespaces.tar.gz` to the repository root. Commit the upload to this branch.
5. From the branch page choose **Code → Codespaces → Create codespace on codespaces/v0.2.48**. Use a 2-core machine. At this first start GitHub will use its default container because the archive's `.devcontainer` has not yet been extracted.

## Import commands

In the initial Codespace terminal, run from the repository root:

```sh
mkdir -p /tmp/att-v0248

tar -xzf att_v0.2.48_codespaces.tar.gz -C /tmp/att-v0248

find . -mindepth 1 -maxdepth 1 \
  ! -name .git ! -name att_v0.2.48_codespaces.tar.gz \
  -exec rm -rf -- {} +

cp -a /tmp/att-v0248/. .
rm -rf /tmp/att-v0248 att_v0.2.48_codespaces.tar.gz

chmod +x .devcontainer/post-create.sh tools/codespace-*.sh \
  tools/inspect_probe_wrappers.sh tools/test_inspect_probe_wrappers.sh

git add -A
git commit -m "Import attendance device development snapshot v0.2.48"
git push -u origin HEAD
```

Then open the Codespace command palette and run **Codespaces: Rebuild Container**. When setup finishes:

```sh
./tools/codespace-doctor.sh
./tools/codespace-test.sh
```

The import replaces the source tree on this branch but preserves `.git` and the branch history. It does not touch `main`. If you had independent changes in this temporary branch, inspect `git status` before the import/commit.
