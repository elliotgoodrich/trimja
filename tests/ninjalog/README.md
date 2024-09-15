# ninjalog

We want to make sure that we read from `.ninja_log` correctly.

The main points we need to cover are:

  1. Build edges not found in `.ninja_log` are always included as this indicates
     that they have been newly added to the ninja build file.
  2. Build edges with a hash mismatch are included as this indicates that the
     build command has changed.

