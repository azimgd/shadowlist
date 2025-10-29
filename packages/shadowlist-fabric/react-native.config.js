module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: [
          'ShadowlistElementViewComponentDescriptor',
          'ShadowlistViewComponentDescriptor',
        ],
        cmakeListsPath: '../android/shadowlist/jni/CMakeLists.txt',
      },
    },
  },
};
