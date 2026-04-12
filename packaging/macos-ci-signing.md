# macOS signing & notarization in CI (no secrets in Git)

Your Apple Developer subscription is enough; **credentials must never be committed**. Use **GitHub Actions encrypted repository secrets** (and optionally a protected **Environment** so only release/tag workflows can read them).

## What stays out of Git

| Never commit | Store instead as |
|--------------|------------------|
| `.p12` / `.p8` / private keys | **Actions → Secrets** (or OIDC + external vault, advanced) |
| Certificate export passwords | Secret |
| App Store Connect API key (`.p8` contents) | Secret |
| Apple ID + app-specific password | Prefer **API key** for `notarytool`; legacy password only if you must |

Fork pull requests do **not** receive secrets; only runs from branches in **this** repo (or tags you push) can use them.

## Apple-side setup (one-time)

1. **Developer ID Application** certificate (Xcode or developer.apple.com → Certificates). Install in Keychain, then **export** as `.p12` with a strong password (remember it for a secret).
2. **Notarization:** Create an **App Store Connect API key** (Users and Access → Integrations → App Store Connect API): role that can notarize (e.g. **Developer** or **App Manager**). Download the **`.p8`** once; you cannot download it again.

## Suggested GitHub secrets

Use these **names** only as a convention; wire the same names if you add a signing job to CI later.

| Secret | Contents |
|--------|----------|
| `MACOS_CERTIFICATE_P12` | Base64-encoded **entire** `.p12` file (see below) |
| `MACOS_CERTIFICATE_PASSWORD` | Password you set when exporting the `.p12` |
| `MACOS_CODESIGN_IDENTITY` | Full identity string, e.g. `Developer ID Application: Your Name (TEAMID)` |
| `APPLE_API_KEY_ID` | 10-character Key ID from App Store Connect |
| `APPLE_API_ISSUER_ID` | Issuer UUID from App Store Connect |
| `APPLE_API_KEY_P8` | **Full text** of the `.p8` file (including `BEGIN`/`END` lines) |

Encode the certificate for a secret:

```bash
base64 -i DeveloperID_application.p12 | pbcopy   # paste into MACOS_CERTIFICATE_P12
```

## Hardening (optional)

- **Environments:** Repository → Settings → Environments → e.g. `apple-release` → add **required reviewers** or **deployment branches** (`refs/tags/v*`), then attach these secrets to that environment so random branch workflows cannot read them.
- **Team ID / bundle ID** stay in `CMakeLists.txt` as **non-secret** identifiers (`MACOSX_BUNDLE_GUI_IDENTIFIER`, etc.).

## What CI would do (when you add a job)

High level only—this repo’s workflow does **not** run these steps until you add them:

1. Create a temporary keychain, import the `.p12`, allow `codesign` to use the key without UI prompts.
2. **`codesign --deep --force --options runtime --sign "$IDENTITY"`** on `skrat.app` (and often every Mach-O inside after `macdeployqt`).
3. **`ditto -c -k --keepParent skrat.app skrat.zip`**, then **`xcrun notarytool submit skrat.zip --key <path> --key-id … --issuer … --wait`**, then **`xcrun stapler staple skrat.app`**.
4. Re-pack the tarball for Releases.

Official references: [Notarizing macOS software before distribution](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution), `man notarytool`, `man codesign`.

## Local alternative

Sign and notarize **on your Mac** with your own keychain, then upload artifacts manually—no GitHub secrets required.
