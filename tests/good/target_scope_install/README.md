# target_scope_install

When install is explicitly requested, its affected phony input makes install affected, so both a and b are required. Bootstrap logs install too, so this exercises propagation rather than a missing log entry.
