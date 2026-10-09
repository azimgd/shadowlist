# Releasing

One tag releases everything at one version: `vX.Y.Z`, equal to `version` in
`packages/shadowlist-fabric/package.json` and `VERSION_NAME` in
`packages/shadowlist-android/ShadowListKit/gradle.properties`.

1. Bump both versions and add a `CHANGELOG.md` entry.
2. Publish the npm package with `yarn release:fabric`.
3. Push the tag: `git tag vX.Y.Z && git push origin vX.Y.Z`.

The tag runs `.github/workflows/release.yml`. It checks the versions match the tag, builds the Swift package,
lints the podspec, pushes it to CocoaPods trunk and publishes the Android kit to Maven Central. A step whose
secrets are missing is skipped with a notice. SwiftPM and JitPack need no publish step: they build from the tag.

## One-time setup

- Maven Central: sign in at https://central.sonatype.com with the GitHub account `azimgd`. The namespace
  `io.github.azimgd` verifies through GitHub. Generate a user token under Account.
- GPG: create a signing key (`gpg --full-generate-key`, RSA 4096) and upload the public key with
  `gpg --keyserver keyserver.ubuntu.com --send-keys <KEY_ID>`. Export the secret key for CI with
  `gpg --armor --export-secret-keys <KEY_ID>`.
- GitHub secrets (Settings, Secrets and variables, Actions):
  - `MAVEN_CENTRAL_USERNAME`, `MAVEN_CENTRAL_PASSWORD`: the Central Portal user token.
  - `SIGNING_KEY`: the armored secret key. `SIGNING_KEY_PASSWORD`: its passphrase. `SIGNING_KEY_ID`: optional.
  - `COCOAPODS_TRUNK_TOKEN`: after `pod trunk register me@azimgd.com 'azimgd'` and the email confirmation,
    the token from `~/.netrc` (the `trunk.cocoapods.org` password).
- JitPack: open https://jitpack.io/#azimgd/shadowlist after the first tag and press Get it to start the
  first build. Later tags build on first request.

A local `./gradlew :ShadowListKit:publishToMavenLocal` in `packages/shadowlist-android` needs no keys.
