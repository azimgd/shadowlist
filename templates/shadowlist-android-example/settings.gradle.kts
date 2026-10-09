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
include(":app")
