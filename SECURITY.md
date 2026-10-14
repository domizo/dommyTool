# Security policy

This repository is a historical source archive, not an actively supported Maya release. Do not load untrusted Maya scenes, MEL/Python scripts, plug-in binaries, VDB files, or cache files into a production workstation solely because they are referenced by this repository. Use isolated, disposable test scenes and directories when evaluating the tools.

Credentials must be supplied through environment variables described in `.env.example`. Never commit real `.env` files, API keys, SMTP passwords, tokens, or local databases.

Optional SMTP notifications verify both the server certificate and hostname using
the runtime's default trusted CA store. Verified TLS capability is required; there
is no unverified fallback. Do not disable verification to work around an obsolete
Maya SSL runtime or missing CA roots. The injectable SMTP factory is a trusted
testing boundary, not a setting for accepting untrusted transport code. Partial
delivery is reported and must not trigger a blanket automatic resend.

Run `./tests/check_publication_readiness.sh` before publishing source changes. It checks for known credential formats, private-key material, local paths and unwanted artifacts, reporting filenames rather than suspected secret values. Pattern checks are not proof that all sensitive information is absent. Review new files and diagnostic output manually as well; reports can contain scene names, file paths or workstation details.

The portable checks reject newly introduced project-owned calls to shell execution and unbounded C-string APIs, and reject default-deleting smart pointers around Maya-owned plug-in data. These checks reduce known risks but do not replace runtime testing of parsers, scene loading, callbacks, or renderer integrations.

The Alembic export command retains its historical MEL and Python callback flags. Those flags execute the command text supplied by the caller and must be treated as an explicit trusted-code boundary. Do not copy callback strings from untrusted scenes or scripts into an export command; the size and encoding checks in the exporter are defensive limits, not a sandbox.

When private vulnerability reporting is enabled on the public GitHub repository, use it for suspected vulnerabilities. Include the affected file, Maya version, reproduction steps, and whether a crafted scene or data file is required. Avoid publishing credentials or a working exploit in a public issue. This inactive archive does not promise security updates or a response timeline.
