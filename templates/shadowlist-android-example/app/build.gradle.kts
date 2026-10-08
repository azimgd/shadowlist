plugins {
  id("com.android.application")
  id("org.jetbrains.kotlin.android")
}

android {
  namespace = "com.shadowlist.kit.example"
  compileSdk = 36
  ndkVersion = "27.1.12297006"

  defaultConfig {
    applicationId = "shadowlist.android.example"
    minSdk = 24
    targetSdk = 36
    versionCode = 1
    versionName = "1.0"
    ndk {
      abiFilters += listOf("arm64-v8a")
    }
  }

  buildTypes {
    release {
      isMinifyEnabled = false
      // Signed with the debug key to install without a keystore. Still a non-debuggable, optimized build.
      signingConfig = signingConfigs.getByName("debug")
    }
  }

  // The bench lives next to ShadowListKit in the shadowlist repo and compiles into the example.
  sourceSets["main"].java.srcDir(project(":ShadowListKit").projectDir.resolve("../Bench/src"))

  compileOptions {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
  }
}

kotlin {
  jvmToolchain(17)
}

dependencies {
  implementation(project(":ShadowListKit"))
  implementation("androidx.recyclerview:recyclerview:1.3.2")
  // CoordinatorLayout and AppBarLayout for the nested scrolling screen.
  implementation("com.google.android.material:material:1.12.0")
}
