# Codex Workstream: Rust Native Bridge

## Goal

Port the native bridge toward Rust while preserving the current external contract:

- binary name: `mwb-client`
- config path: `~/.config/mwb-linux-bridge/config`
- config keys: `windows_ip`, `host`, `port`, `security_key_file`
- key source order: `MWB_SECURITY_KEY`, `security_key_file`, hidden prompt
- default port: `15101`

## Worktree

`../mwb-codex-rust-core`

## Branch

`agents/codex-rust-core`

## Owned Paths

- `Cargo.toml`
- `Cargo.lock`
- `rust/`
- `src-rust/`
- Rust-specific build/package wiring

## Tasks

1. Add a Rust crate for the native bridge.
2. Port protocol structs with explicit byte parsing/serialization instead of raw pointer casts.
3. Port crypto compatibility:
   - PBKDF2-HMAC-SHA512
   - 50,000 iterations
   - UTF-16LE fixed salt derived from `18446744073709551615`
   - AES-256-CBC streaming mode
   - fixed IV `1844674407370955`
   - no padding
   - PowerToys 24-bit hash behavior
4. Port config parsing and CLI compatibility first.
5. Port uinput/input mapping after protocol/config tests are passing.
6. Keep the existing C++ implementation as the reference until Windows interoperability is verified.

## Constraints

- Do not edit `tui/` except if a Rust binary path change absolutely requires it.
- Do not remove C++ source.
- Do not change the config file format.
- Do not pass the security key as a command-line argument.
- Do not claim Windows interoperability until tested against PowerToys.

## Suggested Rust Dependencies

Prefer conservative crates:

- `anyhow`
- `clap`
- `openssl` or RustCrypto equivalents if compatibility is confirmed
- `libc`
- `nix`
- `zeroize`

## Validation

Run what is available:

```bash
cargo fmt --check
cargo test
cargo build
./target/debug/mwb-client --help
git diff --check
```

Final report must list changed files, commands run, and remaining parity gaps against the C++ bridge.
