# Releases

This folder documents how **versioned binaries** are published. The repository does **not** store large binaries in Git; downloads live on **GitHub**.

## Where to download

| Channel | URL | When to use it |
|---------|-----|----------------|
| **GitHub Releases** | [github.com/code-monki/skrat/releases](https://github.com/code-monki/skrat/releases) | Tagged versions (`v1.2.3`, …): each release lists **`.tar.gz`** / **`.zip`** assets per platform and a macOS **`.dmg`** drag-install image. |
| **CI Artifacts** | [Actions → CI workflow](https://github.com/code-monki/skrat/actions/workflows/ci.yml) | Latest build from **`master`** / **`main`** or a branch/PR run (artifacts expire per GitHub’s retention policy). |

## Cutting a new release

1. Ensure **`master`** (or **`main`**) is green in Actions.
   - If any matrix job fails, fix on `master` and cut a new tag (for example `v0.1.8`) rather than reusing the failed tag.
2. Create and push an annotated tag whose name starts with **`v`** (Semantic Versioning is recommended), for example:
   ```bash
   git tag -a v0.2.0 -m "Release v0.2.0"
   git push origin v0.2.0
   ```
3. The **CI** workflow runs for that tag, builds all platforms, then creates or updates a **GitHub Release** for that tag and uploads the packaged assets (`skrat-*.tar.gz` / `skrat-*.zip` / `skrat-*.dmg`).

Release assets include **portable** archives (names ending in **`-portable`**) where CI runs **`windeployqt`**, **`macdeployqt`**, or **linuxdeploy** so end users do **not** need Qt installed. Raw **`skrat` / `skrat.exe`** files may also be listed for debugging; those still expect Qt on the system. See the root **README** CI section for how to run each bundle. **macOS:** unsigned CI builds are blocked by **Gatekeeper** until you use **Finder → right‑click → Open** once (or clear quarantine); see **README → Run → macOS Gatekeeper**.

## Forks

Replace `code-monki/skrat` in the URLs above with your GitHub owner and repository name.
