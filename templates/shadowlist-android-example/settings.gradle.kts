pluginManagement {
  repositories {
    google()
    mavenCentral()
    gradlePluginPortal()
  }
}

dependencyResolutionManagement {
  repositories {
    google()
    mavenCentral()
  }
}

rootProject.name = "shadowlist-android-example"
include(":ShadowListKit")
project(":ShadowListKit").projectDir = file("../../packages/shadowlist-android/ShadowListKit")
include(":ShadowListUtils")
project(":ShadowListUtils").projectDir = file("../../packages/shadowlist-android-utils/ShadowListUtils")
include(":app")

/*
 * ShadowListUtils depends on the kit by its published coordinates. Inside this build they
 * resolve to the kit project beside it.
 */
gradle.beforeProject {
  configurations.configureEach {
    resolutionStrategy.dependencySubstitution {
      substitute(module("io.github.azimgd:shadowlist-kit")).using(project(":ShadowListKit"))
    }
  }
}
