module.exports = {
  dependency: {
    platforms: {
      android: {
        componentDescriptors: [
          'ShadowListContainerComponentDescriptor',
          'ShadowListItemComponentDescriptor',
        ],
        cmakeListsPath: '../android/jni/CMakeLists.txt',
      },
    },
  },
};
