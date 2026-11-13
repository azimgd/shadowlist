import com.vanniktech.maven.publish.AndroidSingleVariantLibrary

plugins {
  id("com.android.library")
  id("org.jetbrains.kotlin.android")
  id("com.vanniktech.maven.publish")
}

android {
  namespace = "com.shadowlist.utils"
  compileSdk = 36
  ndkVersion = "27.1.12297006"

  defaultConfig {
    minSdk = 24
    testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
    externalNativeBuild {
      cmake {
        /*
         * The C++ runtime links statically. The AAR ships one .so and works next to libraries that
         * bring their own libc++_shared.so. Flexible page sizes align it for 16 KB page devices.
         */
        arguments += listOf("-DANDROID_STL=c++_static", "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON")
      }
    }
  }

  externalNativeBuild {
    cmake {
      path = file("src/main/cpp/CMakeLists.txt")
      version = "3.22.1"
    }
  }

  sourceSets {
    // The tests replay the vector files the core tests and the TypeScript tests share.
    getByName("androidTest").assets.srcDir("../../shadowlist-core-tests/vectors")
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
  // api because the template lists subclass ShadowListKitListView publicly.
  api("io.github.azimgd:shadowlist-kit:${property("VERSION_NAME")}")
  // The tests run on a device or an emulator. They load the native core.
  androidTestImplementation("androidx.test:runner:1.6.2")
  androidTestImplementation("androidx.test.ext:junit:1.2.1")
}

/*
 * JitPack publishes under its own group with the tag as the version.
 */
val jitpack = System.getenv("JITPACK") == "true"

mavenPublishing {
  configure(AndroidSingleVariantLibrary(variant = "release", sourcesJar = true, publishJavadocJar = true))
  if (jitpack) {
    coordinates(
      groupId = "${System.getenv("GROUP") ?: "com.github.azimgd"}.${System.getenv("ARTIFACT") ?: "shadowlist"}",
      artifactId = property("POM_ARTIFACT_ID").toString(),
      version = System.getenv("VERSION") ?: property("VERSION_NAME").toString(),
    )
  }
  publishToMavenCentral()
  // Signing needs a key. Without one, publishToMavenLocal still works and Maven Central refuses the upload.
  val signingKey = providers.gradleProperty("signingInMemoryKey").orNull ?: providers.gradleProperty("signing.keyId").orNull
  if (!signingKey.isNullOrBlank()) {
    signAllPublications()
  }
}
