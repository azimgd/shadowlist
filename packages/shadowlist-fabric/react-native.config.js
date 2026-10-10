module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: [
          'ShadowListViewComponentDescriptor',
          'ShadowListCellViewComponentDescriptor',
          'ShadowListTemplateViewComponentDescriptor',
        ],
        cmakeListsPath: '../android/shadowlist/jni/CMakeLists.txt',
      },
    },
  },
};
