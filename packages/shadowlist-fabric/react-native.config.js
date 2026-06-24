module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: [
          'ShadowListViewComponentDescriptor',
          'ShadowListElementViewComponentDescriptor',
          'ShadowListTemplateViewComponentDescriptor',
        ],
        cmakeListsPath: '../android/shadowlist/jni/CMakeLists.txt',
      },
    },
  },
};
