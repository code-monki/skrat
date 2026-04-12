# Releases

This folder documents how **versioned binaries** are published. The repository does **not** store large binaries in Git; downloads live on **GitHub**.

## Where to download

| Channel | URL | When to use it |
|---------|-----|----------------|
| **GitHub Releases** | [github.com/code-monki/skrat/releases](https://github.com/code-monki/skrat/releases) | Tagged versions (`v1.2.3`, …): each release lists **`.tar.gz`** / **`.zip`** assets per platform. |
| **CI Artifacts** | [Actions → CI workflow](https://github.com/code-monki/skrat/actions/workflows/ci.yml) | Latest build from **`master`** / **`main`** or a branch/PR run (artifacts expire per GitHub’s retention policy). |

## Cutting a new release

1. Ensure **`master`** (or **`main`**) is green in Actions.
2. Create and push an annotated tag whose name starts with **`v`** (Semantic Versioning is recommended), for example:
   ```bash
   git tag -a v0.2.0 -m "Release v0.2.0"
   git push origin v0.2.0
   ```
3. The **CI** workflow runs for that tag, builds all platforms, then creates or updates a **GitHub Release** for that tag and uploads the packaged assets (`skrat-*.tar.gz` / `skrat-*.zip`).

Release assets are the same **Qt-linked** executables described in the root **README** (not redistributable installers unless you bundle Qt yourself).

## Forks

Replace `code-monki/skrat` in the URLs above with your GitHub owner and repository name.
