import com.vanniktech.maven.publish.AndroidSingleVariantLibrary

plugins {
  id("com.android.library")
  id("org.jetbrains.kotlin.android")
  id("com.vanniktech.maven.publish")
}

android {
  namespace = "com.shadowlist.kit"
  compileSdk = 36
  ndkVersion = "27.1.12297006"

  defaultConfig {
    minSdk = 24
    externalNativeBuild {
      cmake {
        /*
         * The C++ runtime links statically. The AAR ships one .so and works next to libraries that
         * bring their own libc++_shared.so. Flexible page sizes align it for 16 KB page devices.
         */
        arguments += listOf("-DANDROID_STL=c++_static", "-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON")
        // -PshadowlistDebugLog compiles in the core's [SL] debug log. It prints on every pass.
        if (project.hasProperty("shadowlistDebugLog")) {
          arguments += listOf("-DSHADOWLIST_DEBUG_LOG=1")
        }
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
  // api because the list view implements NestedScrollingChild3 publicly.
  api("androidx.core:core:1.13.1")
}

// JitPack publishes under its own group with the tag as the version.
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
