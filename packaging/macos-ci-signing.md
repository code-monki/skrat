# macOS signing & notarization in CI (no secrets in Git)

**Credentials stay in GitHub Actions → Settings → Secrets and variables → Actions** (never in the repo). Fork pull requests do **not** receive secrets.

## Secrets wired in CI (`.github/workflows/ci.yml`)

The **Sign and notarize (Developer ID)** step runs only when **all** of these repository secrets are non-empty:

| Secret | Contents |
|--------|----------|
| `MACOS_CERTIFICATE` | Base64-encoded **entire** `.p12` (Developer ID Application + private key) |
| `MACOS_CERTIFICATE_PWD` | `.p12` export password |
| `MACOS_CODESIGN_IDENTITY` | Full string from Keychain, e.g. `Developer ID Application: Your Name (TEAMID)` |
| `AC_API_KEY_ID` | App Store Connect API key id (10 characters) |
| `AC_API_ISSUER_ID` | Issuer UUID from App Store Connect |
| `AC_API_KEY_CONTENT` | Full **`.p8`** PEM text (including `BEGIN`/`END` lines) |

Encode the certificate for `MACOS_CERTIFICATE`:

```bash
base64 -i DeveloperID_application.p12 | pbcopy
```

If any secret is missing, the macOS job still builds and packs an **unsigned** app (same as before).

## What CI does

1. After **`macdeployqt`**, create a temporary keychain, import the `.p12`, set the key partition list so **`codesign`** is non-interactive.
2. **`codesign --force --deep --timestamp --options runtime`** with **`packaging/macos/Notarization.entitlements`** on **`build/skrat.app`**.
3. **`ditto -c -k --keepParent`** → **`xcrun notarytool submit … --wait`** → **`xcrun stapler staple`**.
4. Pack **`skrat-macos-universal-portable.tar.gz`** as before.

## If notarization or Gatekeeper still complains

Apple may reject hardened-runtime bundles that load unsigned code inside Qt frameworks. In that case extend **`packaging/macos/Notarization.entitlements`** (e.g. consult Qt / Apple docs for **`com.apple.security.cs.disable-library-validation`**) and re-run CI—do **not** commit secrets.

## Hardening (optional)

- **Environments:** attach the same secrets to an **Environment** (e.g. `apple-release`) with branch rules (`refs/tags/v*`) or required reviewers.
- **Bundle ID** remains in **`CMakeLists.txt`** as non-secret metadata.

Official references: [Notarizing macOS software before distribution](https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution), `man notarytool`, `man codesign`.

## Local alternative

Sign and notarize on your Mac with your own keychain, then upload artifacts manually.
