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

rootProject.name = "shadowlist-android-utils"

/*
 * ShadowListKit builds from the kit's folder beside this one. Its coordinates resolve to that
 * project instead of Maven Central.
 */
includeBuild("../shadowlist-android") {
  dependencySubstitution {
    substitute(module("io.github.azimgd:shadowlist-kit")).using(project(":ShadowListKit"))
  }
}

include(":ShadowListUtils")
