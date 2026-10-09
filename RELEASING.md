# Releasing

The tag `vX.Y.Z` must equal `version` in `packages/shadowlist-fabric/package.json` and `VERSION_NAME` in
`packages/shadowlist-android/ShadowListKit/gradle.properties`.

1. Bump both versions and add a `CHANGELOG.md` entry.
2. `yarn release:fabric`
3. `git tag vX.Y.Z && git push origin vX.Y.Z`

The tag runs `.github/workflows/release.yml`. Steps whose secrets are missing are skipped.

## Secrets

- `MAVEN_CENTRAL_USERNAME`, `MAVEN_CENTRAL_PASSWORD`: Central Portal user token for `io.github.azimgd`.
- `SIGNING_KEY`: armored GPG secret key. `SIGNING_KEY_PASSWORD` and `SIGNING_KEY_ID` are optional.
- `COCOAPODS_TRUNK_TOKEN`: the `trunk.cocoapods.org` password from `~/.netrc`.
