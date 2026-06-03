"""Platform transition to build process_wrapper for linux-musl.

Produces a fully static binary that is safe to remote-cache across hosts
with different glibc versions. The exec-platform cache key does not encode
glibc version, so without this, a binary built on Ubuntu 24.04 (glibc 2.39)
poisons BuildBuddy and crashes on Debian 12 (glibc 2.36).

@@// refers to the root workspace (i9-io/dott), which owns the musl platform
definition. This is intentional — this fork is i9-specific.
"""

def _musl_transition_impl(settings, attr):
    return {"//command_line_option:platforms": "@@//platforms:linux_arm64_musl"}

_musl_transition = transition(
    implementation = _musl_transition_impl,
    inputs = [],
    outputs = ["//command_line_option:platforms"],
)

def _static_process_wrapper_impl(ctx):
    executable = ctx.attr.binary[0][DefaultInfo].files_to_run.executable
    return [DefaultInfo(
        executable = executable,
        files = depset([executable]),
        runfiles = ctx.runfiles(files = [executable]),
    )]

static_process_wrapper = rule(
    implementation = _static_process_wrapper_impl,
    attrs = {
        "binary": attr.label(
            cfg = _musl_transition,
            executable = True,
            mandatory = True,
        ),
    },
    executable = True,
)
