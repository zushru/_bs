# Git workflow

Canonical working tree: `/root/prototype/att`

Remote: `git@github.com:zushru/_bs.git` (`main`). The GitHub repository is
public. Never commit student data, private credentials, live attendance exports,
or any material that should not be publicly readable.

## Commit and push cadence

Create one focused commit per completed engineering increment. The tracked
`.githooks/post-commit` hook checks the number of commits ahead of
`origin/main` after each commit and pushes when the unpublished count reaches
five. A successful push resets the count. Push failures leave commits local
and visible; retry with `git push origin main` after resolving authentication or
network failure. Do not force-push or rewrite shared history.

The local checkout activates the tracked hook with:

```sh
git config core.hooksPath .githooks
```

New clones must run that command once. Hooks run only for commits made through
this local Git checkout; they do not run on GitHub edits or commits made in a
different clone unless its hooks path is configured.

## Public-repository exclusions

- `docs/sf2/reference/*.xlsx`: uploaded workbook copies are kept local because
  they may contain school/student information.
- `sketch/as608_readonly_web/`: this standalone diagnostic includes a fixed
  SoftAP credential and is kept local until that credential handling is
  redesigned.
- `comms/staging/`: local before/after snapshots and patches are retained as
  recovery material but not duplicated into the public Git history.
- `*.tmp.*`: transient editor/atomic-write scratch files are not source artifacts.

These files are not deleted or modified. To share any of them later, review
their contents and replace/remove sensitive material first, then deliberately
revise `.gitignore`.
