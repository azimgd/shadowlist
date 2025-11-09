module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: [
          'ShadowlistViewComponentDescriptor',
          'ShadowlistElementViewComponentDescriptor',
          'ShadowlistTemplateViewComponentDescriptor',
        ],
        cmakeListsPath: '../android/shadowlist/jni/CMakeLists.txt',
      },
    },
  },
};
