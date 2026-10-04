<!--
Title in the imperative mood, describing the change. "Cull hidden faces with
word masks", not "Added culling" or "Update the mesher".
-->

## What changed

<!--
One or two sentences. A reader who has not read the issue should be able to
tell what moved. Do not restate the diff.
-->

## Why it was needed

<!--
The reasoning a reader cannot get from the code. What was broken, what was
missing, or what the change makes possible.
-->

Fixes #

## How it was verified

<!--
What you actually ran. Name the configuration and say what passed.

If a benchmark number is in here, all four of these go with it:

  terrain          the named generator and its parameters
  chunk size       the extent, and the padding
  machine          the CPU
  quad count       what the mesher emitted for that terrain

A timing on its own is not evidence.

If you did not run the build or the tests, say so here rather than implying you
did. "Not run, the change is comments only" is a complete and acceptable answer.
-->

- [ ] Builds clean in the configuration CI uses, Release included
- [ ] New tests pass, existing tests still pass
- [ ] ASan and UBSan clean
- [ ] Debug and Release both pass
- [ ] No new warnings, and none suppressed to get there
- [ ] New public symbols appear in the README
- [ ] Quads per chunk reported, if the mesher changed
- [ ] The new code has a test that fails without it

## Trade-offs and anything left undone

<!--
What you chose not to do, and what a reviewer should push back on. A blank
section here is a signal, so if there is nothing, say "none".
-->