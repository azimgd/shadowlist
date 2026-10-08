plugins {
  id("com.android.library")
  id("org.jetbrains.kotlin.android")
}

android {
  namespace = "com.shadowlist.kit"
  compileSdk = 36
  ndkVersion = "27.1.12297006"

  defaultConfig {
    minSdk = 24
    externalNativeBuild {
      cmake {
        arguments += listOf("-DANDROID_STL=c++_shared")
      }
    }
  }

  externalNativeBuild {
    cmake {
      path = file("src/main/cpp/CMakeLists.txt")
      version = "3.22.1"
    }
  }

  compileOptions {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
  }
}

kotlin {
  jvmToolchain(17)
}

dependencies {
  // NestedScrollingChild3 and its helper, for lists inside CoordinatorLayout.
  implementation("androidx.core:core:1.13.1")
}
