# Registration architecture

`PluginRegistration.h` owns the transactional lifetime mechanism used by `initializePlugin` and `uninitializePlugin`.

Files under `modules/` are include-only registration manifests. They deliberately contain statements rather than standalone translation units because the legacy node classes are still declared through the existing aggregate headers in `pluginMain.cpp`. Keeping each feature family in a separate manifest makes the active registration set reviewable without changing Maya 2019 symbol or link behavior.

Registration rules:

- Cleanup storage is prepared before any node, command, data, draw override,
  callback, or subsystem operation can mutate host/native state. Allocation
  failure in bookkeeping therefore cannot strand a successfully registered item.
- The transaction records the first registration failure even when a legacy manifest continues after a `CHECK_MSTATUS` call; `commit()` then refuses to accept a partial registration set.
- A failure during initialization triggers reverse-order rollback through the transaction destructor.
- The process lifetime owns cleanup records from the beginning; failed-load
  rollback also retains failed removals for retry. No list transfer is needed at
  commit, and a nonempty lifetime cannot be silently overwritten.
- Unload attempts every cleanup action even after a failure and retains only
  failed actions for a possible retry. Internal compaction does not allocate.
- A failed host registration removes only its staged record: a duplicate-ID
  failure must not deregister somebody else's node. Subsystem setup and throwing
  operations keep cleanup because setup may have partially succeeded.
- Callback IDs are captured by the checked factory; partial solver callback
  setup never erases IDs before successful removal. Retrying plug-in initialization
  first attempts any retained cleanup and aborts if it still fails.
- Syntax-bearing commands use `registerCommandWithSyntax<Command>()`. Checked
  syntax preflight runs before registration. A syntax callback failure after
  successful native registration retains cleanup and fails the transaction.
  Later callback failures latch that command closed until successful preflight
  on registration; command execution must use the shared checked syntax builder.
- Dependency-graph file writers are excluded unless
  `DOMMY_WITH_DG_FILE_WRITERS` is enabled in addition to the owning integration.
  This keeps filesystem side effects outside the default registration surface.

Pointer factories preserve host ownership on success and return null on a
guarded allocation/constructor failure. Syntax factories return a Maya value,
not `MStatus`: outer function-try blocks cover plug-in-owned copies and a
recoverable empty fallback, but persistent SDK value-construction/copy failure
cannot be fully recovered through that ABI. Do not add `noexcept` to these value
callbacks or claim that host-free fixtures prove Maya's failure recovery.
